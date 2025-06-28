#include "../../include/steppers.h"

void ExplicitEulerStepper::step(const ODESystem& system, double t, double dt, 
                               std::vector<double>& y) {
    // Explicit Euler: y_{n+1} = y_n + dt * f(t_n, y_n)
    auto dydt = system.rhs(t, y);
    
    for (size_t i = 0; i < y.size(); ++i) {
        y[i] += dt * dydt[i];
    }
} 