#pragma once
#include "solver_base.h"
#include <memory>

// Abstract base class for time-stepping algorithms
class TimeStepper {
public:
    virtual ~TimeStepper() = default;
    
    // Advance the solution by one time step
    virtual void step(const ODESystem& system, double t, double dt, 
                     std::vector<double>& y) = 0;
    
    virtual std::string name() const = 0;
    virtual int order() const = 0;  // Accuracy order of the method
};

// Explicit Euler method: y_{n+1} = y_n + dt * f(t_n, y_n)
class ExplicitEulerStepper : public TimeStepper {
public:
    void step(const ODESystem& system, double t, double dt, 
             std::vector<double>& y) override;
    
    std::string name() const override { return "Explicit_Euler"; }
    int order() const override { return 1; }
};

// Runge-Kutta 4th/5th order (Dormand-Prince)
class RK45Stepper : public TimeStepper {
public:
    void step(const ODESystem& system, double t, double dt, 
             std::vector<double>& y) override;
    
    std::string name() const override { return "RK45_Dormand_Prince"; }
    int order() const override { return 5; }

private:
    std::vector<double> rk45_step(const ODESystem& system, double t, 
                                 const std::vector<double>& y, double h);
};

// Factory function for creating steppers
std::unique_ptr<TimeStepper> create_stepper(const std::string& method_name); 