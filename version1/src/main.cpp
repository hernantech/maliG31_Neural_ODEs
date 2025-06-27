#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include "cpu_solver.h"
#include "gpu_solver.h"
#include "test_problems.h"
#include "timer.h"

double compute_error(const std::vector<std::vector<double>>& solution,
                    const ODESystem& system, double dt) {
    if (!system.analytical_solution) return -1.0;
    
    double max_error = 0.0;
    for (size_t i = 0; i < solution.size(); ++i) {
        double t = system.t_start + i * dt;
        auto analytical = system.analytical_solution(t);
        
        for (size_t j = 0; j < solution[i].size(); ++j) {
            double error = std::abs(solution[i][j] - analytical[j]);
            max_error = std::max(max_error, error);
        }
    }
    return max_error;
}

void run_benchmark(const ODESystem& system, double dt) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Benchmark: " << system.name << std::endl;
    std::cout << "System dimension: " << system.dimension << std::endl;
    std::cout << "Time step: " << dt << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // CPU Solver
    CPUSolver cpu_solver;
    Timer timer;
    
    std::cout << "\nRunning CPU solver..." << std::endl;
    timer.start();
    std::vector<std::vector<double>> cpu_solution;
    cpu_solver.solve(system, system.t_start, system.t_end, dt, 
                    system.initial_conditions, cpu_solution);
    double cpu_time = timer.elapsed();
    
    double cpu_error = compute_error(cpu_solution, system, dt);
    
    std::cout << "CPU Results:" << std::endl;
    std::cout << "  Time: " << std::fixed << std::setprecision(6) << cpu_time << " seconds" << std::endl;
    if (cpu_error >= 0) {
        std::cout << "  Max Error: " << std::scientific << std::setprecision(3) << cpu_error << std::endl;
    }
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0) 
              << system.dimension / cpu_time << " ODEs/second" << std::endl;
    
    // GPU Solver (only for systems it supports)
    if (system.name.find("Exponential") != std::string::npos || 
        system.name.find("Scalability") != std::string::npos) {
        
        GPUSolver gpu_solver;
        
        std::cout << "\nRunning GPU solver..." << std::endl;
        timer.start();
        std::vector<std::vector<double>> gpu_solution;
        gpu_solver.solve(system, system.t_start, system.t_end, dt, 
                        system.initial_conditions, gpu_solution);
        double gpu_time = timer.elapsed();
        
        double gpu_error = compute_error(gpu_solution, system, dt);
        
        std::cout << "GPU Results:" << std::endl;
        std::cout << "  Time: " << std::fixed << std::setprecision(6) << gpu_time << " seconds" << std::endl;
        if (gpu_error >= 0) {
            std::cout << "  Max Error: " << std::scientific << std::setprecision(3) << gpu_error << std::endl;
        }
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0) 
                  << system.dimension / gpu_time << " ODEs/second" << std::endl;
        
        // Comparison
        std::cout << "\nComparison:" << std::endl;
        if (gpu_time > 0) {
            double speedup = cpu_time / gpu_time;
            std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x";
            if (speedup > 1.0) {
                std::cout << " (GPU faster)";
            } else {
                std::cout << " (CPU faster)";
            }
            std::cout << std::endl;
        }
        
        // Solution consistency check
        if (!cpu_solution.empty() && !gpu_solution.empty()) {
            double max_diff = 0.0;
            size_t min_size = std::min(cpu_solution.size(), gpu_solution.size());
            for (size_t i = 0; i < min_size; ++i) {
                for (size_t j = 0; j < cpu_solution[i].size(); ++j) {
                    double diff = std::abs(cpu_solution[i][j] - gpu_solution[i][j]);
                    max_diff = std::max(max_diff, diff);
                }
            }
            std::cout << "  Max CPU-GPU difference: " << std::scientific 
                      << std::setprecision(3) << max_diff << std::endl;
        }
    }
}

int main() {
    std::cout << "RK45 CPU vs GPU Benchmark Suite" << std::endl;
    std::cout << "Orange Pi Zero 2W - Mali G31 MP2" << std::endl;
    
    const double dt = 0.01;
    
    // Test 1: Exponential Decay (validation)
    auto exp_decay = TestProblems::create_exponential_decay();
    run_benchmark(exp_decay, dt);
    
    // Test 2: Scalability tests
    std::vector<int> problem_sizes = {100, 1000, 10000};
    
    for (int N : problem_sizes) {
        auto scalability_test = TestProblems::create_scalability_test(N);
        run_benchmark(scalability_test, dt);
    }
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Benchmark Complete" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    return 0;
} 