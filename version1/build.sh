#!/bin/bash

# Build script for Orange Pi Zero 2W
echo "RK45 Benchmark Build Script"
echo "==========================="

# Check if we're on Orange Pi with Mali GPU
if [ ! -e /dev/dri/renderD128 ]; then
    echo "Warning: Mali GPU render node not found at /dev/dri/renderD128"
    echo "Make sure Panfrost driver is loaded and user is in 'video' group"
fi

# Create build directory
mkdir -p build
cd build

# Configure build
echo "Configuring build..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Check if configuration succeeded
if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed"
    echo "Make sure required libraries are installed:"
    echo "  sudo apt install libegl1-mesa-dev libgles2-mesa-dev libgbm-dev"
    echo "  sudo apt install cmake build-essential pkg-config"
    exit 1
fi

# Build
echo "Building..."
make -j$(nproc)

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Run the benchmark with: ./build/rk45_benchmark"
    echo ""
    echo "System information:"
    echo "GPU: $(glxinfo | grep 'OpenGL renderer' 2>/dev/null || echo 'N/A')"
    echo "Driver: $(glxinfo | grep 'OpenGL version' 2>/dev/null || echo 'N/A')"
else
    echo "Error: Build failed"
    exit 1
fi 