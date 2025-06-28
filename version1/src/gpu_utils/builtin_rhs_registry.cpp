#include "../../include/builtin_rhs_registry.h"
#include <stdexcept>

BuiltinRHSRegistry& BuiltinRHSRegistry::instance() {
    static BuiltinRHSRegistry instance;
    return instance;
}

BuiltinRHSRegistry::BuiltinRHSRegistry() {
    register_builtin_systems();
}

void BuiltinRHSRegistry::register_rhs(const std::string& name, 
                                     const RHSDefinition& definition) {
    registry_[name] = definition;
}

RHSDefinition BuiltinRHSRegistry::get_rhs(const std::string& name) {
    auto it = registry_.find(name);
    if (it == registry_.end()) {
        throw std::invalid_argument("Unknown RHS system: " + name);
    }
    return it->second;
}

std::vector<std::string> BuiltinRHSRegistry::list_available() {
    std::vector<std::string> names;
    for (const auto& pair : registry_) {
        names.push_back(pair.first);
    }
    return names;
}

bool BuiltinRHSRegistry::has_rhs(const std::string& name) {
    return registry_.find(name) != registry_.end();
}

void BuiltinRHSRegistry::register_builtin_systems() {
    // Exponential decay: dy/dt = -lambda * y
    RHSDefinition exponential;
    exponential.glsl_code = R"(
float evaluate_rhs(uint eq_idx, float y_val, float t) {
    return -lambda * y_val;
}
)";
    exponential.uniform_names = {"lambda"};
    exponential.problem_type_id = 0;
    exponential.description = "Exponential decay: dy/dt = -lambda * y";
    register_rhs("exponential", exponential);
    
    // Van der Pol oscillator: dx/dt = y, dy/dt = mu*(1-x^2)*y - x
    RHSDefinition vanderpol;
    vanderpol.glsl_code = R"(
float evaluate_rhs(uint eq_idx, float y_val, float t) {
    if (eq_idx % 2u == 0u) {
        // Position equation: dx/dt = v
        uint v_idx = eq_idx + 1u;
        return (v_idx < uint(n_equations)) ? current_state[v_idx] : 0.0;
    } else {
        // Velocity equation: dv/dt = mu*(1-x^2)*v - x
        uint x_idx = eq_idx - 1u;
        float x = current_state[x_idx];
        return mu * (1.0 - x*x) * y_val - x;
    }
}
)";
    vanderpol.uniform_names = {"mu"};
    vanderpol.problem_type_id = 1;
    vanderpol.description = "Van der Pol oscillator";
    register_rhs("vanderpol", vanderpol);
    
    // Lorenz system: dx/dt = σ(y-x), dy/dt = x(ρ-z)-y, dz/dt = xy-βz
    RHSDefinition lorenz;
    lorenz.glsl_code = R"(
float evaluate_rhs(uint eq_idx, float y_val, float t) {
    uint base_idx = (eq_idx / 3u) * 3u;
    uint local_idx = eq_idx % 3u;
    
    if (base_idx + 2u < uint(n_equations)) {
        float x = current_state[base_idx + 0u];
        float y = current_state[base_idx + 1u]; 
        float z = current_state[base_idx + 2u];
        
        if (local_idx == 0u) return sigma * (y - x);           // dx/dt
        if (local_idx == 1u) return x * (rho - z) - y;        // dy/dt
        if (local_idx == 2u) return x * y - beta * z;         // dz/dt
    }
    return 0.0;
}
)";
    lorenz.uniform_names = {"sigma", "rho", "beta"};
    lorenz.problem_type_id = 2;
    lorenz.description = "Lorenz system";
    register_rhs("lorenz", lorenz);
    
    // Harmonic oscillator: d²x/dt² = -ω²x
    RHSDefinition harmonic;
    harmonic.glsl_code = R"(
float evaluate_rhs(uint eq_idx, float y_val, float t) {
    if (eq_idx % 2u == 0u) {
        // Position equation: dx/dt = v
        uint v_idx = eq_idx + 1u;
        return (v_idx < uint(n_equations)) ? current_state[v_idx] : 0.0;
    } else {
        // Velocity equation: dv/dt = -ω²x  
        uint x_idx = eq_idx - 1u;
        return -omega_sq * current_state[x_idx];
    }
}
)";
    harmonic.uniform_names = {"omega_sq"};
    harmonic.problem_type_id = 3;
    harmonic.description = "Harmonic oscillator";
    register_rhs("harmonic", harmonic);
} 