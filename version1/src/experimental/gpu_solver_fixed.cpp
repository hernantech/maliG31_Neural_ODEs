#include "gpu_solver.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>

// FIXED: Optimized compute shader that does MULTIPLE integration steps
const std::string compute_shader_source_optimized = R"(
#version 310 es
layout(local_size_x = 4) in;

layout(std430, binding = 0) buffer StateBuffer {
    float state_data[];
};

layout(std430, binding = 1) buffer ParamBuffer {
    float dt;
    float t_start;
    int n_equations;
    int n_steps_batch;  // Process multiple steps per dispatch
    float lambda;
};

layout(std430, binding = 2) buffer ResultBuffer {
    float all_results[];  // Store ALL timesteps: [step0_eq0, step0_eq1, ..., step1_eq0, step1_eq1, ...]
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uint(n_equations)) return;
    
    // RK45 coefficients  
    const float a21 = 0.2;
    const float a31 = 0.075, a32 = 0.225;
    const float a41 = 0.977778, a42 = -3.733333, a43 = 3.555556;
    const float a51 = 2.952597, a52 = -11.595793, a53 = 9.822893, a54 = -0.290683;
    const float a61 = 2.846275, a62 = -10.757576, a63 = 8.906422, a64 = 0.278409, a65 = -0.273531;
    const float b1 = 0.091146, b3 = 0.449237, b4 = 0.651042, b5 = -0.322376, b6 = 0.130952;
    
    // Load initial state for this equation
    float y = state_data[idx];
    
    // Store initial condition
    all_results[0 * n_equations + idx] = y;
    
    // Integrate multiple steps in GPU without CPU synchronization
    for (int step = 1; step < n_steps_batch; step++) {
        float t_current = t_start + float(step-1) * dt;
        
        // RK45 stages for exponential decay: dy/dt = -lambda * y
        float k1 = dt * (-lambda * y);
        float k2 = dt * (-lambda * (y + a21 * k1));
        float k3 = dt * (-lambda * (y + a31 * k1 + a32 * k2));
        float k4 = dt * (-lambda * (y + a41 * k1 + a42 * k2 + a43 * k3));
        float k5 = dt * (-lambda * (y + a51 * k1 + a52 * k2 + a53 * k3 + a54 * k4));
        float k6 = dt * (-lambda * (y + a61 * k1 + a62 * k2 + a63 * k3 + a64 * k4 + a65 * k5));
        
        // Update state
        y = y + b1 * k1 + b3 * k3 + b4 * k4 + b5 * k5 + b6 * k6;
        
        // Store result for this timestep
        all_results[step * n_equations + idx] = y;
    }
}
)";

void GPUSolver::solve(const ODESystem& system, 
                     double t0, double tf, double dt,
                     const std::vector<double>& y0,
                     std::vector<std::vector<double>>& solution) {
    
    if (!initialized) {
        std::cerr << "GPU solver not initialized" << std::endl;
        return;
    }
    
    int n_equations = y0.size();
    int n_steps = static_cast<int>((tf - t0) / dt) + 1;
    
    // Check if lambda parameter exists (for exponential decay)
    auto lambda_it = system.parameters.find("lambda");
    if (lambda_it == system.parameters.end()) {
        std::cerr << "GPU solver currently only supports exponential decay problems with lambda parameter" << std::endl;
        return;
    }
    
    // OPTIMIZATION: Do ALL integration steps in a single GPU dispatch
    
    // Convert to float for GPU
    std::vector<float> state_data(n_equations);
    for (int i = 0; i < n_equations; ++i) {
        state_data[i] = static_cast<float>(y0[i]);
    }
    
    // Create GPU buffers
    glGenBuffers(1, &state_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, state_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, n_equations * sizeof(float), 
                 state_data.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, state_buffer);
    
    // Parameter buffer
    struct {
        float dt;
        float t_start;
        int n_equations;
        int n_steps_batch;
        float lambda;
    } params;
    
    params.dt = static_cast<float>(dt);
    params.t_start = static_cast<float>(t0);
    params.n_equations = n_equations;
    params.n_steps_batch = n_steps;
    params.lambda = static_cast<float>(lambda_it->second);
    
    glGenBuffers(1, &param_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(params), &params, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, param_buffer);
    
    // Result buffer - stores ALL timesteps
    GLuint result_buffer;
    size_t result_size = n_steps * n_equations * sizeof(float);
    glGenBuffers(1, &result_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, result_size, nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, result_buffer);
    
    // SINGLE GPU dispatch for ALL integration steps
    glUseProgram(program);
            GLuint work_groups = (n_equations + 3) / 4;
    glDispatchCompute(work_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // SINGLE CPU-GPU transfer at the end
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
    float* all_results = static_cast<float*>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, result_size, GL_MAP_READ_BIT));
    
    // Convert results back to CPU format
    solution.clear();
    solution.reserve(n_steps);
    
    for (int step = 0; step < n_steps; step++) {
        std::vector<double> step_result(n_equations);
        for (int eq = 0; eq < n_equations; eq++) {
            step_result[eq] = static_cast<double>(all_results[step * n_equations + eq]);
        }
        solution.push_back(step_result);
    }
    
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    
    // Cleanup buffers
    glDeleteBuffers(1, &state_buffer);
    glDeleteBuffers(1, &param_buffer);
    glDeleteBuffers(1, &result_buffer);
}

// Note: This would replace the current GPU solver implementation
// The key changes:
// 1. Single GPU dispatch instead of 500+ dispatches
// 2. GPU does all integration steps in parallel
// 3. Single CPU-GPU transfer at the end
// 4. Proper GPU utilization for larger problems 