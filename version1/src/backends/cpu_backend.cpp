#include "../../include/steppers.h"
#include "../../include/solver_base.h"
#include <memory>

class CPUBackend : public SolverBase {
public:
    CPUBackend(std::unique_ptr<TimeStepper> stepper) 
        : stepper_(std::move(stepper)) {}
    
    void solve(const ODESystem& system, 
              double t0, double tf, double dt,
              const std::vector<double>& y0,
              std::vector<std::vector<double>>& solution) override {
        
        int n_steps = static_cast<int>((tf - t0) / dt) + 1;
        solution.clear();
        solution.reserve(n_steps);
        
        std::vector<double> y = y0;
        double t = t0;
        
        // Store initial condition
        solution.push_back(y);
        
        // Integration loop using stepper
        for (int i = 1; i < n_steps; ++i) {
            stepper_->step(system, t, dt, y);
            t = t0 + i * dt;
            solution.push_back(y);
        }
    }
    
    std::string name() const override { 
        return "CPU_" + stepper_->name(); 
    }

private:
    std::unique_ptr<TimeStepper> stepper_;
}; 