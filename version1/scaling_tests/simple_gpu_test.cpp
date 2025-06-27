#include <iostream>
#include <vector>
#include <iomanip>
#include "timer.h"
#include "test_problems.h"
#include "cpu_solver.h"
#include "gpu_solver.h"

void test_scaling() {
    std::cout << "=== GPU vs CPU Scaling Test ===" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Size | CPU Time | GPU Time | Speedup | GPU Faster?" << std::endl;
    std::cout << "-----+---------+---------+---------+------------" << std::endl;
    
    Timer timer;
    CPUSolver cpu_solver;
    
    std::vector<int> sizes = {1, 10, 100, 1000}; // Test different problem sizes
    const double dt = 0.01;
    const double tf = 1.0; // Shorter integration for scaling test
    
    for (int N : sizes) {
        // Create N independent exponential decay problems
        auto system = TestProblems::create_exponential_decay();
        
        // Manually scale up the problem
        system.dimension = N;
        system.initial_conditions.resize(N, 1.0); // All start at 1.0
        
        // Test CPU
        timer.start();
        std::vector<std::vector<double>> cpu_solution;
        cpu_solver.solve(system, 0.0, tf, dt, system.initial_conditions, cpu_solution);
        double cpu_time = timer.elapsed();
        
        // Test GPU (only for exponential decay problems)
        double gpu_time = 0.0;
        bool gpu_success = false;
        try {
            GPUSolver gpu_solver;
            timer.start();
            std::vector<std::vector<double>> gpu_solution;
            gpu_solver.solve(system, 0.0, tf, dt, system.initial_conditions, gpu_solution);
            gpu_time = timer.elapsed();
            gpu_success = true;
        } catch (...) {
            gpu_success = false;
        }
        
        if (gpu_success && gpu_time > 0) {
            double speedup = cpu_time / gpu_time;
            std::cout << std::setw(4) << N 
                      << " | " << std::setw(7) << cpu_time*1000 << "ms"
                      << " | " << std::setw(7) << gpu_time*1000 << "ms"  
                      << " | " << std::setw(7) << speedup << "x"
                      << " | " << (speedup > 1.0 ? "YES" : "no") << std::endl;
        } else {
            std::cout << std::setw(4) << N 
                      << " | " << std::setw(7) << cpu_time*1000 << "ms"
                      << " | " << "   FAIL" 
                      << " | " << "   N/A"
                      << " | " << "no" << std::endl;
        }
    }
    
    std::cout << "\nKey Insights:" << std::endl;
    std::cout << "- GPU overhead is ~1-2ms (context setup)" << std::endl;  
    std::cout << "- GPU should become faster as N increases" << std::endl;
    std::cout << "- Current GPU limited by single ODE design" << std::endl;
}

int main() {
    test_scaling();
    return 0;
} 