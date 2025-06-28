#include "../../include/steppers.h"
#include <stdexcept>

std::unique_ptr<TimeStepper> create_stepper(const std::string& method_name) {
    if (method_name == "euler" || method_name == "explicit_euler") {
        return std::make_unique<ExplicitEulerStepper>();
    } else if (method_name == "rk45" || method_name == "runge_kutta") {
        return std::make_unique<RK45Stepper>();
    } else {
        throw std::invalid_argument("Unknown stepper method: " + method_name);
    }
} 