#!/bin/bash

# Mali G31 MP2 Architecture Correction Validation Script
# =====================================================
# This script validates that all architecture corrections have been properly
# implemented for the Mali G31 MP2 GPU (4 ALUs, not 128).

set -e  # Exit on any error

echo "🚀 Mali G31 MP2 Architecture Correction Validation"
echo "================================================="
echo ""

# Check if we're in the correct directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: Must be run from the version1 directory"
    echo "   Current directory: $(pwd)"
    echo "   Expected: .../ode_solver/version1"
    exit 1
fi

# Create build directory
echo "📁 Setting up build environment..."
mkdir -p build
cd build

# Configure with tests enabled
echo "🔧 Configuring CMake with tests enabled..."
cmake -DBUILD_TESTS=ON .. || {
    echo "❌ CMake configuration failed!"
    echo "   Make sure you have the required dependencies:"
    echo "   - EGL development libraries"
    echo "   - OpenGL ES development libraries"
    echo "   - GBM development libraries"
    exit 1
}

# Build the architecture validation test
echo "🔨 Building architecture validation test..."
make test_architecture_correction -j4 || {
    echo "❌ Build failed!"
    echo "   Check compiler errors above."
    exit 1
}

echo "✅ Build successful!"
echo ""

# Check if GPU is available
echo "🔍 Checking GPU availability..."
if [ -c "/dev/dri/renderD128" ]; then
    echo "✅ GPU render node found: /dev/dri/renderD128"
    
    # Check permissions
    if [ -r "/dev/dri/renderD128" ] && [ -w "/dev/dri/renderD128" ]; then
        echo "✅ GPU permissions OK"
        GPU_AVAILABLE=true
    else
        echo "⚠️  GPU permissions insufficient"
        echo "   Run: sudo usermod -a -G video $USER && newgrp video"
        GPU_AVAILABLE=false
    fi
else
    echo "⚠️  GPU render node not found"
    echo "   GPU tests will be skipped"
    GPU_AVAILABLE=false
fi

echo ""

# Run the validation test
echo "🧪 Running architecture correction validation..."
echo "================================================="

if [ "$GPU_AVAILABLE" = true ]; then
    echo "Running with GPU support enabled..."
else
    echo "Running in CPU-only mode (GPU tests will be skipped)..."
fi

echo ""
./test_architecture_correction || {
    echo ""
    echo "❌ ARCHITECTURE VALIDATION FAILED!"
    echo "   Some tests did not pass. Please review the output above."
    echo "   Common issues:"
    echo "   - GPU not available or accessible"
    echo "   - Hardcoded values not properly updated"
    echo "   - Performance expectations not realistic"
    exit 1
}

echo ""
echo "🎉 ARCHITECTURE CORRECTION VALIDATION COMPLETE!"
echo ""
echo "📊 Summary of corrections implemented:"
echo "   ✅ Shader workgroup sizes: 128 → 4"
echo "   ✅ ALU count references: 128 → 4"
echo "   ✅ Workgroup calculations: (n+127)/128 → (n+3)/4"
echo "   ✅ Performance expectations: realistic for 4 ALUs"
echo "   ✅ Memory usage: optimized for 4K load/store cache"
echo "   ✅ Problem sizing: N=4 optimal, N≤16 efficient"
echo ""
echo "💡 Next steps:"
echo "   1. Test on actual Orange Pi Zero 2W hardware"
echo "   2. Measure real power consumption"
echo "   3. Validate performance on target device"
echo "   4. Consider batching multiple small problems"
echo ""
echo "📚 For details, see:"
echo "   - version1/MALI_G31_ARCHITECTURE_CORRECTION.md"
echo "   - version1/GPU_OPTIMAL_SOLVERS.md (updated)"
echo "   - tests/test_architecture_correction.cpp" 