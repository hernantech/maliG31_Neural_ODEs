# RK45 CPU vs GPU Benchmark

A performance comparison benchmark for Runge-Kutta 4th/5th order (RK45) ODE solving between CPU sequential execution and GPU parallel execution using OpenGL ES compute shaders on Mali G31 MP2 hardware.

## Target Hardware

- **Device**: Orange Pi Zero 2W
- **SoC**: Allwinner H618 (4x Cortex-A53 @ 1.5GHz)
- **GPU**: Mali G31 MP2 (2 shader cores, Valhall architecture)
- **OS**: Ubuntu 22.04 (headless)
- **Driver**: Panfrost (Mesa 23.2.1+)

## Prerequisites

### System Requirements

1. **Mali GPU driver loaded**:
   ```bash
   # Check if Panfrost is loaded
   lsmod | grep panfrost
   
   # Check GPU render node exists
   ls -la /dev/dri/renderD128
   ```

2. **User permissions**:
   ```bash
   # Add user to video group for GPU access
   sudo usermod -a -G video $USER
   newgrp video
   ```

3. **Required libraries** (already installed):
   ```bash
   sudo apt update
   sudo apt install libegl1-mesa-dev libgles2-mesa-dev libgbm-dev
   sudo apt install cmake build-essential pkg-config
   ```

## Building

### Quick Build
```bash
chmod +x build.sh
./build.sh
```

### Manual Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Running the Benchmark

### Quick Run
```bash
chmod +x run_benchmark.sh
./run_benchmark.sh
```

### Manual Run
```bash
./build/rk45_benchmark
```

## Test Problems

The benchmark includes the following ODE test cases:

### 1. Exponential Decay (Validation)
- **Equation**: `dy/dt = -λy`, `y(0) = 1`
- **Analytical Solution**: `y(t) = e^(-λt)`
- **Purpose**: Accuracy validation with known solution
- **Parameters**: λ = 2.0, t ∈ [0, 5]

### 2. Scalability Test (Parallel Performance)
- **System**: Weakly coupled oscillators
- **Equation**: `dxi/dt = -xi + sin(xi-1) + ε*xi+1`
- **Purpose**: Test GPU parallel advantage
- **Sizes**: N = 100, 1,000, 10,000 equations

## Expected Results

### Performance Characteristics

1. **Small Problems (N < 1000)**:
   - CPU likely faster due to GPU setup overhead
   - GPU data transfer costs dominate

2. **Large Problems (N > 10000)**:
   - GPU should show significant speedup
   - Parallel execution advantages emerge

3. **Crossover Point**:
   - Problem size where GPU becomes faster than CPU
   - Typically around 1,000-5,000 equations for Mali G31 MP2

## File Structure

```
version1/
├── CMakeLists.txt          # Build configuration
├── build.sh               # Build script
├── run_benchmark.sh       # Execution script
├── README.md              # This file
│
├── include/               # Header files
│   ├── solver_base.h      # Abstract solver interface
│   ├── cpu_solver.h       # CPU implementation
│   ├── gpu_solver.h       # GPU implementation
│   ├── test_problems.h    # ODE test cases
│   └── timer.h            # High-precision timing
│
├── src/                   # Source files
│   ├── main.cpp           # Main benchmark program
│   ├── cpu_solver.cpp     # CPU RK45 implementation
│   ├── gpu_solver.cpp     # GPU RK45 + OpenGL setup
│   └── test_problems.cpp  # ODE system definitions
│
└── data/results/          # Benchmark output (generated)
    └── benchmark_*.txt    # Timestamped results
```

## Implementation Details

### CPU Solver
- **Algorithm**: Fixed-step RK45 (Dormand-Prince coefficients)
- **Precision**: Double precision (64-bit)
- **Implementation**: Sequential, single-threaded

### GPU Solver
- **API**: OpenGL ES 3.1 compute shaders
- **Context**: Headless EGL with GBM platform
- **Precision**: Single precision (32-bit)
- **Parallelization**: One thread per ODE equation
- **Work Group Size**: 64 threads (optimized for Mali architecture)

## Troubleshooting

### GPU Not Detected
```bash
# Check if Mali driver is loaded
dmesg | grep -i panfrost

# Verify render node permissions
ls -la /dev/dri/renderD128
groups $USER  # Should include 'video'
```

### Build Errors
```bash
# Check pkg-config can find libraries
pkg-config --libs egl glesv2 gbm
```

### Runtime Errors
```bash
# Check EGL initialization
export MESA_DEBUG=1
export EGL_LOG_LEVEL=debug
./build/rk45_benchmark
``` 