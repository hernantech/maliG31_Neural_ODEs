#pragma once
#include "solver_base.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <gbm.h>

class GPUSolver : public SolverBase {
public:
    GPUSolver();
    ~GPUSolver();
    
    void solve(const ODESystem& system, 
              double t0, double tf, double dt,
              const std::vector<double>& y0,
              std::vector<std::vector<double>>& solution) override;
    
    std::string name() const override { return "GPU_RK45"; }

private:
    bool initialize_gpu();
    void cleanup_gpu();
    GLuint compile_compute_shader(const std::string& source);
    
    // GPU context
    int dri_fd;
    struct gbm_device* gbm;
    EGLDisplay display;
    EGLContext context;
    
    // Compute shader resources
    GLuint program;
    GLuint state_buffer;
    GLuint param_buffer;
    bool initialized;
}; 