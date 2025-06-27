#pragma once
#include <vector>
#include <string>
#include <functional>
#include <map>

struct ODESystem {
    std::string name;
    int dimension;
    std::function<std::vector<double>(double, const std::vector<double>&)> rhs;
    std::function<std::vector<double>(double)> analytical_solution;
    std::vector<double> initial_conditions;
    double t_start, t_end;
    std::map<std::string, double> parameters;
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