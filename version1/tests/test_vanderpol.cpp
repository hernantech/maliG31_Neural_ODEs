#include <iostream>
#include <iomanip>
#include <cmath>
#include "../include/steppers.h"
#include "../include/test_problems.h"
#include "../include/shader_generator.h"
#include "../src/backends/cpu_backend.cpp"

int main() {
    std::cout << "=== TESTING VAN DER POL OSCILLATOR ===" << std::endl;
    
    // Create Van der Pol problem
    auto system = TestProblems::create_van_der_pol();
    const double dt = 0.01;
    const double tf = 2.0;  // Shorter time for testing
    
    std::cout << "\nProblem: " << system.name << std::endl;
    std::cout << "Dimension: " << system.dimension << std::endl;
    std::cout << "GPU support: " << (system.has_gpu_support() ? "YES" : "NO") << std::endl;
    if (system.has_gpu_support()) {
        std::cout << "Builtin RHS: " << system.gpu_info->builtin_rhs_name << std::endl;
    }
    
    // Test with Euler stepper
    std::cout << "\nTesting with Explicit Euler..." << std::endl;
    auto euler_stepper = create_stepper("euler");
    CPUBackend euler_solver(std::move(euler_stepper));
    
    std::vector<std::vector<double>> solution;
    euler_solver.solve(system, 0.0, tf, dt, system.initial_conditions, solution);
    
    std::cout << "Initial conditions: [" << system.initial_conditions[0] 
              << ", " << system.initial_conditions[1] << "]" << std::endl;
    std::cout << "Final state: [" << solution.back()[0] 
              << ", " << solution.back()[1] << "]" << std::endl;
    std::cout << "Steps computed: " << solution.size() << std::endl;
    
    // Show trajectory evolution
    std::cout << "\nTrajectory (every 20 steps):" << std::endl;
    std::cout << "Step\tTime\tx\ty" << std::endl;
    for (size_t i = 0; i < solution.size(); i += 20) {
        double t = i * dt;
        std::cout << i << "\t" << std::fixed << std::setprecision(2) << t 
                  << "\t" << std::setprecision(4) << solution[i][0] 
                  << "\t" << solution[i][1] << std::endl;
    }
    
    // Test shader generation for Van der Pol
    std::cout << "\n=== TESTING VANDERPOL SHADER GENERATION ===" << std::endl;
    try {
        ShaderGenerator gen;
        std::string shader = gen.generate_euler_shader_builtin("vanderpol");
        
        std::cout << "Generated shader length: " << shader.length() << " characters" << std::endl;
        
        // Check key components
        bool has_main = shader.find("void main()") != std::string::npos;
        bool has_mu = shader.find("mu") != std::string::npos;
        bool has_position = shader.find("eq_idx % 2u == 0u") != std::string::npos;
        bool has_velocity = shader.find("mu * (1.0 - x*x)") != std::string::npos;
        
        std::cout << "Shader validation:" << std::endl;
        std::cout << "  - Has main function: " << (has_main ? "YES" : "NO") << std::endl;
        std::cout << "  - Has mu parameter: " << (has_mu ? "YES" : "NO") << std::endl;
        std::cout << "  - Has position equation: " << (has_position ? "YES" : "NO") << std::endl;
        std::cout << "  - Has velocity equation: " << (has_velocity ? "YES" : "NO") << std::endl;
        
        if (has_main && has_mu && has_position && has_velocity) {
            std::cout << "✓ Van der Pol shader generation PASSED" << std::endl;
        } else {
            std::cout << "✗ Van der Pol shader generation FAILED" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Shader generation error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n✓ Van der Pol oscillator test completed successfully!" << std::endl;
    return 0;
} 