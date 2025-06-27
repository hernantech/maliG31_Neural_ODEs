# Directory Structure Documentation

This document explains the organized structure of the RK45 GPU vs CPU benchmark project.

## 📁 Project Layout

```
version1/
├── 📁 bin/                          # Compiled executables
│   ├── 🔧 rk45_benchmark           # Main benchmark program
│   ├── 📊 performance_analysis     # Performance analysis tool
│   ├── 🧪 simple_comparison        # Simple CPU vs GPU test
│   ├── 🛡️  test_memory_safe         # Memory-safe GPU implementation
│   └── ⚡ test_massive_parallel     # Massive parallel GPU implementation
│
├── 📁 src/                          # Source code
│   ├── 📁 core/                     # Core implementation files
│   │   ├── cpu_solver.cpp           # CPU sequential RK45 solver
│   │   ├── gpu_solver.cpp           # Standard GPU solver
│   │   ├── main.cpp                 # Main benchmark program
│   │   └── test_problems.cpp        # ODE test problem definitions
│   │
│   └── 📁 experimental/             # Experimental GPU implementations
│       ├── gpu_solver_fixed.cpp             # Early fixed version
│       ├── gpu_solver_optimized.cpp         # Single-dispatch optimized
│       ├── gpu_solver_memory_safe.cpp       # Memory-safe with context reuse
│       └── gpu_solver_massively_parallel.cpp # 128-problem parallel version
│
├── 📁 include/                      # Header files
│   ├── solver_base.h                # Abstract solver interface
│   ├── cpu_solver.h                 # CPU solver declarations
│   ├── gpu_solver.h                 # GPU solver declarations
│   ├── test_problems.h              # Test problem definitions
│   └── timer.h                      # High-precision timing utilities
│
├── 📁 tests/                        # Test source files
│   ├── comprehensive_test.cpp       # Comprehensive benchmark test
│   ├── simple_comparison.cpp        # Basic CPU vs GPU comparison
│   └── test_optimized.cpp           # Optimized solver test
│
├── 📁 examples/                     # Example programs
│   └── performance_analysis.cpp     # Performance analysis example
│
├── 📁 data/                         # Data and results
│   └── 📁 results/                  # Benchmark output data
│
├── 📁 build/                        # CMake build directory (Release)
├── 📁 build_debug/                  # CMake build directory (Debug)
├── 📁 scaling_tests/                # Scalability test files
├── 📁 scripts/                      # Utility scripts
│
├── 📄 CMakeLists.txt                # Build configuration
├── 🔧 build.sh                      # Build script with options
├── 📊 run_benchmark.sh              # Automated benchmark runner
└── 📖 README.md                     # Main documentation
```

## 🚀 Building the Project

### Basic Build
```bash
./build.sh                    # Release build, core executables only
```

### Build Options
```bash
./build.sh --debug            # Debug build with symbols
./build.sh --tests            # Include test executables
./build.sh --clean            # Clean previous builds
./build.sh --help             # Show all options
```

### Environment Variables
```bash
BUILD_TESTS=ON ./build.sh     # Enable test builds via environment
```

## 🔧 GPU Solver Variants

### Core Implementation (`src/core/gpu_solver.cpp`)
- Standard GPU implementation with OpenGL ES compute shaders
- Single-dispatch optimization (500 time steps in one GPU call)
- Used by main benchmark program

### Experimental Variants (`src/experimental/`)

| File | Description | Performance | Stability |
|------|-------------|-------------|-----------|
| `gpu_solver_optimized.cpp` | Single-dispatch optimization | 2.8ms/problem | ⚠️ Segfaults |
| `gpu_solver_memory_safe.cpp` | Context reuse, memory safe | 0.5ms/problem | ✅ Stable |
| `gpu_solver_massively_parallel.cpp` | 128 problems in parallel | 0.057ms/problem | ⚠️ Shader issues |
| `gpu_solver_fixed.cpp` | Early working version | 67ms/problem | ✅ Stable |

## 🧪 Test Programs

### Main Benchmark (`bin/rk45_benchmark`)
Comprehensive benchmark suite comparing CPU vs GPU performance across multiple test problems.

### Test Executables (built with `--tests`)
- **`simple_comparison`**: Basic CPU vs GPU timing comparison
- **`test_memory_safe`**: Tests memory-safe GPU implementation
- **`test_massive_parallel`**: Tests 128-problem parallel GPU version

## 📊 Performance Summary

| Implementation | Time/Problem | GPU Utilization | Segfault Risk |
|----------------|--------------|-----------------|---------------|
| CPU Sequential | 3.8ms | N/A | None |
| GPU Original | 67ms | 0.8% | High |
| GPU Optimized | 2.8ms | 0.8% | High |
| GPU Memory Safe | 0.5ms | 0.8% | None |
| GPU Massive Parallel | 0.057ms | 100% | Medium |

## 🔍 Key Architectural Improvements

### 1. **Eliminated Per-Step Synchronization**
- **Before**: 500+ CPU↔GPU round trips per integration
- **After**: Single GPU dispatch for entire integration
- **Result**: 16x GPU performance improvement

### 2. **Memory Safety Through Context Reuse**
- **Issue**: Panfrost driver crashes during rapid context creation/destruction
- **Solution**: Global EGL context reuse pattern
- **Result**: Zero segfaults, 5.6x additional speedup

### 3. **Massive Parallelism**
- **Approach**: Process 128 problems simultaneously
- **Target**: 100% GPU utilization (all 128 ALUs on Mali G31 MP2)
- **Potential**: 50x speedup demonstrated

## 🛠️ Development Workflow

### Adding New GPU Variants
1. Create new file in `src/experimental/`
2. Add to CMakeLists.txt test section
3. Build with `./build.sh --tests`
4. Test with specific executable in `bin/`

### Running Benchmarks
```bash
# Basic comparison
./bin/simple_comparison

# Full benchmark suite
./bin/rk45_benchmark

# Specific implementations
./bin/test_memory_safe
./bin/test_massive_parallel
```

### Performance Analysis
```bash
./bin/performance_analysis > results.txt
```

This organized structure separates concerns clearly:
- 🎯 **Core implementations** for production use
- 🔬 **Experimental variants** for research and optimization
- 🧪 **Test programs** for validation and comparison
- 📦 **Binary organization** for easy execution
- 📊 **Documentation** for understanding and maintenance 