cmake_minimum_required(VERSION 3.16)
project(RK45_Benchmark)

set(CMAKE_CXX_STANDARD 17)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)
pkg_check_modules(GBM REQUIRED gbm)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(include)

# Source files from organized structure
set(CORE_SOURCES
    src/core/cpu_solver.cpp
    src/core/gpu_solver.cpp
    src/core/test_problems.cpp
    src/core/main.cpp
)

# New architecture sources
set(STEPPER_SOURCES
    src/steppers/explicit_euler.cpp
    src/steppers/rk45.cpp
    src/steppers/stepper_factory.cpp
)

set(GPU_UTIL_SOURCES
    src/gpu_utils/builtin_rhs_registry.cpp
    src/gpu_utils/shader_generator.cpp
    src/gpu_utils/gpu_buffer_manager.cpp
    src/gpu_utils/gpu_context_manager.cpp
)

set(BACKEND_SOURCES
    src/backends/cpu_backend.cpp
    src/backends/gpu_euler_backend.cpp
)

# Main benchmark executable
add_executable(rk45_benchmark ${CORE_SOURCES})

# Link libraries
target_link_libraries(rk45_benchmark
    ${EGL_LIBRARIES}
    ${GLES_LIBRARIES}
    ${GBM_LIBRARIES}
)

# Include directories for libraries
target_include_directories(rk45_benchmark PRIVATE
    ${EGL_INCLUDE_DIRS}
    ${GLES_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
)

# Performance analysis tool
add_executable(performance_analysis examples/performance_analysis.cpp src/core/cpu_solver.cpp src/core/test_problems.cpp)

# Test executables (optional builds)
option(BUILD_TESTS "Build test executables" OFF)

if(BUILD_TESTS)
    # Simple comparison test
    add_executable(simple_comparison 
        tests/simple_comparison.cpp 
        src/core/cpu_solver.cpp 
        src/core/gpu_solver.cpp 
        src/core/test_problems.cpp
    )
    target_link_libraries(simple_comparison ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(simple_comparison PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # Memory safe test
    add_executable(test_memory_safe 
        src/experimental/gpu_solver_memory_safe.cpp 
        src/core/cpu_solver.cpp 
        src/core/gpu_solver.cpp 
        src/core/test_problems.cpp
    )
    target_link_libraries(test_memory_safe ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(test_memory_safe PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # Massive parallel test
    add_executable(test_massive_parallel 
        src/experimental/gpu_solver_massively_parallel.cpp 
        src/core/cpu_solver.cpp 
        src/core/gpu_solver.cpp 
        src/core/test_problems.cpp
    )
    target_link_libraries(test_massive_parallel ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(test_massive_parallel PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # GPU-optimal solver tests
    add_executable(euler_massively_parallel 
        src/experimental/gpu_solver_euler_massively_parallel.cpp 
        src/core/cpu_solver.cpp 
        src/core/gpu_solver.cpp 
        src/core/test_problems.cpp
    )
    target_link_libraries(euler_massively_parallel ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(euler_massively_parallel PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    add_executable(leapfrog_physics 
        src/experimental/gpu_solver_leapfrog.cpp 
        src/core/cpu_solver.cpp 
        src/core/gpu_solver.cpp 
        src/core/test_problems.cpp
    )
    target_link_libraries(leapfrog_physics ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(leapfrog_physics PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    add_executable(gpu_solver_comparison 
        tests/gpu_solver_comparison.cpp 
        src/core/cpu_solver.cpp 
        src/core/gpu_solver.cpp 
        src/core/test_problems.cpp
    )
    target_link_libraries(gpu_solver_comparison ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(gpu_solver_comparison PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # Test new architecture
    add_executable(test_new_architecture 
        tests/test_new_architecture.cpp 
        src/core/test_problems.cpp
        ${STEPPER_SOURCES}
        ${GPU_UTIL_SOURCES}
    )
    target_link_libraries(test_new_architecture ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(test_new_architecture PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # Show generated shader
    add_executable(show_generated_shader 
        tests/show_generated_shader.cpp 
        ${GPU_UTIL_SOURCES}
    )
    target_link_libraries(show_generated_shader ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(show_generated_shader PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # Test Van der Pol
    add_executable(test_vanderpol 
        tests/test_vanderpol.cpp 
        src/core/test_problems.cpp
        ${STEPPER_SOURCES}
        ${GPU_UTIL_SOURCES}
    )
    target_link_libraries(test_vanderpol ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(test_vanderpol PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # GPU vs CPU comprehensive test
    add_executable(test_gpu_vs_cpu 
        tests/test_gpu_vs_cpu.cpp 
        src/core/test_problems.cpp
        ${STEPPER_SOURCES}
        ${GPU_UTIL_SOURCES}
        ${BACKEND_SOURCES}
    )
    target_link_libraries(test_gpu_vs_cpu ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(test_gpu_vs_cpu PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # Simple GPU test
    add_executable(test_gpu_simple 
        tests/test_gpu_simple.cpp 
        src/core/test_problems.cpp
        ${GPU_UTIL_SOURCES}
        ${BACKEND_SOURCES}
    )
    target_link_libraries(test_gpu_simple ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(test_gpu_simple PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
    
    # Architecture correction validation test
    add_executable(test_architecture_correction 
        tests/test_architecture_correction.cpp 
        src/core/test_problems.cpp
        ${STEPPER_SOURCES}
        ${GPU_UTIL_SOURCES}
        ${BACKEND_SOURCES}
    )
    target_link_libraries(test_architecture_correction ${EGL_LIBRARIES} ${GLES_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(test_architecture_correction PRIVATE ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
endif()

# Install targets to bin directory
install(TARGETS rk45_benchmark DESTINATION bin)
install(TARGETS performance_analysis DESTINATION bin)

# Copy shader templates to build directory
file(COPY shaders/ DESTINATION ${CMAKE_BINARY_DIR}/shaders/)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "EGL libraries: ${EGL_LIBRARIES}")
message(STATUS "GLES libraries: ${GLES_LIBRARIES}")
message(STATUS "GBM libraries: ${GBM_LIBRARIES}") 