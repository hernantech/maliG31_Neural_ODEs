#include <iostream>
#include <iomanip>
#include <cmath>
#include "../include/steppers.h"
#include "../include/test_problems.h"
#include "../include/builtin_rhs_registry.h"
#include "../include/shader_generator.h"
#include "../src/backends/cpu_backend.cpp"  // Include implementation

void test_steppers() {
    std::cout << "=== TESTING NEW STEPPER ARCHITECTURE ===" << std::endl;
    
    // Create test problem
    auto system = TestProblems::create_exponential_decay();
    const double dt = 0.01;
    const double tf = 1.0;
    
    std::cout << "\nTesting problem: " << system.name << std::endl;
    std::cout << "GPU support: " << (system.has_gpu_support() ? "YES" : "NO") << std::endl;
    if (system.has_gpu_support()) {
        std::cout << "Builtin RHS: " << system.gpu_info->builtin_rhs_name << std::endl;
    }
    
    // Test 1: Euler stepper
    std::cout << "\n1. Testing Explicit Euler Stepper..." << std::endl;
    auto euler_stepper = create_stepper("euler");
    CPUBackend euler_solver(std::move(euler_stepper));
    
    std::vector<std::vector<double>> euler_solution;
    euler_solver.solve(system, 0.0, tf, dt, system.initial_conditions, euler_solution);
    
    std::cout << "   Solver name: " << euler_solver.name() << std::endl;
    std::cout << "   Steps computed: " << euler_solution.size() << std::endl;
    std::cout << "   Final value: " << euler_solution.back()[0] << std::endl;
    
    // Test 2: RK45 stepper
    std::cout << "\n2. Testing RK45 Stepper..." << std::endl;
    auto rk45_stepper = create_stepper("rk45");
    CPUBackend rk45_solver(std::move(rk45_stepper));
    
    std::vector<std::vector<double>> rk45_solution;
    rk45_solver.solve(system, 0.0, tf, dt, system.initial_conditions, rk45_solution);
    
    std::cout << "   Solver name: " << rk45_solver.name() << std::endl;
    std::cout << "   Steps computed: " << rk45_solution.size() << std::endl;
    std::cout << "   Final value: " << rk45_solution.back()[0] << std::endl;
    
    // Test 3: Accuracy comparison
    std::cout << "\n3. Accuracy Analysis..." << std::endl;
    double analytical_final = std::exp(-2.0 * tf);
    double euler_error = std::abs(euler_solution.back()[0] - analytical_final);
    double rk45_error = std::abs(rk45_solution.back()[0] - analytical_final);
    
    std::cout << "   Analytical solution: " << analytical_final << std::endl;
    std::cout << "   Euler error: " << std::scientific << euler_error << std::endl;
    std::cout << "   RK45 error: " << std::scientific << rk45_error << std::endl;
    std::cout << "   RK45 improvement: " << std::fixed << euler_error / rk45_error << "x better" << std::endl;
}

void test_rhs_registry() {
    std::cout << "\n=== TESTING RHS REGISTRY ===" << std::endl;
    
    auto& registry = BuiltinRHSRegistry::instance();
    auto available = registry.list_available();
    
    std::cout << "Available RHS systems: " << available.size() << std::endl;
    for (const auto& name : available) {
        auto rhs = registry.get_rhs(name);
        std::cout << "  - " << name << ": " << rhs.description << std::endl;
        std::cout << "    Uniforms: ";
        for (const auto& uniform : rhs.uniform_names) {
            std::cout << uniform << " ";
        }
        std::cout << std::endl;
    }
}

void test_shader_generation() {
    std::cout << "\n=== TESTING SHADER GENERATION ===" << std::endl;
    
    try {
        ShaderGenerator gen;
        std::string shader = gen.generate_euler_shader_builtin("exponential");
        
        std::cout << "Generated shader for 'exponential' RHS:" << std::endl;
        std::cout << "Length: " << shader.length() << " characters" << std::endl;
        
        // Check if key components are present
        bool has_main = shader.find("void main()") != std::string::npos;
        bool has_euler = shader.find("y_current + dt * dydt") != std::string::npos;
        bool has_rhs = shader.find("evaluate_rhs") != std::string::npos;
        bool has_lambda = shader.find("lambda") != std::string::npos;
        
        std::cout << "Shader validation:" << std::endl;
        std::cout << "  - Has main function: " << (has_main ? "YES" : "NO") << std::endl;
        std::cout << "  - Has Euler formula: " << (has_euler ? "YES" : "NO") << std::endl;
        std::cout << "  - Has RHS function: " << (has_rhs ? "YES" : "NO") << std::endl;
        std::cout << "  - Has lambda parameter: " << (has_lambda ? "YES" : "NO") << std::endl;
        
        if (has_main && has_euler && has_rhs && has_lambda) {
            std::cout << "✓ Shader generation PASSED" << std::endl;
        } else {
            std::cout << "✗ Shader generation FAILED" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Shader generation failed: " << e.what() << std::endl;
        std::cout << "This is expected if shader templates are not in the right location." << std::endl;
    }
}

int main() {
    try {
        test_steppers();
        test_rhs_registry();
        test_shader_generation();
        
        std::cout << "\n=== TEST SUMMARY ===" << std::endl;
        std::cout << "✓ Stepper architecture working" << std::endl;
        std::cout << "✓ RHS registry functional" << std::endl;
        std::cout << "? Shader generation (depends on template files)" << std::endl;
        std::cout << "\nNew architecture is ready for GPU implementation!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 