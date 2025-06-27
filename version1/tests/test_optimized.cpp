#include <iostream>
#include <vector>
#include <iomanip>
#include "timer.h"
#include "test_problems.h"
#include "cpu_solver.h"
#include "gpu_solver.h"

// Forward declaration of optimized GPU solver
extern void test_optimized_gpu();

int main() {
    std::cout << "=== GPU Optimization Test ===" << std::endl;
    std::cout << "Testing standard vs optimized GPU implementations" << std::endl;
    
    // Test the optimized GPU solver
    test_optimized_gpu();
    
    std::cout << "\n=== Performance Analysis ===" << std::endl;
    std::cout << "Key optimizations applied:" << std::endl;
    std::cout << "✓ Shared memory for Butcher tableau coefficients" << std::endl;
    std::cout << "✓ Memory coalescing and optimal alignment" << std::endl;
    std::cout << "✓ Register usage optimization" << std::endl;
    std::cout << "✓ Mali G31 MP2 specific tuning" << std::endl;
    std::cout << "✓ Loop unrolling for better instruction pipelining" << std::endl;
    
    return 0;
} 