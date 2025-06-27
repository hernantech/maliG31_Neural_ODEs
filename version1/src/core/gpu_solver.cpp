#include "gpu_solver.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>

const std::string compute_shader_source = R"(
#version 310 es
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer StateBuffer {
    float state_data[];
};

layout(std430, binding = 1) buffer ParamBuffer {
    float dt;
    float t_start;
    int n_equations;
    int n_steps_batch;  // Process multiple steps per dispatch
    float lambda;
};

layout(std430, binding = 2) buffer ResultBuffer {
    float all_results[];  // Store ALL timesteps: [step0_eq0, step0_eq1, ..., step1_eq0, step1_eq1, ...]
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uint(n_equations)) return;
    
    // RK45 coefficients  
    const float a21 = 0.2;
    const float a31 = 0.075, a32 = 0.225;
    const float a41 = 0.977778, a42 = -3.733333, a43 = 3.555556;
    const float a51 = 2.952597, a52 = -11.595793, a53 = 9.822893, a54 = -0.290683;
    const float a61 = 2.846275, a62 = -10.757576, a63 = 8.906422, a64 = 0.278409, a65 = -0.273531;
    const float b1 = 0.091146, b3 = 0.449237, b4 = 0.651042, b5 = -0.322376, b6 = 0.130952;
    
    // Load initial state for this equation
    float y = state_data[idx];
    
    // Store initial condition
    all_results[0 * n_equations + int(idx)] = y;
    
    // Integrate multiple steps in GPU without CPU synchronization
    for (int step = 1; step < n_steps_batch; step++) {
        float t_current = t_start + float(step-1) * dt;
        
        // RK45 stages for exponential decay: dy/dt = -lambda * y
        float k1 = dt * (-lambda * y);
        float k2 = dt * (-lambda * (y + a21 * k1));
        float k3 = dt * (-lambda * (y + a31 * k1 + a32 * k2));
        float k4 = dt * (-lambda * (y + a41 * k1 + a42 * k2 + a43 * k3));
        float k5 = dt * (-lambda * (y + a51 * k1 + a52 * k2 + a53 * k3 + a54 * k4));
        float k6 = dt * (-lambda * (y + a61 * k1 + a62 * k2 + a63 * k3 + a64 * k4 + a65 * k5));
        
        // Update state
        y = y + b1 * k1 + b3 * k3 + b4 * k4 + b5 * k5 + b6 * k6;
        
        // Store result for this timestep
        all_results[step * n_equations + int(idx)] = y;
    }
}
)";

GPUSolver::GPUSolver() : dri_fd(-1), gbm(nullptr), display(EGL_NO_DISPLAY), 
                        context(EGL_NO_CONTEXT), program(0), 
                        state_buffer(0), param_buffer(0), initialized(false) {
    if (!initialize_gpu()) {
        std::cerr << "Failed to initialize GPU context" << std::endl;
    }
}

GPUSolver::~GPUSolver() {
    cleanup_gpu();
}

bool GPUSolver::initialize_gpu() {
    // Open DRI device
    dri_fd = open("/dev/dri/renderD128", O_RDWR);
    if (dri_fd < 0) {
        std::cerr << "Failed to open DRI device" << std::endl;
        return false;
    }
    
    // Create GBM device
    gbm = gbm_create_device(dri_fd);
    if (!gbm) {
        std::cerr << "Failed to create GBM device" << std::endl;
        return false;
    }
    
    // Initialize EGL
    display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm, nullptr);
    if (display == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display" << std::endl;
        return false;
    }
    
    if (!eglInitialize(display, nullptr, nullptr)) {
        std::cerr << "Failed to initialize EGL" << std::endl;
        return false;
    }
    
    // Configure EGL
    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
        std::cerr << "Failed to choose EGL config" << std::endl;
        return false;
    }
    
    // Create context
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        std::cerr << "Failed to create EGL context" << std::endl;
        return false;
    }
    
    // Make context current
    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
        std::cerr << "Failed to make EGL context current" << std::endl;
        return false;
    }
    
    // Compile compute shader
    program = compile_compute_shader(compute_shader_source);
    if (program == 0) {
        std::cerr << "Failed to compile compute shader" << std::endl;
        return false;
    }
    
    initialized = true;
    return true;
}

GLuint GPUSolver::compile_compute_shader(const std::string& source) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* src_ptr = source.c_str();
    glShaderSource(shader, 1, &src_ptr, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        std::cerr << "Compute shader compilation failed: " << info_log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, nullptr, info_log);
        std::cerr << "Shader program linking failed: " << info_log << std::endl;
        glDeleteProgram(program);
        glDeleteShader(shader);
        return 0;
    }
    
    glDeleteShader(shader);
    return program;
}

void GPUSolver::solve(const ODESystem& system, 
                     double t0, double tf, double dt,
                     const std::vector<double>& y0,
                     std::vector<std::vector<double>>& solution) {
    
    if (!initialized) {
        std::cerr << "GPU solver not initialized" << std::endl;
        return;
    }
    
    int n_equations = y0.size();
    int n_steps = static_cast<int>((tf - t0) / dt) + 1;
    
    // Check if lambda parameter exists (for exponential decay)
    auto lambda_it = system.parameters.find("lambda");
    if (lambda_it == system.parameters.end()) {
        std::cerr << "GPU solver currently only supports exponential decay problems with lambda parameter" << std::endl;
        return;
    }
    
    // OPTIMIZATION: Do ALL integration steps in a single GPU dispatch
    
    // Convert to float for GPU
    std::vector<float> state_data(n_equations);
    for (int i = 0; i < n_equations; ++i) {
        state_data[i] = static_cast<float>(y0[i]);
    }
    
    // Create GPU buffers
    glGenBuffers(1, &state_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, state_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, n_equations * sizeof(float), 
                 state_data.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, state_buffer);
    
    // Parameter buffer
    struct {
        float dt;
        float t_start;
        int n_equations;
        int n_steps_batch;
        float lambda;
    } params;
    
    params.dt = static_cast<float>(dt);
    params.t_start = static_cast<float>(t0);
    params.n_equations = n_equations;
    params.n_steps_batch = n_steps;
    params.lambda = static_cast<float>(lambda_it->second);
    
    glGenBuffers(1, &param_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(params), &params, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, param_buffer);
    
    // Result buffer - stores ALL timesteps
    GLuint result_buffer;
    size_t result_size = n_steps * n_equations * sizeof(float);
    glGenBuffers(1, &result_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, result_size, nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, result_buffer);
    
    // SINGLE GPU dispatch for ALL integration steps
    glUseProgram(program);
    GLuint work_groups = (n_equations + 63) / 64;
    glDispatchCompute(work_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // SINGLE CPU-GPU transfer at the end
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, result_buffer);
    float* all_results = static_cast<float*>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, result_size, GL_MAP_READ_BIT));
    
    // Convert results back to CPU format
    solution.clear();
    solution.reserve(n_steps);
    
    for (int step = 0; step < n_steps; step++) {
        std::vector<double> step_result(n_equations);
        for (int eq = 0; eq < n_equations; eq++) {
            step_result[eq] = static_cast<double>(all_results[step * n_equations + eq]);
        }
        solution.push_back(step_result);
    }
    
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    
    // Cleanup buffers
    glDeleteBuffers(1, &state_buffer);
    glDeleteBuffers(1, &param_buffer);
    glDeleteBuffers(1, &result_buffer);
}

void GPUSolver::cleanup_gpu() {
    if (program != 0) {
        glDeleteProgram(program);
        program = 0;
    }
    
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    
    if (display != EGL_NO_DISPLAY) {
        eglTerminate(display);
        display = EGL_NO_DISPLAY;
    }
    
    if (gbm) {
        gbm_device_destroy(gbm);
        gbm = nullptr;
    }
    
    if (dri_fd >= 0) {
        close(dri_fd);
        dri_fd = -1;
    }
    
    initialized = false;
} 