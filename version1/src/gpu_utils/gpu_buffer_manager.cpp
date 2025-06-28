#include "../../include/gpu_buffer_manager.h"
#include <iostream>
#include <cstring>

GPUBufferManager::GPUBufferManager() : allocated_(false), n_equations_(0), n_timesteps_(0) {
    buffers_.state_buffer = 0;
    buffers_.param_buffer = 0;
    buffers_.timeseries_buffer = 0;
    buffers_.time_control_buffer = 0;
}

GPUBufferManager::~GPUBufferManager() {
    cleanup();
}

bool GPUBufferManager::allocate_standard_buffers(int n_equations, int n_timesteps, 
                                                const std::vector<float>& initial_state) {
    if (allocated_) {
        cleanup_buffers();
    }
    
    n_equations_ = n_equations;
    n_timesteps_ = n_timesteps;
    
    // Buffer 0: State buffer
    glGenBuffers(1, &buffers_.state_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.state_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, n_equations * sizeof(float),
                 initial_state.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers_.state_buffer);
    
    // Buffer 1: Parameter buffer
    glGenBuffers(1, &buffers_.param_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.param_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(SystemParams), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers_.param_buffer);
    
    // Buffer 2: Time series buffer (optional, for storing full trajectory)
    if (n_timesteps > 1) {
        size_t timeseries_size = n_timesteps * n_equations * sizeof(float);
        glGenBuffers(1, &buffers_.timeseries_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.timeseries_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, timeseries_size, nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffers_.timeseries_buffer);
    }
    
    // Buffer 3: Time control buffer
    glGenBuffers(1, &buffers_.time_control_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.time_control_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(TimeControl), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buffers_.time_control_buffer);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error during buffer allocation: " << error << std::endl;
        cleanup_buffers();
        return false;
    }
    
    allocated_ = true;
    return true;
}

void GPUBufferManager::bind_buffers() {
    if (!allocated_) return;
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers_.state_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers_.param_buffer);
    if (buffers_.timeseries_buffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffers_.timeseries_buffer);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buffers_.time_control_buffer);
}

void GPUBufferManager::update_system_params(const SystemParams& params) {
    if (!allocated_) return;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.param_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(SystemParams), &params);
}

void GPUBufferManager::update_time_control(const TimeControl& time_ctrl) {
    if (!allocated_) return;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.time_control_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(TimeControl), &time_ctrl);
}

std::vector<float> GPUBufferManager::read_state_buffer() {
    if (!allocated_) return {};
    
    std::vector<float> result(n_equations_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.state_buffer);
    
    float* data = static_cast<float*>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, n_equations_ * sizeof(float), GL_MAP_READ_BIT));
    
    if (data) {
        std::memcpy(result.data(), data, n_equations_ * sizeof(float));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    
    return result;
}

std::vector<float> GPUBufferManager::read_timeseries_buffer(int n_equations, int n_steps) {
    if (!allocated_ || buffers_.timeseries_buffer == 0) return {};
    
    size_t total_size = n_equations * n_steps;
    std::vector<float> result(total_size);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_.timeseries_buffer);
    
    float* data = static_cast<float*>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, total_size * sizeof(float), GL_MAP_READ_BIT));
    
    if (data) {
        std::memcpy(result.data(), data, total_size * sizeof(float));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    
    return result;
}

void GPUBufferManager::cleanup() {
    cleanup_buffers();
    allocated_ = false;
}

void GPUBufferManager::cleanup_buffers() {
    if (buffers_.state_buffer != 0) {
        glDeleteBuffers(1, &buffers_.state_buffer);
        buffers_.state_buffer = 0;
    }
    if (buffers_.param_buffer != 0) {
        glDeleteBuffers(1, &buffers_.param_buffer);
        buffers_.param_buffer = 0;
    }
    if (buffers_.timeseries_buffer != 0) {
        glDeleteBuffers(1, &buffers_.timeseries_buffer);
        buffers_.timeseries_buffer = 0;
    }
    if (buffers_.time_control_buffer != 0) {
        glDeleteBuffers(1, &buffers_.time_control_buffer);
        buffers_.time_control_buffer = 0;
    }
} 