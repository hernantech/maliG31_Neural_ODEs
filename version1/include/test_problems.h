#pragma once
#include "solver_base.h"

class TestProblems {
public:
    static ODESystem create_exponential_decay();
    static ODESystem create_van_der_pol();
    static ODESystem create_scalability_test(int N);
}; 