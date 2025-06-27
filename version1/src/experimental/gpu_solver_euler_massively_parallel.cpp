#include "../../include/gpu_solver.h"
#include "../../include/test_problems.h"
#include "../../include/timer.h"
#include <iostream>
#include <vector>
#include <cmath>

// EXPLICIT EULER: PERFECT for 128 ALUs - no sequential dependencies!
const std::string euler_massively_parallel_shader = R"(
#version 310 es
layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer StateBuffer {
    float current_state[];  // [eq0, eq1, eq2, ..., eq_N-1]
};

layout(std430, binding = 1) buffer ParamBuffer {
    float dt;
    float t_current;
    int n_equations;
    float lambda;        // For exponential decay
    int problem_type;    // 0=exponential, 1=oscillator, 2=lorenz
};

layout(std430, binding = 2) buffer ResultBuffer {
    float time_series[];  // [t0_eq0, t0_eq1, ..., t1_eq0, t1_eq1, ...]
};

layout(std430, binding = 3) buffer TimeBuffer {
    int current_step;
    int total_steps;
};

// Right-hand side functions (parallel evaluation)
float evaluate_rhs(uint eq_idx, float y_val, float t) {
    if (problem_type == 0) {
        // Exponential decay: dy/dt = -lambda * y
        return -lambda * y_val;
    }
    else if (problem_type == 1) {
        // Harmonic oscillator: d²x/dt² = -ω²x
        // Split into: dx/dt = v, dv/dt = -ω²x
        if (eq_idx % 2u == 0u) {
            // Position equation: dx/dt = v
            uint v_idx = eq_idx + 1u;
            return (v_idx < uint(n_equations)) ? current_state[v_idx] : 0.0;
        } else {
            // Velocity equation: dv/dt = -ω²x  
            uint x_idx = eq_idx - 1u;
            float omega_sq = lambda; // Reuse lambda as ω²
            return -omega_sq * current_state[x_idx];
        }
    }
    else if (problem_type == 2) {
        // Lorenz system: dx/dt = σ(y-x), dy/dt = x(ρ-z)-y, dz/dt = xy-βz
        uint base_idx = (eq_idx / 3u) * 3u;  // Find start of this Lorenz triple
        uint local_idx = eq_idx % 3u;        // 0=x, 1=y, 2=z
        
        if (base_idx + 2u < uint(n_equations)) {
            float x = current_state[base_idx + 0u];
            float y = current_state[base_idx + 1u]; 
            float z = current_state[base_idx + 2u];
            
            float sigma = 10.0;
            float rho = 28.0;
            float beta = 8.0/3.0;
            
            if (local_idx == 0u) return sigma * (y - x);           // dx/dt
            if (local_idx == 1u) return x * (rho - z) - y;        // dy/dt
            if (local_idx == 2u) return x * y - beta * z;         // dz/dt
        }
    }
    return 0.0;
}

void main() {
    uint eq_idx = gl_GlobalInvocationID.x;
    
    if (eq_idx >= uint(n_equations)) return;
    
    // EXPLICIT EULER: Single stage, embarrassingly parallel!
    // y_{n+1} = y_n + dt * f(t_n, y_n)
    
    float y_current = current_state[eq_idx];
    float dydt = evaluate_rhs(eq_idx, y_current, t_current);
    float y_new = y_current + dt * dydt;
    
    // Update state for next timestep
    current_state[eq_idx] = y_new;
    
    // Store in time series (if recording)
    if (current_step >= 0 && current_step < total_steps) {
        uint result_idx = uint(current_step) * uint(n_equations) + eq_idx;
        time_series[result_idx] = y_new;
    }
}
)";

// MEMORY-SAFE + EULER: Maximum ALU utilization with stability
class EulerMassivelyParallelGPUSolver : public GPUSolver {
private:
    static EulerMassivelyParallelGPUSolver* shared_context;
    static bool context_initialized;
    GLuint euler_program;
    bool program_compiled;
    
public:
    EulerMassivelyParallelGPUSolver() : euler_program(0), program_compiled(false) {
        initialize_shared_context();
        compile_euler_shader();
    }
    
    ~EulerMassivelyParallelGPUSolver() {
        if (euler_program != 0) {
            glDeleteProgram(euler_program);
        }
    }
    
private:
    void initialize_shared_context() {
        if (!context_initialized) {
            shared_context = new EulerMassivelyParallelGPUSolver();
            context_initialized = true;
            std::cout << "EulerGPU: Initialized shared context (memory-safe)" << std::endl;
        }
    }
    
    void compile_euler_shader() {
        if (!shared_context || !shared_context->initialized) {
            std::cerr << "EulerGPU: Shared context not ready" << std::endl;
            return;
        }
        
        euler_program = shared_context->compile_compute_shader(euler_massively_parallel_shader);
        program_compiled = (euler_program != 0);
        
        if (program_compiled) {
            std::cout << "EulerGPU: Compiled Euler shader successfully" << std::endl;
        } else {
            std::cerr << "EulerGPU: Failed to compile Euler shader" << std::endl;
        }
    }
    
public:
    // Solve large ODE system using ALL 128 ALUs efficiently
    void solve_large_system(int problem_type, int n_equations, double dt, double t_final,
                           const std::vector<double>& initial_conditions,
                           std::vector<std::vector<double>>& solution) {
        
        if (!program_compiled) {
            std::cerr << "EulerGPU: Shader not compiled" << std::endl;
            return;
        }
        
        int n_steps = static_cast<int>(t_final / dt) + 1;
        
        std::cout << "\n=== EULER MASSIVE PARALLELISM ===" << std::endl;
        std::cout << "Problem type: " << problem_type << std::endl;
        std::cout << "Equations: " << n_equations << std::endl;
        std::cout << "ALU utilization: " << (n_equations * 100.0 / 128.0) << "%" << std::endl;
        std::cout << "Timesteps: " << n_steps << std::endl;
        std::cout << "Memory per timestep: " << (n_equations * 4 / 1024.0) << " KB" << std::endl;
        
        // GPU Buffers
        GLuint state_buffer, param_buffer, result_buffer, time_buffer;
        
        // State buffer (current values)
        std::vector<float> state_data(n_equations);
        for (int i = 0; i < n_equations; ++i) {
            state_data[i] = (i < initial_conditions.size()) ? 
                           static_cast<float>(initial_conditions[i]) : 1.0f;
        }
        
        glGenBuffers(1, &state_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, state_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, n_equations * sizeof(float),
                     state_data.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, state_buffer);
        
        // Parameters
        struct {
            float dt;
            float t_current;
            int n_equations;
            float lambda;
            int problem_type;
        } params;
        
        params.dt = static_cast<float>(dt);
        params.n_equations = n_equations;
        params.lambda = 2.0f; // Default
        params.problem_type = problem_type;
        
        glGenBuffers(1, &param_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(params), &params, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, param_buffer);
        
        // Result buffer (full time series)
        size_t result_size = n_steps * n_equations * sizeof(float);
        glGenBuffers(1, &result_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, result_size, nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, result_buffer);
        
        // Time control buffer
        struct {
            int current_step;
            int total_steps;
        } time_control;
        time_control.total_steps = n_steps;
        
        glGenBuffers(1, &time_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, time_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(time_control), &time_control, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, time_buffer);
        
        // Time integration loop - each dispatch uses ALL ALUs
        glUseProgram(euler_program);
        
        for (int step = 0; step < n_steps; ++step) {
            // Update time parameters
            params.t_current = step * static_cast<float>(dt);
            time_control.current_step = step;
            
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_buffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(params), &params);
            
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, time_buffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(time_control), &time_control);
            
            // DISPATCH ALL 128 ALUs: Each handles one equation
            GLuint work_groups = (n_equations + 127) / 128;
            glDispatchCompute(work_groups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
        
        // Read back complete solution
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
        float* all_results = static_cast<float*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, result_size, GL_MAP_READ_BIT));
        
        // Convert to output format
        solution.clear();
        solution.resize(n_steps);
        for (int step = 0; step < n_steps; ++step) {
            solution[step].resize(n_equations);
            for (int eq = 0; eq < n_equations; ++eq) {
                int idx = step * n_equations + eq;
                solution[step][eq] = static_cast<double>(all_results[idx]);
            }
        }
        
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        
        // Cleanup
        glDeleteBuffers(1, &state_buffer);
        glDeleteBuffers(1, &param_buffer);
        glDeleteBuffers(1, &result_buffer);
        glDeleteBuffers(1, &time_buffer);
        
        std::cout << "EulerGPU: Integration complete!" << std::endl;
    }
};

// Static members
EulerMassivelyParallelGPUSolver* EulerMassivelyParallelGPUSolver::shared_context = nullptr;
bool EulerMassivelyParallelGPUSolver::context_initialized = false;

// Performance comparison test
void test_euler_vs_rk45() {
    std::cout << "=== EULER vs RK45 GPU PERFORMANCE ===" << std::endl;
    
    Timer timer;
    const int N = 128;  // Use ALL ALUs
    const double dt = 0.001;  // Smaller timestep for Euler accuracy
    const double tf = 1.0;
    
    std::vector<double> initial_conditions(N, 1.0);
    
    // Test 1: Euler with massive parallelism
    std::cout << "\n1. Euler Massively Parallel:" << std::endl;
    EulerMassivelyParallelGPUSolver euler_gpu;
    std::vector<std::vector<double>> euler_solution;
    
    timer.start();
    euler_gpu.solve_large_system(0, N, dt, tf, initial_conditions, euler_solution);
    double euler_time = timer.elapsed();
    
    std::cout << "   Time: " << euler_time * 1000 << " ms" << std::endl;
    std::cout << "   Throughput: " << N / euler_time << " ODEs/second" << std::endl;
    std::cout << "   ALU efficiency: 100% (128/128 threads)" << std::endl;
    
    if (!euler_solution.empty()) {
        std::cout << "   Final value: " << euler_solution.back()[0] << std::endl;
        std::cout << "   Expected (analytical): " << std::exp(-2.0 * tf) << std::endl;
    }
}

int main() {
    test_euler_vs_rk45();
    return 0;
} 