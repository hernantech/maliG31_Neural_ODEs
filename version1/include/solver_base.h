#pragma once
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <optional>

struct ODESystem {
    std::string name;
    int dimension;
    std::function<std::vector<double>(double, const std::vector<double>&)> rhs;
    std::function<std::vector<double>(double)> analytical_solution;
    std::vector<double> initial_conditions;
    double t_start, t_end;
    std::map<std::string, double> parameters;
    
    // GPU-specific information
    struct GPUInfo {
        std::string glsl_rhs_code;           // Custom GLSL snippet
        std::vector<float> gpu_uniforms;     // Additional parameters
        std::string builtin_rhs_name;        // e.g., "exponential", "vanderpol"
        bool force_cpu_fallback = false;    // Disable GPU for this problem
    };
    std::optional<GPUInfo> gpu_info;
    
    // Helper methods
    bool has_gpu_support() const { return gpu_info.has_value(); }
    bool use_builtin_rhs() const { 
        return gpu_info && !gpu_info->builtin_rhs_name.empty(); 
    }
};

class SolverBase {
public:
    virtual ~SolverBase() = default;
    virtual void solve(const ODESystem& system, 
                      double t0, double tf, double dt,
                      const std::vector<double>& y0,
                      std::vector<std::vector<double>>& solution) = 0;
    virtual std::string name() const = 0;
}; 