#pragma once
#include "solver_base.h"

class CPUSolver : public SolverBase {
public:
    void solve(const ODESystem& system, 
              double t0, double tf, double dt,
              const std::vector<double>& y0,
              std::vector<std::vector<double>>& solution) override;
    
    std::string name() const override { return "CPU_RK45"; }

private:
    std::vector<double> rk45_step(const ODESystem& system, double t, 
                                  const std::vector<double>& y, double h);
}; 