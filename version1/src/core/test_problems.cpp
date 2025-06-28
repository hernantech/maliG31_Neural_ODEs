#include "test_problems.h"
#include <cmath>

ODESystem TestProblems::create_exponential_decay() {
    ODESystem system;
    system.name = "Exponential Decay";
    system.dimension = 1;
    system.t_start = 0.0;
    system.t_end = 5.0;
    system.initial_conditions = {1.0};
    system.parameters["lambda"] = 2.0;
    
    // RHS function: dy/dt = -lambda * y
    system.rhs = [](double t, const std::vector<double>& y) -> std::vector<double> {
        return {-2.0 * y[0]};
    };
    
    // Analytical solution: y(t) = y0 * exp(-lambda * t)
    system.analytical_solution = [](double t) -> std::vector<double> {
        return {std::exp(-2.0 * t)};
    };
    
    // GPU support
    system.gpu_info = ODESystem::GPUInfo{};
    system.gpu_info->builtin_rhs_name = "exponential";
    system.gpu_info->gpu_uniforms = {2.0f};  // lambda value
    
    return system;
}

ODESystem TestProblems::create_van_der_pol() {
    ODESystem system;
    system.name = "Van der Pol Oscillator";
    system.dimension = 2;
    system.t_start = 0.0;
    system.t_end = 20.0;
    system.initial_conditions = {2.0, 0.0};
    system.parameters["mu"] = 1.0;
    
    // RHS function: dx/dt = y, dy/dt = mu*(1-x^2)*y - x
    system.rhs = [](double t, const std::vector<double>& y) -> std::vector<double> {
        double x = y[0];
        double v = y[1];
        double mu = 1.0;
        return {v, mu * (1 - x*x) * v - x};
    };
    
    // GPU support
    system.gpu_info = ODESystem::GPUInfo{};
    system.gpu_info->builtin_rhs_name = "vanderpol";
    system.gpu_info->gpu_uniforms = {1.0f};  // mu value
    
    return system;
}

ODESystem TestProblems::create_scalability_test(int N) {
    ODESystem system;
    system.name = "Scalability Test N=" + std::to_string(N);
    system.dimension = N;
    system.t_start = 0.0;
    system.t_end = 5.0;
    system.parameters["epsilon"] = 0.1;
    
    system.initial_conditions.resize(N);
    for (int i = 0; i < N; ++i) {
        system.initial_conditions[i] = i * 0.1;
    }
    
    // RHS function: dxi/dt = -xi + sin(xi-1) + epsilon*xi+1
    system.rhs = [N](double t, const std::vector<double>& y) -> std::vector<double> {
        std::vector<double> dydt(N);
        const double eps = 0.1;
        
        for (int i = 0; i < N; ++i) {
            dydt[i] = -y[i];
            if (i > 0) dydt[i] += std::sin(y[i-1]);
            if (i < N-1) dydt[i] += eps * y[i+1];
        }
        return dydt;
    };
    
    return system;
} 