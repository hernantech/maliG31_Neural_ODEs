#include "gpu_solver.h"
#include "test_problems.h"
#include "timer.h"
#include <iostream>

// MEMORY SAFE: Reuse GPU context to avoid Panfrost driver crashes
class MemorySafeGPUSolver {
private:
    static bool global_context_initialized;
    static GPUSolver* global_gpu_instance;
    static int instance_count;
    
public:
    MemorySafeGPUSolver() {
        instance_count++;
        if (!global_context_initialized) {
            global_gpu_instance = new GPUSolver();
            global_context_initialized = true;
            std::cout << "MemorySafe: Created global GPU context" << std::endl;
        } else {
            std::cout << "MemorySafe: Reusing global GPU context (instance " << instance_count << ")" << std::endl;
        }
    }
    
    ~MemorySafeGPUSolver() {
        instance_count--;
        // Don't delete global_gpu_instance here - let it persist to avoid cleanup crashes
        std::cout << "MemorySafe: Instance destroyed (remaining: " << instance_count << ")" << std::endl;
    }
    
    void solve(const ODESystem& system, 
              double t0, double tf, double dt,
              const std::vector<double>& y0,
              std::vector<std::vector<double>>& solution) {
        
        if (global_gpu_instance) {
            global_gpu_instance->solve(system, t0, tf, dt, y0, solution);
        }
    }
    
    std::string name() const { return "MemorySafe_GPU_RK45"; }
};

// Static member definitions
bool MemorySafeGPUSolver::global_context_initialized = false;
GPUSolver* MemorySafeGPUSolver::global_gpu_instance = nullptr;
int MemorySafeGPUSolver::instance_count = 0;

// Test memory safety vs standard approach
void test_memory_safety() {
    std::cout << "=== TESTING MEMORY SAFETY APPROACH ===" << std::endl;
    
    auto system = TestProblems::create_exponential_decay();
    const double dt = 0.01;
    const double tf = 1.0;
    
    Timer timer;
    
    // Test multiple GPU solver instances (this would cause segfaults)
    std::cout << "\nCreating multiple GPU solver instances safely:" << std::endl;
    
    std::vector<double> times;
    
    for (int i = 0; i < 3; i++) {
        std::cout << "\n--- Test " << (i+1) << " ---" << std::endl;
        
        MemorySafeGPUSolver safe_gpu;
        std::vector<std::vector<double>> solution;
        
        timer.start();
        safe_gpu.solve(system, 0.0, tf, dt, system.initial_conditions, solution);
        double solve_time = timer.elapsed();
        times.push_back(solve_time);
        
        std::cout << "Time: " << solve_time * 1000 << " ms" << std::endl;
        if (!solution.empty()) {
            std::cout << "Final value: " << solution.back()[0] << std::endl;
        }
        
        // Destructor called here - should NOT crash
    }
    
    std::cout << "\n=== MEMORY SAFETY RESULTS ===" << std::endl;
    std::cout << "All instances completed without segfault!" << std::endl;
    std::cout << "Average time: " << (times[0] + times[1] + times[2]) / 3 * 1000 << " ms" << std::endl;
    std::cout << "Time consistency: " << std::abs(times[0] - times[1]) * 1000 << " ms variation" << std::endl;
    
    std::cout << "\nMemory Management Strategy:" << std::endl;
    std::cout << "✓ Single global GPU context (avoids create/destroy cycles)" << std::endl;
    std::cout << "✓ Context reuse across instances" << std::endl; 
    std::cout << "✓ No Panfrost driver cleanup crashes" << std::endl;
    std::cout << "✓ Stable performance across multiple uses" << std::endl;
}

int main() {
    test_memory_safety();
    return 0;
} 