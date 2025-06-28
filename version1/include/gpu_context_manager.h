#pragma once
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <gbm.h>
#include <string>

// Singleton GPU context manager to avoid Panfrost driver issues
class GPUContextManager {
public:
    static GPUContextManager& instance();
    
    bool initialize();
    bool is_initialized() const { return initialized_; }
    
    GLuint compile_compute_shader(const std::string& source);
    
    // Prevent copying
    GPUContextManager(const GPUContextManager&) = delete;
    GPUContextManager& operator=(const GPUContextManager&) = delete;

private:
    GPUContextManager();
    ~GPUContextManager();
    
    bool initialized_;
    int dri_fd_;
    struct gbm_device* gbm_;
    EGLDisplay display_;
    EGLContext context_;
    
    void cleanup();
}; 