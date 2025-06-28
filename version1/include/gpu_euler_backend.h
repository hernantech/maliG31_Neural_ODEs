#pragma once
#include "solver_base.h"
#include "shader_generator.h"
#include "gpu_buffer_manager.h"
#include "builtin_rhs_registry.h"
#include "gpu_context_manager.h"
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <unordered_map>

class GPUEulerBackend : public SolverBase {
public:
    GPUEulerBackend();
    ~GPUEulerBackend();
    
    void solve(const ODESystem& system, 
              double t0, double tf, double dt,
              const std::vector<double>& y0,
              std::vector<std::vector<double>>& solution) override;
    
    std::string name() const override { return "GPU_Euler"; }

protected:
    GLuint get_or_compile_shader(const ODESystem& system);
    void setup_uniforms(const ODESystem& system, SystemParams& params);

private:
    // Shader and buffer management
    ShaderGenerator shader_gen_;
    GPUBufferManager buffer_mgr_;
    std::unordered_map<std::string, GLuint> shader_cache_;
}; 