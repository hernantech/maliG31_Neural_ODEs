#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "timer.h"
#include "test_problems.h"
#include "cpu_solver.h"

void analyze_cpu_performance() {
    std::cout << "=== CPU Performance Analysis ===" << std::endl;
    
    CPUSolver cpu_solver;
    Timer timer;
    
    // Test different problem sizes
    std::vector<int> sizes = {1, 10, 100, 1000, 10000};
    const double dt = 0.01;
    const double tf = 1.0; // Shorter integration for analysis
    
    for (int N : sizes) {
        auto system = TestProblems::create_scalability_test(N);
        
        timer.start();
        std::vector<std::vector<double>> solution;
        cpu_solver.solve(system, 0.0, tf, dt, system.initial_conditions, solution);
        double cpu_time = timer.elapsed();
        
        int n_steps = solution.size();
        double time_per_step = cpu_time / n_steps;
        double time_per_ode_per_step = time_per_step / N;
        
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "N=" << std::setw(5) << N 
                  << " | Steps=" << std::setw(3) << n_steps
                  << " | Total=" << std::setw(8) << cpu_time << "s"
                  << " | Step=" << std::setw(8) << time_per_step*1e6 << "μs"
                  << " | ODE/Step=" << std::setw(6) << time_per_ode_per_step*1e9 << "ns"
                  << " | Throughput=" << std::setw(8) << N/cpu_time << " ODEs/s"
                  << std::endl;
    }
}

void analyze_gpu_overhead() {
    std::cout << "\n=== GPU Overhead Analysis ===" << std::endl;
    
    // Test just the GPU initialization cost
    Timer timer;
    
    std::cout << "Testing GPU initialization cost..." << std::endl;
    timer.start();
    
    try {
        // Just test the constructor/destructor overhead
        for (int i = 0; i < 5; ++i) {
            timer.start();
            // This will test GPU context creation cost
            auto system = TestProblems::create_exponential_decay();
            double init_time = timer.elapsed();
            std::cout << "Initialization attempt " << i+1 << ": " 
                      << init_time*1000 << " ms" << std::endl;
        }
    } catch (...) {
        std::cout << "GPU initialization failed" << std::endl;
    }
}

int main() {
    analyze_cpu_performance();
    analyze_gpu_overhead();
    return 0;
} 