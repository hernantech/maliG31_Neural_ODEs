#include "gpu_solver.h"
#include "test_problems.h"
#include "timer.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>

// MASSIVELY PARALLEL: Use ALL GPU threads by solving multiple problems
const std::string compute_shader_massively_parallel = R"(
#version 310 es
layout(local_size_x = 4, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer StateBuffer {
    float state_data[];  // [problem0_eq0, problem0_eq1, ..., problem1_eq0, problem1_eq1, ...]
};

layout(std430, binding = 1) buffer ParamBuffer {
    float dt;
    float t_start;
    int n_equations_per_problem;
    int n_problems;  // NEW: Multiple problems in parallel
    int n_steps_batch;
    float lambda;
};

layout(std430, binding = 2) buffer ResultBuffer {
    float all_results[];  // [step0_prob0_eq0, step0_prob0_eq1, step0_prob1_eq0, ..., step1_prob0_eq0, ...]
};

void main() {
    uint global_idx = gl_GlobalInvocationID.x;
    
    // Map global thread ID to problem and equation
    uint problem_id = global_idx / uint(n_equations_per_problem);
    uint equation_id = global_idx % uint(n_equations_per_problem);
    
    if (problem_id >= uint(n_problems)) return;
    
    // RK45 coefficients (constants in registers)
    const float a21 = 0.2;
    const float a31 = 0.075, a32 = 0.225;
    const float a41 = 0.977778, a42 = -3.733333, a43 = 3.555556;
    const float a51 = 2.952597, a52 = -11.595793, a53 = 9.822893, a54 = -0.290683;
    const float a61 = 2.846275, a62 = -10.757576, a63 = 8.906422, a64 = 0.278409, a65 = -0.273531;
    const float b1 = 0.091146, b3 = 0.449237, b4 = 0.651042, b5 = -0.322376, b6 = 0.130952;
    
    // Load initial state for this specific equation of this specific problem
    uint state_idx = problem_id * uint(n_equations_per_problem) + equation_id;
    float y = state_data[state_idx];
    
    // Store initial condition
    uint result_base = 0 * uint(n_problems) * uint(n_equations_per_problem);
    all_results[result_base + state_idx] = y;
    
    // MASSIVE PARALLELISM: Each thread integrates one equation of one problem
    for (int step = 1; step < n_steps_batch; step++) {
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
        result_base = uint(step) * uint(n_problems) * uint(n_equations_per_problem);
        all_results[result_base + state_idx] = y;
    }
}
)";

// MASSIVELY PARALLEL GPU Solver - uses ALL GPU threads
class MassivelyParallelGPUSolver : public GPUSolver {
private:
    GLuint parallel_program;
    
public:
    MassivelyParallelGPUSolver() : parallel_program(0) {
        if (!initialize_parallel_gpu()) {
            std::cerr << "Failed to initialize massively parallel GPU context" << std::endl;
        }
    }
    
    ~MassivelyParallelGPUSolver() {
        if (parallel_program != 0) {
            glDeleteProgram(parallel_program);
        }
    }
    
    bool initialize_parallel_gpu() {
        if (!GPUSolver::initialize_gpu()) {
            return false;
        }
        
        parallel_program = compile_compute_shader(compute_shader_massively_parallel);
        if (parallel_program == 0) {
            std::cerr << "Failed to compile massively parallel compute shader" << std::endl;
            return false;
        }
        
        return true;
    }
    
    // Solve MULTIPLE problems in parallel to use all GPU threads
    void solve_multiple_problems(const ODESystem& base_system, 
                                double t0, double tf, double dt,
                                int n_problems,  // NEW: Number of parallel problems
                                std::vector<std::vector<std::vector<double>>>& all_solutions) {
        
        if (!initialized) {
            std::cerr << "Massively parallel GPU solver not initialized" << std::endl;
            return;
        }
        
        int n_equations_per_problem = base_system.initial_conditions.size();
        int n_steps = static_cast<int>((tf - t0) / dt) + 1;
        int total_equations = n_problems * n_equations_per_problem;
        
        std::cout << "GPU MASSIVE PARALLELISM:" << std::endl;
        std::cout << "- Problems: " << n_problems << std::endl;
        std::cout << "- Equations per problem: " << n_equations_per_problem << std::endl;
        std::cout << "- Total GPU threads: " << total_equations << std::endl;
        std::cout << "- GPU utilization: " << (total_equations * 100.0 / 128) << "%" << std::endl;
        
        // Check lambda parameter
        auto lambda_it = base_system.parameters.find("lambda");
        if (lambda_it == base_system.parameters.end()) {
            std::cerr << "Massively parallel GPU solver only supports exponential decay" << std::endl;
            return;
        }
        
        // Create initial state for ALL problems (slightly different initial conditions)
        std::vector<float> state_data(total_equations);
        for (int prob = 0; prob < n_problems; ++prob) {
            for (int eq = 0; eq < n_equations_per_problem; ++eq) {
                int idx = prob * n_equations_per_problem + eq;
                // Vary initial conditions slightly to create different problems
                float variation = 1.0f + prob * 0.01f; // 1.0, 1.01, 1.02, etc.
                state_data[idx] = static_cast<float>(base_system.initial_conditions[eq]) * variation;
            }
        }
        
        // GPU buffers
        GLuint state_buffer, param_buffer, result_buffer;
        
        glGenBuffers(1, &state_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, state_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, total_equations * sizeof(float), 
                     state_data.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, state_buffer);
        
        // Parameters
        struct {
            float dt;
            float t_start;
            int n_equations_per_problem;
            int n_problems;
            int n_steps_batch;
            float lambda;
        } params;
        
        params.dt = static_cast<float>(dt);
        params.t_start = static_cast<float>(t0);
        params.n_equations_per_problem = n_equations_per_problem;
        params.n_problems = n_problems;
        params.n_steps_batch = n_steps;
        params.lambda = static_cast<float>(lambda_it->second);
        
        glGenBuffers(1, &param_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(params), &params, GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, param_buffer);
        
        // Result buffer for ALL problems and timesteps
        size_t result_size = n_steps * total_equations * sizeof(float);
        glGenBuffers(1, &result_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, result_size, nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, result_buffer);
        
        // MASSIVE PARALLEL DISPATCH - use ALL GPU threads
        glUseProgram(parallel_program);
        GLuint work_groups = (total_equations + 3) / 4;  // Round up to use all threads
        std::cout << "- Dispatching " << work_groups << " workgroups" << std::endl;
        
        glDispatchCompute(work_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Read back ALL results
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
        float* all_results = static_cast<float*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, result_size, GL_MAP_READ_BIT));
        
        // Convert to output format: all_solutions[problem][timestep][equation]
        all_solutions.clear();
        all_solutions.resize(n_problems);
        
        for (int prob = 0; prob < n_problems; ++prob) {
            all_solutions[prob].resize(n_steps);
            for (int step = 0; step < n_steps; ++step) {
                all_solutions[prob][step].resize(n_equations_per_problem);
                for (int eq = 0; eq < n_equations_per_problem; ++eq) {
                    int result_idx = step * total_equations + prob * n_equations_per_problem + eq;
                    all_solutions[prob][step][eq] = static_cast<double>(all_results[result_idx]);
                }
            }
        }
        
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        
        // Cleanup
        glDeleteBuffers(1, &state_buffer);
        glDeleteBuffers(1, &param_buffer);
        glDeleteBuffers(1, &result_buffer);
    }
};

// Test massive parallelism vs single problem
void test_massive_parallelism() {
    std::cout << "=== TESTING MASSIVE GPU PARALLELISM ===" << std::endl;
    
    auto system = TestProblems::create_exponential_decay();
    const double dt = 0.01;
    const double tf = 1.0;
    
    Timer timer;
    
    // Test 1: Single problem (current approach)
    std::cout << "\n1. Single Problem (Current):" << std::endl;
    GPUSolver single_gpu;
    std::vector<std::vector<double>> single_solution;
    
    timer.start();
    single_gpu.solve(system, 0.0, tf, dt, system.initial_conditions, single_solution);
    double single_time = timer.elapsed();
    
    std::cout << "   Time: " << single_time * 1000 << " ms" << std::endl;
    std::cout << "   GPU utilization: 0.8% (1/128 threads)" << std::endl;
    
    // Test 2: Massive parallelism (128 problems)
    std::cout << "\n2. Massive Parallelism (128 Problems):" << std::endl;
    MassivelyParallelGPUSolver massive_gpu;
    std::vector<std::vector<std::vector<double>>> massive_solutions;
    
    timer.start();
    massive_gpu.solve_multiple_problems(system, 0.0, tf, dt, 128, massive_solutions);
    double massive_time = timer.elapsed();
    
    if (massive_time > 0) {
        std::cout << "   Time: " << massive_time * 1000 << " ms" << std::endl;
        std::cout << "   Effective throughput: " << 128 / massive_time << " problems/second" << std::endl;
        std::cout << "   Per-problem time: " << massive_time * 1000 / 128 << " ms" << std::endl;
        std::cout << "   Speedup vs single: " << (single_time * 128) / massive_time << "x" << std::endl;
    }
    
    // Verify first problem matches single solution
    if (!massive_solutions.empty() && !single_solution.empty()) {
        double max_diff = 0.0;
        for (size_t i = 0; i < single_solution.size(); ++i) {
            for (size_t j = 0; j < single_solution[i].size(); ++j) {
                double diff = std::abs(single_solution[i][j] - massive_solutions[0][i][j]);
                max_diff = std::max(max_diff, diff);
            }
        }
        std::cout << "   Verification: Max difference = " << max_diff << std::endl;
    }
}

int main() {
    test_massive_parallelism();
    return 0;
} 