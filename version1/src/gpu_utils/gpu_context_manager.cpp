#include "../../include/gpu_context_manager.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

GPUContextManager& GPUContextManager::instance() {
    static GPUContextManager instance;
    return instance;
}

GPUContextManager::GPUContextManager() 
    : initialized_(false), dri_fd_(-1), gbm_(nullptr), 
      display_(EGL_NO_DISPLAY), context_(EGL_NO_CONTEXT) {
}

GPUContextManager::~GPUContextManager() {
    cleanup();
}

bool GPUContextManager::initialize() {
    if (initialized_) {
        return true;  // Already initialized
    }
    
    // Open DRI device
    dri_fd_ = open("/dev/dri/renderD128", O_RDWR);
    if (dri_fd_ < 0) {
        std::cerr << "Failed to open DRI device" << std::endl;
        return false;
    }
    
    // Create GBM device
    gbm_ = gbm_create_device(dri_fd_);
    if (!gbm_) {
        std::cerr << "Failed to create GBM device" << std::endl;
        return false;
    }
    
    // Initialize EGL
    display_ = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm_, nullptr);
    if (display_ == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display" << std::endl;
        return false;
    }
    
    if (!eglInitialize(display_, nullptr, nullptr)) {
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
    if (!eglChooseConfig(display_, config_attribs, &config, 1, &num_configs)) {
        std::cerr << "Failed to choose EGL config" << std::endl;
        return false;
    }
    
    // Create context
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, context_attribs);
    if (context_ == EGL_NO_CONTEXT) {
        std::cerr << "Failed to create EGL context" << std::endl;
        return false;
    }
    
    // Make context current
    if (!eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_)) {
        std::cerr << "Failed to make EGL context current" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "GPU context manager initialized successfully" << std::endl;
    return true;
}

GLuint GPUContextManager::compile_compute_shader(const std::string& source) {
    if (!initialized_) {
        std::cerr << "GPU context not initialized" << std::endl;
        return 0;
    }
    
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

void GPUContextManager::cleanup() {
    if (context_ != EGL_NO_CONTEXT) {
        eglDestroyContext(display_, context_);
        context_ = EGL_NO_CONTEXT;
    }
    
    if (display_ != EGL_NO_DISPLAY) {
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
    
    if (gbm_) {
        gbm_device_destroy(gbm_);
        gbm_ = nullptr;
    }
    
    if (dri_fd_ >= 0) {
        close(dri_fd_);
        dri_fd_ = -1;
    }
    
    initialized_ = false;
} 