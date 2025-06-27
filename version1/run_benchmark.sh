#!/bin/bash

# Benchmark execution script for Orange Pi Zero 2W
echo "RK45 CPU vs GPU Benchmark"
echo "========================="

# Check if executable exists
if [ ! -f "./build/rk45_benchmark" ]; then
    echo "Error: Benchmark executable not found"
    echo "Run ./build.sh first to compile the benchmark"
    exit 1
fi

# Check GPU accessibility
if [ ! -r /dev/dri/renderD128 ]; then
    echo "Warning: Cannot access GPU render node"
    echo "You may need to add your user to the 'video' group:"
    echo "  sudo usermod -a -G video $USER"
    echo "  newgrp video"
fi

# Create results directory
mkdir -p data/results

# System information
echo "System Information:"
echo "==================="
echo "Date: $(date)"
echo "Kernel: $(uname -r)"
echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
echo "Memory: $(grep 'MemTotal' /proc/meminfo | awk '{print $2 " " $3}')"
echo "GPU Driver: $(cat /sys/kernel/debug/dri/0/name 2>/dev/null || echo 'N/A')"
echo ""

# Run benchmark with timing
echo "Starting benchmark..."
echo "Results will be saved to data/results/"
echo ""

# Create timestamped results file
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RESULTS_FILE="data/results/benchmark_${TIMESTAMP}.txt"

# Run benchmark and capture output
{
    echo "RK45 CPU vs GPU Benchmark Results"
    echo "Generated on: $(date)"
    echo "System: Orange Pi Zero 2W (Mali G31 MP2)"
    echo ""
    ./build/rk45_benchmark
} 2>&1 | tee "${RESULTS_FILE}"

echo ""
echo "Benchmark complete!"
echo "Results saved to: ${RESULTS_FILE}" 