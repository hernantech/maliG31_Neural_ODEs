#include "../../include/steppers.h"
#include <cmath>

void RK45Stepper::step(const ODESystem& system, double t, double dt, 
                      std::vector<double>& y) {
    y = rk45_step(system, t, y, dt);
}

std::vector<double> RK45Stepper::rk45_step(const ODESystem& system, double t, 
                                          const std::vector<double>& y, double h) {
    // RK45 (Dormand-Prince) coefficients
    const double a21 = 1.0/5.0;
    const double a31 = 3.0/40.0, a32 = 9.0/40.0;
    const double a41 = 44.0/45.0, a42 = -56.0/15.0, a43 = 32.0/9.0;
    const double a51 = 19372.0/6561.0, a52 = -25360.0/2187.0, 
                 a53 = 64448.0/6561.0, a54 = -212.0/729.0;
    const double a61 = 9017.0/3168.0, a62 = -355.0/33.0, 
                 a63 = 46732.0/5247.0, a64 = 49.0/176.0, a65 = -5103.0/18656.0;
                 
    const double b1 = 35.0/384.0, b3 = 500.0/1113.0, b4 = 125.0/192.0,
                 b5 = -2187.0/6784.0, b6 = 11.0/84.0;
    
    // Compute k values
    auto k1 = system.rhs(t, y);
    for (auto& k : k1) k *= h;
    
    std::vector<double> y_temp(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        y_temp[i] = y[i] + a21 * k1[i];
    }
    auto k2 = system.rhs(t + h/5.0, y_temp);
    for (auto& k : k2) k *= h;
    
    for (size_t i = 0; i < y.size(); ++i) {
        y_temp[i] = y[i] + a31 * k1[i] + a32 * k2[i];
    }
    auto k3 = system.rhs(t + 3.0*h/10.0, y_temp);
    for (auto& k : k3) k *= h;
    
    for (size_t i = 0; i < y.size(); ++i) {
        y_temp[i] = y[i] + a41 * k1[i] + a42 * k2[i] + a43 * k3[i];
    }
    auto k4 = system.rhs(t + 4.0*h/5.0, y_temp);
    for (auto& k : k4) k *= h;
    
    for (size_t i = 0; i < y.size(); ++i) {
        y_temp[i] = y[i] + a51 * k1[i] + a52 * k2[i] + a53 * k3[i] + a54 * k4[i];
    }
    auto k5 = system.rhs(t + 8.0*h/9.0, y_temp);
    for (auto& k : k5) k *= h;
    
    for (size_t i = 0; i < y.size(); ++i) {
        y_temp[i] = y[i] + a61 * k1[i] + a62 * k2[i] + a63 * k3[i] + 
                    a64 * k4[i] + a65 * k5[i];
    }
    auto k6 = system.rhs(t + h, y_temp);
    for (auto& k : k6) k *= h;
    
    // Compute final result
    std::vector<double> y_new(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        y_new[i] = y[i] + b1 * k1[i] + b3 * k3[i] + b4 * k4[i] + 
                   b5 * k5[i] + b6 * k6[i];
    }
    
    return y_new;
} 