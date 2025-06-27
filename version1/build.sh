#!/bin/bash

# RK45 Benchmark Build Script
# Updated for organized directory structure

set -e  # Exit on any error

echo "========================================="
echo "   RK45 GPU vs CPU Benchmark Build"
echo "========================================="

# Configuration
BUILD_TYPE="${1:-Release}"  # Default to Release, accept Debug as argument
BUILD_TESTS="${BUILD_TESTS:-OFF}"  # Set to ON to build test executables
BUILD_DIR="build"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --tests)
            BUILD_TESTS="ON"
            shift
            ;;
        --clean)
            echo "🧹 Cleaning previous build..."
            rm -rf $BUILD_DIR build_debug
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug    Build in Debug mode"
            echo "  --tests    Build test executables"
            echo "  --clean    Clean previous builds"
            echo "  --help     Show this help"
            exit 0
            ;;
        *)
            BUILD_TYPE="$1"
            shift
            ;;
    esac
done

# Create build directory
echo "📁 Creating build directory: $BUILD_DIR"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with CMake
echo "⚙️  Configuring CMake (Build Type: $BUILD_TYPE, Tests: $BUILD_TESTS)..."
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DBUILD_TESTS=$BUILD_TESTS \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo "🔨 Building project..."
make -j$(nproc)

# Copy binaries to bin directory
echo "📦 Installing binaries to ../bin/..."
cd ..
mkdir -p bin

# Copy main executable
if [ -f $BUILD_DIR/rk45_benchmark ]; then
    cp $BUILD_DIR/rk45_benchmark bin/
    echo "✅ Main benchmark: bin/rk45_benchmark"
fi

# Copy performance analysis tool
if [ -f $BUILD_DIR/performance_analysis ]; then
    cp $BUILD_DIR/performance_analysis bin/
    echo "✅ Performance analysis: bin/performance_analysis"
fi

# Copy test executables if built
if [ "$BUILD_TESTS" = "ON" ]; then
    for test_exe in simple_comparison test_memory_safe test_massive_parallel; do
        if [ -f $BUILD_DIR/$test_exe ]; then
            cp $BUILD_DIR/$test_exe bin/
            echo "✅ Test executable: bin/$test_exe"
        fi
    done
fi

echo ""
echo "🎉 Build completed successfully!"
echo ""
echo "📍 Directory structure:"
echo "   bin/           - Compiled executables"
echo "   src/core/      - Core implementation"
echo "   src/experimental/ - GPU variant experiments"
echo "   tests/         - Test source files"
echo "   examples/      - Example programs"
echo ""
echo "🚀 To run:"
echo "   ./bin/rk45_benchmark            # Main benchmark"
echo "   ./bin/performance_analysis      # Performance analysis"
if [ "$BUILD_TESTS" = "ON" ]; then
echo "   ./bin/simple_comparison         # Simple CPU vs GPU test"
echo "   ./bin/test_memory_safe          # Memory-safe GPU test"
echo "   ./bin/test_massive_parallel     # Massive parallel GPU test"
fi
echo "" 