#include <iostream>
#include <cmath>
#include "../include/test_problems.h"
#include "../include/gpu_euler_backend.h"

int main() {
    std::cout << "=== SIMPLE GPU TEST ===" << std::endl;
    
    try {
        // Test only exponential decay first
        auto system = TestProblems::create_exponential_decay();
        const double dt = 0.1;  // Larger timestep for faster test
        const double tf = 0.5;  // Shorter time
        
        std::cout << "Testing: " << system.name << std::endl;
        std::cout << "GPU support: " << (system.has_gpu_support() ? "YES" : "NO") << std::endl;
        
        GPUEulerBackend gpu_solver;
        std::vector<std::vector<double>> solution;
        
        gpu_solver.solve(system, 0.0, tf, dt, system.initial_conditions, solution);
        
        if (!solution.empty()) {
            std::cout << "✓ GPU solver completed successfully" << std::endl;
            std::cout << "Initial: " << solution[0][0] << std::endl;
            std::cout << "Final: " << solution.back()[0] << std::endl;
            std::cout << "Expected: " << std::exp(-2.0 * tf) << std::endl;
        } else {
            std::cout << "✗ GPU solver returned empty solution" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 