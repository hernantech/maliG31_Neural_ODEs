# RK45 GPU vs CPU Benchmark Implementation Specification

## Project Overview

**Objective**: Create a controlled benchmark comparing CPU sequential RK45 ODE solving vs GPU-accelerated RK45 solving using OpenGL ES compute shaders on Mali G31 MP2 hardware.

**Target Hardware**: Orange Pi Zero 2W (Mali G31 MP2, 2 shader cores)

**Language**: C++ with OpenGL ES 3.1 compute shaders

## Standard Test Problems

Based on research into ODE benchmark suites (DETEST, OTP, academic literature), implement the following test cases:

### 1. **Linear Exponential Decay** (Validation Problem)
```
dy/dt = -λy,  y(0) = y0
Analytical: y(t) = y0 * exp(-λt)
```
- **Purpose**: Validation with known analytical solution
- **Parameters**: λ = 2.0, y0 = 1.0, t ∈ [0, 5]
- **Expected**: Simple exponential decay, perfect for accuracy verification

### 2. **Van der Pol Oscillator** (Nonlinear, Limit Cycle)
```
d²x/dt² - μ(1-x²)dx/dt + x = 0
Converted to first-order system:
dx/dt = y
dy/dt = μ(1-x²)y - x
```
- **Purpose**: Nonlinear dynamics with limit cycle behavior
- **Parameters**: μ = 1.0 (mild nonlinearity), initial: x(0) = 2.0, y(0) = 0.0
- **Integration**: t ∈ [0, 20]

### 3. **Lorenz System** (Chaotic, 3D)
```
dx/dt = σ(y - x)
dy/dt = x(ρ - z) - y  
dz/dt = xy - βz
```
- **Purpose**: Chaotic dynamics, 3D system, sensitive to numerical precision
- **Parameters**: σ = 10, ρ = 28, β = 8/3 (classic chaotic parameters)
- **Initial**: (10, 10, 10), t ∈ [0, 20]

### 4. **Brusselator** (Chemical Kinetics, Stiff)
```
dx/dt = A + x²y - Bx - x
dy/dt = Bx - x²y
```
- **Purpose**: Chemical reaction kinetics with potential stiffness
- **Parameters**: A = 1.0, B = 3.0, initial: x(0) = 1.0, y(0) = 1.0
- **Integration**: t ∈ [0, 10]

### 5. **Scalability Test System** (Parallel N-Body Style)
```
For i = 1 to N:
dxi/dt = -xi + sin(xi-1) + ε*xi+1
```
- **Purpose**: Scalability testing with weakly coupled oscillators
- **Parameters**: ε = 0.1, xi(0) = i*0.1, N = {100, 1000, 10000, 100000}
- **Integration**: t ∈ [0, 5]

## File Structure Specification

```
rk45_benchmark/
├── CMakeLists.txt                 # Build configuration
├── README.md                      # Setup and usage instructions
│
├── include/
│   ├── solver_base.h              # Abstract solver interface
│   ├── cpu_solver.h               # CPU sequential RK45 declaration
│   ├── gpu_solver.h               # OpenGL compute shader RK45 declaration
│   ├── test_problems.h            # ODE system definitions
│   ├── benchmark_runner.h         # Performance testing framework
│   └── timer.h                    # High-precision timing utilities
│
├── src/
│   ├── cpu_solver.cpp             # Sequential RK45 implementation
│   ├── gpu_solver.cpp             # OpenGL/EGL setup + compute dispatch
│   ├── test_problems.cpp          # ODE RHS functions for all test cases
│   ├── benchmark_runner.cpp       # Test execution and data collection
│   ├── timer.cpp                  # Timing implementation
│   └── main.cpp                   # Command-line interface
│
├── shaders/
│   └── rk45_compute.glsl          # GPU compute shader implementation
│
├── data/
│   └── results/                   # Performance measurements (CSV output)
│       ├── cpu_results.csv
│       ├── gpu_results.csv
│       └── comparison_report.txt
│
└── scripts/
    ├── plot_results.py            # Python plotting for analysis
    └── run_benchmark.sh           # Automated benchmark execution
```

## Implementation Requirements

### Base Solver Interface (`solver_base.h`)
```cpp
class SolverBase {
public:
    virtual ~SolverBase() = default;
    virtual void solve(const ODESystem& system, 
                      double t0, double tf, double dt,
                      const std::vector<double>& y0,
                      std::vector<std::vector<double>>& solution) = 0;
    virtual std::string name() const = 0;
};
```

### Test Problem Interface (`test_problems.h`)
```cpp
struct ODESystem {
    std::string name;
    int dimension;
    std::function<std::vector<double>(double, const std::vector<double>&)> rhs;
    std::function<std::vector<double>(double)> analytical_solution; // null if none
    std::vector<double> initial_conditions;
    double t_start, t_end;
    std::map<std::string, double> parameters;
};

// Factory functions
ODESystem create_exponential_decay();
ODESystem create_van_der_pol();
ODESystem create_lorenz();
ODESystem create_brusselator();
ODESystem create_scalability_test(int N);
```

### CPU Solver Requirements (`cpu_solver.cpp`)
- **Algorithm**: Fixed-step RK45 (Dormand-Prince coefficients)
- **Implementation**: Pure sequential, no parallelization
- **Memory**: Use `std::vector<double>` for state storage
- **Precision**: Double precision throughout

### GPU Solver Requirements (`gpu_solver.cpp`)
- **Setup**: Use GBM/EGL headless initialization (as per Orange Pi setup)
- **Compute Shader**: One thread per ODE, parallel RK45 stages
- **Memory**: GPU buffer objects for state storage and coefficients
- **Precision**: Float precision (GPU native)
- **Synchronization**: Proper barriers between RK45 stages

### Benchmark Framework (`benchmark_runner.cpp`)
- **Timing**: Exclude data transfer time for GPU (measure compute-only)
- **Validation**: Compare against analytical solutions where available
- **Scaling**: Test with increasing problem sizes: 100, 1K, 10K, 100K ODEs
- **Metrics**: Wall-clock time, solution accuracy (L2 norm error), memory usage

### Performance Metrics
1. **Execution Time**: Total time for integration (excluding setup)
2. **Accuracy**: L2 norm error vs analytical solution (where available)
3. **Throughput**: ODEs solved per second
4. **Crossover Point**: Problem size where GPU becomes faster than CPU
5. **Memory Bandwidth**: Data transfer costs (GPU only)

## Compute Shader Specification (`rk45_compute.glsl`)

### Workgroup Organization
- **Local Size**: Optimize for Mali G31 MP2 (test 32, 64, 128 threads per group)
- **Dispatch**: One thread per ODE equation
- **Shared Memory**: Store RK45 Butcher tableau coefficients

### RK45 Algorithm Structure
```glsl
#version 310 es
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer StateBuffer {
    float state_data[];
};

layout(std430, binding = 1) buffer ParamBuffer {
    float dt;
    float t_current;
    int n_equations;
    int n_steps;
};

// RK45 Butcher tableau as constants
const float a21 = 0.25;
const float a31 = 3.0/32.0, a32 = 9.0/32.0;
// ... (full coefficient set)

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= n_equations) return;
    
    // Load current state
    float y = state_data[idx];
    
    // RK45 stages
    float k1 = dt * rhs_function(t_current, y, idx);
    float k2 = dt * rhs_function(t_current + 0.25*dt, y + a21*k1, idx);
    // ... (complete RK45 stages)
    
    // Update state
    float y_new = y + (35.0/384.0)*k1 + (500.0/1113.0)*k3 + 
                  (125.0/192.0)*k4 - (2187.0/6784.0)*k5 + (11.0/84.0)*k6;
    
    state_data[idx] = y_new;
}
```

## Testing and Validation Protocol

### Phase 1: Correctness Validation
1. **Single ODE Test**: Exponential decay with analytical comparison
2. **Accuracy Threshold**: L2 error < 1e-6 for both CPU and GPU
3. **Consistency Check**: CPU and GPU solutions should agree within 1e-5

### Phase 2: Performance Characterization
1. **Small Scale**: N = 100 ODEs (GPU setup overhead dominates)
2. **Medium Scale**: N = 1,000-10,000 ODEs (transition region)
3. **Large Scale**: N = 100,000 ODEs (GPU should dominate)

### Phase 3: Scalability Analysis
1. **Plot Performance Curves**: Time vs problem size for both solvers
2. **Identify Crossover**: Where GPU becomes faster than CPU
3. **Efficiency Analysis**: ODEs/second and energy consumption

## Expected Deliverables

1. **Working Benchmark Suite**: Compiles and runs on Orange Pi Zero 2W
2. **Performance Data**: CSV files with timing and accuracy results
3. **Analysis Report**: Summary of GPU vs CPU performance characteristics
4. **Visualization**: Python plots showing performance scaling curves
5. **Documentation**: README with build instructions and usage examples

## Build Requirements

### Dependencies
```bash
# Required libraries (already installed on Orange Pi)
sudo apt install libegl1-mesa-dev libgles2-mesa-dev libgbm-dev
sudo apt install cmake build-essential
```

### CMake Configuration
- **Standard**: C++17
- **OpenGL**: Link against EGL, GLESv2, GBM
- **Optimization**: -O3 for CPU solver, proper GPU shader compilation
- **Debug**: Optional debug build with timing diagnostics

## Success Criteria

1. **Correctness**: All test problems solve accurately on both CPU and GPU
2. **Performance**: Clear identification of GPU vs CPU performance trade-offs
3. **Scalability**: Demonstrate GPU advantage at large problem sizes
4. **Reproducibility**: Consistent results across multiple runs
5. **Documentation**: Clear usage instructions and performance analysis

This specification provides a complete roadmap for implementing a controlled RK45 benchmark comparing CPU sequential solving vs GPU parallel solving using standard ODE test problems, specifically designed for the Mali G31 MP2 hardware environment.