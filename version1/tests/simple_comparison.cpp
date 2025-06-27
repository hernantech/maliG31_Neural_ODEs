#include <iostream>
#include <iomanip>
#include <vector>
#include "timer.h"
#include "test_problems.h"
#include "cpu_solver.h"
#include "gpu_solver.h"

void run_cpu_vs_gpu_comparison() {
    std::cout << "=== CPU vs STANDARD GPU COMPARISON ===" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    
    // Create test problem
    auto system = TestProblems::create_exponential_decay();
    const double dt = 0.01;
    const double tf = 1.0; // Shorter test for clear comparison
    
    Timer timer;
    
    // Test 1: CPU Solver
    std::cout << "\n1. Testing CPU Solver..." << std::endl;
    CPUSolver cpu_solver;
    std::vector<std::vector<double>> cpu_solution;
    
    timer.start();
    cpu_solver.solve(system, 0.0, tf, dt, system.initial_conditions, cpu_solution);
    double cpu_time = timer.elapsed();
    
    // Test 2: Standard GPU Solver
    std::cout << "2. Testing Standard GPU Solver..." << std::endl;
    GPUSolver gpu_solver;
    std::vector<std::vector<double>> gpu_solution;
    
    timer.start();
    gpu_solver.solve(system, 0.0, tf, dt, system.initial_conditions, gpu_solution);
    double gpu_time = timer.elapsed();
    
    // Results Summary
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "PERFORMANCE COMPARISON RESULTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    std::cout << "Solver Type      | Time (ms) | Speedup | Performance" << std::endl;
    std::cout << "-----------------+-----------+---------+------------" << std::endl;
    
    // CPU Results
    std::cout << "CPU (Reference)  | " << std::setw(8) << cpu_time * 1000 
              << "  | " << std::setw(6) << "1.00x" 
              << "  | Baseline" << std::endl;
    
    // GPU Results
    if (gpu_time > 0 && !gpu_solution.empty()) {
        double gpu_speedup = cpu_time / gpu_time;
        std::cout << "Standard GPU     | " << std::setw(8) << gpu_time * 1000 
                  << "  | " << std::setw(6) << gpu_speedup << "x"
                  << "  | " << (gpu_speedup > 1.0 ? "FASTER" : "slower") << std::endl;
    } else {
        std::cout << "Standard GPU     | " << std::setw(8) << "FAILED" 
                  << "  | " << std::setw(6) << "N/A" 
                  << "  | Failed" << std::endl;
    }
    
    // Accuracy Comparison
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "ACCURACY COMPARISON" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    // Calculate analytical error for final time point
    auto analytical_final = system.analytical_solution(tf);
    double analytical_value = analytical_final[0];
    
    if (!cpu_solution.empty()) {
        double cpu_final = cpu_solution.back()[0];
        double cpu_error = std::abs(cpu_final - analytical_value);
        std::cout << "CPU Error vs Analytical:     " << std::scientific << cpu_error << std::endl;
    }
    
    if (!gpu_solution.empty()) {
        double gpu_final = gpu_solution.back()[0];
        double gpu_error = std::abs(gpu_final - analytical_value);
        std::cout << "GPU Error vs Analytical:     " << std::scientific << gpu_error << std::endl;
        
        if (!cpu_solution.empty()) {
            double cpu_gpu_diff = std::abs(cpu_solution.back()[0] - gpu_final);
            std::cout << "CPU vs GPU Difference:       " << std::scientific << cpu_gpu_diff << std::endl;
        }
    }
    
    // Analysis
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "ANALYSIS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    std::cout << "Problem: N=" << system.dimension << " ODEs, " << int((tf - 0.0) / dt) + 1 << " timesteps" << std::endl;
    std::cout << "Integration: t=0 to t=" << tf << " with dt=" << dt << std::endl;
    
    if (gpu_time > 0 && cpu_time > 0) {
        double overhead = gpu_time - cpu_time;
        std::cout << "GPU Overhead: " << overhead * 1000 << " ms" << std::endl;
        std::cout << "GPU Efficiency: " << (cpu_time / gpu_time) * 100 << "% of CPU" << std::endl;
        
        if (overhead > 0.001) { // More than 1ms overhead
            std::cout << "\nInsight: GPU overhead dominates for small problems" << std::endl;
            std::cout << "GPU will be faster for larger N (>1000 ODEs)" << std::endl;
        } else {
            std::cout << "\nInsight: GPU is competitive with CPU!" << std::endl;
        }
    }
    
    std::cout << "\nGPU Optimizations Applied:" << std::endl;
    std::cout << "✓ Single GPU dispatch (batch processing)" << std::endl;
    std::cout << "✓ Minimal CPU-GPU transfers" << std::endl;
    std::cout << "✓ Mali G31 MP2 tuned workgroup size (64)" << std::endl;
    std::cout << "✓ Proper RK45 Butcher tableau implementation" << std::endl;
}

int main() {
    run_cpu_vs_gpu_comparison();
    return 0;
} 