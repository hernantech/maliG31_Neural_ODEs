cmake_minimum_required(VERSION 3.10)
project(RK45Benchmark VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Default to Release build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Find OpenGL ES and EGL
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)
pkg_check_modules(GBM REQUIRED gbm)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${EGL_INCLUDE_DIRS})
include_directories(${GLES_INCLUDE_DIRS})
include_directories(${GBM_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/cpu_solver.cpp
    src/gpu_solver.cpp
    src/test_problems.cpp
)

# Create executable
add_executable(rk45_benchmark ${SOURCES})

# Link libraries
target_link_libraries(rk45_benchmark
    ${EGL_LIBRARIES}
    ${GLES_LIBRARIES}
    ${GBM_LIBRARIES}
    -pthread
)

# Compiler definitions
target_compile_definitions(rk45_benchmark PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "EGL libraries: ${EGL_LIBRARIES}")
message(STATUS "GLES libraries: ${GLES_LIBRARIES}")
message(STATUS "GBM libraries: ${GBM_LIBRARIES}") 