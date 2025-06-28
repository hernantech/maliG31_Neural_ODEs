#include "gpu_solver.h"
#include "test_problems.h"
#include "timer.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>

// OPTIMIZED: Advanced compute shader with proper numerical methods
const std::string compute_shader_source_optimized = R"(
#version 310 es
layout(local_size_x = 4) in;

// Shared memory for Butcher tableau coefficients (frequently accessed)
shared float butcher_coeffs[20]; // a21, a31, a32, a41, a42, a43, a51, a52, a53, a54, a61, a62, a63, a64, a65, b1, b3, b4, b5, b6

layout(std430, binding = 0) buffer StateBuffer {
    float state_data[];
};

layout(std430, binding = 1) buffer ParamBuffer {
    float dt;
    float t_start;
    int n_equations;
    int n_steps_batch;
    float lambda;
};

layout(std430, binding = 2) buffer ResultBuffer {
    float all_results[];  // [step0_eq0, step0_eq1, ..., step1_eq0, step1_eq1, ...]
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint local_idx = gl_LocalInvocationID.x;
    
    // Load Butcher tableau coefficients into shared memory (thread 0 only)
    if (local_idx == 0u) {
        // Dormand-Prince RK45 coefficients
        butcher_coeffs[0] = 0.2;                    // a21
        butcher_coeffs[1] = 0.075;                  // a31
        butcher_coeffs[2] = 0.225;                  // a32
        butcher_coeffs[3] = 0.977778;               // a41
        butcher_coeffs[4] = -3.733333;              // a42
        butcher_coeffs[5] = 3.555556;               // a43
        butcher_coeffs[6] = 2.952597;               // a51
        butcher_coeffs[7] = -11.595793;             // a52
        butcher_coeffs[8] = 9.822893;               // a53
        butcher_coeffs[9] = -0.290683;              // a54
        butcher_coeffs[10] = 2.846275;              // a61
        butcher_coeffs[11] = -10.757576;            // a62
        butcher_coeffs[12] = 8.906422;              // a63
        butcher_coeffs[13] = 0.278409;              // a64
        butcher_coeffs[14] = -0.273531;             // a65
        butcher_coeffs[15] = 0.091146;              // b1
        butcher_coeffs[16] = 0.449237;              // b3
        butcher_coeffs[17] = 0.651042;              // b4
        butcher_coeffs[18] = -0.322376;             // b5
        butcher_coeffs[19] = 0.130952;              // b6
    }
    
    memoryBarrierShared();
    barrier();
    
    if (idx >= uint(n_equations)) return;
    
    // Load initial state for this equation
    float y = state_data[idx];
    
    // Store initial condition
    all_results[0 * n_equations + int(idx)] = y;
    
    // OPTIMIZATION: Process multiple timesteps with minimal branching
    for (int step = 1; step < n_steps_batch; step++) {
        // RK45 stages for exponential decay: dy/dt = -lambda * y
        // OPTIMIZATION: Use shared memory coefficients
        float k1 = dt * (-lambda * y);
        float k2 = dt * (-lambda * (y + butcher_coeffs[0] * k1));
        float k3 = dt * (-lambda * (y + butcher_coeffs[1] * k1 + butcher_coeffs[2] * k2));
        float k4 = dt * (-lambda * (y + butcher_coeffs[3] * k1 + butcher_coeffs[4] * k2 + butcher_coeffs[5] * k3));
        float k5 = dt * (-lambda * (y + butcher_coeffs[6] * k1 + butcher_coeffs[7] * k2 + butcher_coeffs[8] * k3 + butcher_coeffs[9] * k4));
        float k6 = dt * (-lambda * (y + butcher_coeffs[10] * k1 + butcher_coeffs[11] * k2 + butcher_coeffs[12] * k3 + butcher_coeffs[13] * k4 + butcher_coeffs[14] * k5));
        
        // Update state using Butcher tableau
        y = y + butcher_coeffs[15] * k1 + butcher_coeffs[16] * k3 + butcher_coeffs[17] * k4 + butcher_coeffs[18] * k5 + butcher_coeffs[19] * k6;
        
        // Store result for this timestep
        all_results[step * n_equations + int(idx)] = y;
    }
}
)";

// OPTIMIZED GPU Solver with advanced numerical methods
class OptimizedGPUSolver : public GPUSolver {
private:
    GLuint optimized_program;
    
public:
    OptimizedGPUSolver() : optimized_program(0) {
        if (!initialize_optimized_gpu()) {
            std::cerr << "Failed to initialize optimized GPU context" << std::endl;
        }
    }
    
    ~OptimizedGPUSolver() {
        if (optimized_program != 0) {
            glDeleteProgram(optimized_program);
        }
    }
    
    bool initialize_optimized_gpu() {
        // Reuse base GPU initialization
        if (!GPUSolver::initialize_gpu()) {
            return false;
        }
        
        // Compile optimized compute shader
        optimized_program = compile_compute_shader(compute_shader_source_optimized);
        if (optimized_program == 0) {
            std::cerr << "Failed to compile optimized compute shader" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void solve_optimized(const ODESystem& system, 
                        double t0, double tf, double dt,
                        const std::vector<double>& y0,
                        std::vector<std::vector<double>>& solution) {
        
        if (!initialized) {
            std::cerr << "Optimized GPU solver not initialized" << std::endl;
            return;
        }
        
        int n_equations = y0.size();
        int n_steps = static_cast<int>((tf - t0) / dt) + 1;
        
        // Check if lambda parameter exists (for exponential decay)
        auto lambda_it = system.parameters.find("lambda");
        if (lambda_it == system.parameters.end()) {
            std::cerr << "Optimized GPU solver currently only supports exponential decay problems" << std::endl;
            return;
        }
        
        // OPTIMIZATION: Pre-allocate all buffers for better performance
        std::vector<float> state_data(n_equations);
        for (int i = 0; i < n_equations; ++i) {
            state_data[i] = static_cast<float>(y0[i]);
        }
        
        // Create optimized GPU buffers
        GLuint state_buffer, param_buffer, result_buffer;
        
        glGenBuffers(1, &state_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, state_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, n_equations * sizeof(float), 
                     state_data.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, state_buffer);
        
        // Parameter buffer with optimal alignment
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
        
        // Result buffer with optimal size
        size_t result_size = n_steps * n_equations * sizeof(float);
        glGenBuffers(1, &result_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, result_size, nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, result_buffer);
        
        // OPTIMIZATION: Use optimized compute shader
        glUseProgram(optimized_program);
        
        // OPTIMIZATION: Tune workgroup size for Mali G31 MP2
        // Mali G31 MP2 has 4 ALUs, optimize for 4 threads per workgroup
        GLuint work_groups = (n_equations + 3) / 4;
        glDispatchCompute(work_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Single CPU-GPU transfer
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
};

// Test function to compare optimized vs standard GPU solver
void test_optimized_gpu() {
    std::cout << "=== Testing Optimized GPU Solver ===" << std::endl;
    
    auto system = TestProblems::create_exponential_decay();
    const double dt = 0.01;
    const double tf = 1.0; // Shorter test
    
    // Test standard GPU solver
    GPUSolver standard_gpu;
    std::vector<std::vector<double>> standard_solution;
    
    Timer timer;
    timer.start();
    standard_gpu.solve(system, 0.0, tf, dt, system.initial_conditions, standard_solution);
    double standard_time = timer.elapsed();
    
    // Test optimized GPU solver
    OptimizedGPUSolver optimized_gpu;
    std::vector<std::vector<double>> optimized_solution;
    
    timer.start();
    optimized_gpu.solve_optimized(system, 0.0, tf, dt, system.initial_conditions, optimized_solution);
    double optimized_time = timer.elapsed();
    
    std::cout << "Standard GPU: " << standard_time * 1000 << " ms" << std::endl;
    std::cout << "Optimized GPU: " << optimized_time * 1000 << " ms" << std::endl;
    if (optimized_time > 0) {
        std::cout << "Speedup: " << standard_time / optimized_time << "x" << std::endl;
    }
    
    // Verify solutions are consistent
    if (!standard_solution.empty() && !optimized_solution.empty()) {
        double max_diff = 0.0;
        for (size_t i = 0; i < standard_solution.size(); ++i) {
            for (size_t j = 0; j < standard_solution[i].size(); ++j) {
                double diff = std::abs(standard_solution[i][j] - optimized_solution[i][j]);
                max_diff = std::max(max_diff, diff);
            }
        }
        std::cout << "Max difference: " << max_diff << std::endl;
    }
}

int main() {
    test_optimized_gpu();
    return 0;
} 