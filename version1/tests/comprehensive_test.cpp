#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include "timer.h"
#include "test_problems.h"
#include "cpu_solver.h"
#include "gpu_solver.h"

// Forward declaration for optimized GPU solver
class OptimizedGPUSolver : public GPUSolver {
public:
    void solve_optimized(const ODESystem& system, 
                        double t0, double tf, double dt,
                        const std::vector<double>& y0,
                        std::vector<std::vector<double>>& solution);
};

void run_comprehensive_comparison() {
    std::cout << "=== COMPREHENSIVE SOLVER COMPARISON ===" << std::endl;
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
    GPUSolver standard_gpu;
    std::vector<std::vector<double>> gpu_solution;
    
    timer.start();
    standard_gpu.solve(system, 0.0, tf, dt, system.initial_conditions, gpu_solution);
    double gpu_time = timer.elapsed();
    
    // Test 3: Optimized GPU Solver (if available)
    std::cout << "3. Testing Optimized GPU Solver..." << std::endl;
    double optimized_time = 0.0;
    std::vector<std::vector<double>> optimized_solution;
    bool optimized_available = false;
    
    try {
        OptimizedGPUSolver optimized_gpu;
        timer.start();
        optimized_gpu.solve_optimized(system, 0.0, tf, dt, system.initial_conditions, optimized_solution);
        optimized_time = timer.elapsed();
        optimized_available = true;
    } catch (...) {
        std::cout << "   Optimized GPU solver not available" << std::endl;
    }
    
    // Results Summary
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PERFORMANCE COMPARISON RESULTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    std::cout << "Solver Type           | Time (ms) | Speedup vs CPU | Relative Performance" << std::endl;
    std::cout << "---------------------+-----------+----------------+--------------------" << std::endl;
    
    // CPU Results
    std::cout << "CPU (Reference)       | " << std::setw(8) << cpu_time * 1000 
              << "  | " << std::setw(13) << "1.00x" 
              << "  | Baseline" << std::endl;
    
    // Standard GPU Results
    if (gpu_time > 0 && !gpu_solution.empty()) {
        double gpu_speedup = cpu_time / gpu_time;
        std::cout << "Standard GPU          | " << std::setw(8) << gpu_time * 1000 
                  << "  | " << std::setw(13) << gpu_speedup << "x"
                  << "  | " << (gpu_speedup > 1.0 ? "FASTER" : "slower") << std::endl;
    } else {
        std::cout << "Standard GPU          | " << std::setw(8) << "FAILED" 
                  << "  | " << std::setw(13) << "N/A" 
                  << "  | Failed" << std::endl;
    }
    
    // Optimized GPU Results
    if (optimized_available && optimized_time > 0 && !optimized_solution.empty()) {
        double opt_speedup = cpu_time / optimized_time;
        std::cout << "Optimized GPU         | " << std::setw(8) << optimized_time * 1000 
                  << "  | " << std::setw(13) << opt_speedup << "x"
                  << "  | " << (opt_speedup > 1.0 ? "FASTER" : "slower") << std::endl;
                  
        // GPU vs GPU comparison
        if (gpu_time > 0) {
            double gpu_improvement = gpu_time / optimized_time;
            std::cout << "\nGPU Optimization Improvement: " << gpu_improvement << "x faster" << std::endl;
        }
    } else {
        std::cout << "Optimized GPU         | " << std::setw(8) << "N/A" 
                  << "  | " << std::setw(13) << "N/A" 
                  << "  | Not available" << std::endl;
    }
    
    // Accuracy Comparison
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "ACCURACY COMPARISON" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    // Calculate analytical error for final time point
    auto analytical_final = system.analytical_solution(tf);
    double analytical_value = analytical_final[0];
    
    if (!cpu_solution.empty()) {
        double cpu_final = cpu_solution.back()[0];
        double cpu_error = std::abs(cpu_final - analytical_value);
        std::cout << "CPU Error vs Analytical:      " << std::scientific << cpu_error << std::endl;
    }
    
    if (!gpu_solution.empty()) {
        double gpu_final = gpu_solution.back()[0];
        double gpu_error = std::abs(gpu_final - analytical_value);
        std::cout << "Standard GPU Error:           " << std::scientific << gpu_error << std::endl;
        
        if (!cpu_solution.empty()) {
            double cpu_gpu_diff = std::abs(cpu_solution.back()[0] - gpu_final);
            std::cout << "CPU vs Standard GPU Diff:    " << std::scientific << cpu_gpu_diff << std::endl;
        }
    }
    
    if (optimized_available && !optimized_solution.empty()) {
        double opt_final = optimized_solution.back()[0];
        double opt_error = std::abs(opt_final - analytical_value);
        std::cout << "Optimized GPU Error:          " << std::scientific << opt_error << std::endl;
        
        if (!cpu_solution.empty()) {
            double cpu_opt_diff = std::abs(cpu_solution.back()[0] - opt_final);
            std::cout << "CPU vs Optimized GPU Diff:   " << std::scientific << cpu_opt_diff << std::endl;
        }
    }
    
    // Summary and Insights
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "INSIGHTS AND ANALYSIS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    std::cout << "Problem Size: N=" << system.dimension << " ODEs, " << int((tf - 0.0) / dt) + 1 << " timesteps" << std::endl;
    std::cout << "Integration: t=0 to t=" << tf << " with dt=" << dt << std::endl;
    
    if (gpu_time > 0 && cpu_time > 0) {
        double overhead = gpu_time - cpu_time;
        std::cout << "GPU Overhead: " << overhead * 1000 << " ms" << std::endl;
        std::cout << "GPU Efficiency: " << (cpu_time / gpu_time) * 100 << "%" << std::endl;
        
        if (overhead > 0.001) { // More than 1ms overhead
            std::cout << "Note: GPU overhead dominates for small problems" << std::endl;
            std::cout << "GPU will be faster for larger N (>1000 ODEs)" << std::endl;
        }
    }
    
    std::cout << "\nFor Mali G31 MP2 optimization:" << std::endl;
    std::cout << "- Current workgroup size: 64 threads" << std::endl;
    std::cout << "- Batch processing: " << int((tf - 0.0) / dt) + 1 << " timesteps per dispatch" << std::endl;
    std::cout << "- Memory transfers: 1 upload + 1 download (optimal)" << std::endl;
}

int main() {
    run_comprehensive_comparison();
    return 0;
} 