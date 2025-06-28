#pragma once
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <vector>

// Standardized GPU buffer structure
struct StandardGPUBuffers {
    // Buffer 0: State vector (always present)
    GLuint state_buffer;
    
    // Buffer 1: System parameters (standardized)
    GLuint param_buffer;
    
    // Buffer 2: Time series storage (optional)
    GLuint timeseries_buffer;
    
    // Buffer 3: Time control (for multi-step integration)
    GLuint time_control_buffer;
};

// System parameters structure matching shader layout
struct SystemParams {
    float dt;
    float t_current;
    int n_equations;
    float user_uniforms[16];  // Fixed-size user parameter array
};

// Time control structure
struct TimeControl {
    int current_step;
    int total_steps;
};

class GPUBufferManager {
public:
    GPUBufferManager();
    ~GPUBufferManager();
    
    // Buffer allocation and management
    bool allocate_standard_buffers(int n_equations, int n_timesteps, 
                                  const std::vector<float>& initial_state);
    void bind_buffers();
    void update_system_params(const SystemParams& params);
    void update_time_control(const TimeControl& time_ctrl);
    void cleanup();
    
    // Data retrieval
    std::vector<float> read_state_buffer();
    std::vector<float> read_timeseries_buffer(int n_equations, int n_steps);
    
    // Buffer access
    GLuint get_state_buffer() const { return buffers_.state_buffer; }
    GLuint get_param_buffer() const { return buffers_.param_buffer; }
    GLuint get_timeseries_buffer() const { return buffers_.timeseries_buffer; }
    GLuint get_time_control_buffer() const { return buffers_.time_control_buffer; }
    
    bool is_allocated() const { return allocated_; }
    
private:
    StandardGPUBuffers buffers_;
    bool allocated_;
    int n_equations_;
    int n_timesteps_;
    
    void cleanup_buffers();
}; 