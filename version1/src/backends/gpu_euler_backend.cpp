#include "../../include/gpu_euler_backend.h"
#include <iostream>
#include <functional>

GPUEulerBackend::GPUEulerBackend() {
    // GPU context is managed by singleton, no need to initialize here
}

GPUEulerBackend::~GPUEulerBackend() {
    // Clean up cached shaders only
    for (auto& pair : shader_cache_) {
        glDeleteProgram(pair.second);
    }
    shader_cache_.clear();
}

GLuint GPUEulerBackend::get_or_compile_shader(const ODESystem& system) {
    // Generate cache key based on system properties
    std::string cache_key;
    if (system.has_gpu_support() && system.use_builtin_rhs()) {
        cache_key = system.gpu_info->builtin_rhs_name;
    } else if (system.has_gpu_support() && !system.gpu_info->glsl_rhs_code.empty()) {
        // Hash the custom GLSL code
        std::hash<std::string> hasher;
        cache_key = "custom_" + std::to_string(hasher(system.gpu_info->glsl_rhs_code));
    } else {
        std::cerr << "System has no GPU support information" << std::endl;
        return 0;
    }
    
    // Check cache first
    auto it = shader_cache_.find(cache_key);
    if (it != shader_cache_.end()) {
        return it->second;
    }
    
    // Generate shader
    std::string shader_source;
    try {
        if (system.use_builtin_rhs()) {
            shader_source = shader_gen_.generate_euler_shader_builtin(system.gpu_info->builtin_rhs_name);
        } else {
            // Handle custom GLSL code (not implemented in this step)
            std::cerr << "Custom GLSL RHS not yet implemented" << std::endl;
            return 0;
        }
    } catch (const std::exception& e) {
        std::cerr << "Shader generation failed: " << e.what() << std::endl;
        return 0;
    }
    
    // Compile shader
    GLuint program = GPUContextManager::instance().compile_compute_shader(shader_source);
    if (program != 0) {
        shader_cache_[cache_key] = program;
    }
    
    return program;
}

void GPUEulerBackend::setup_uniforms(const ODESystem& system, SystemParams& params) {
    // Clear uniform array
    for (int i = 0; i < 16; ++i) {
        params.user_uniforms[i] = 0.0f;
    }
    
    if (system.has_gpu_support() && !system.gpu_info->gpu_uniforms.empty()) {
        // Use GPU-specific uniforms
        for (size_t i = 0; i < std::min(system.gpu_info->gpu_uniforms.size(), size_t(16)); ++i) {
            params.user_uniforms[i] = system.gpu_info->gpu_uniforms[i];
        }
    } else {
        // Fallback: try to extract from parameters map
        if (system.use_builtin_rhs()) {
            auto& registry = BuiltinRHSRegistry::instance();
            auto rhs_def = registry.get_rhs(system.gpu_info->builtin_rhs_name);
            
            for (size_t i = 0; i < rhs_def.uniform_names.size() && i < 16; ++i) {
                const std::string& uniform_name = rhs_def.uniform_names[i];
                auto param_it = system.parameters.find(uniform_name);
                if (param_it != system.parameters.end()) {
                    params.user_uniforms[i] = static_cast<float>(param_it->second);
                }
            }
        }
    }
}

void GPUEulerBackend::solve(const ODESystem& system, 
                           double t0, double tf, double dt,
                           const std::vector<double>& y0,
                           std::vector<std::vector<double>>& solution) {
    
    // Initialize GPU context using singleton
    if (!GPUContextManager::instance().initialize()) {
        std::cerr << "Failed to initialize GPU context" << std::endl;
        return;
    }
    
    if (!system.has_gpu_support()) {
        std::cerr << "System does not have GPU support information" << std::endl;
        return;
    }
    
    int n_equations = y0.size();
    int n_steps = static_cast<int>((tf - t0) / dt) + 1;
    
    std::cout << "GPU Euler: Solving " << n_equations << " equations for " 
              << n_steps << " steps" << std::endl;
    
    // Get or compile shader
    GLuint program = get_or_compile_shader(system);
    if (program == 0) {
        std::cerr << "Failed to get shader program" << std::endl;
        return;
    }
    
    // Convert initial conditions to float
    std::vector<float> initial_state(n_equations);
    for (int i = 0; i < n_equations; ++i) {
        initial_state[i] = static_cast<float>(y0[i]);
    }
    
    // Allocate GPU buffers
    if (!buffer_mgr_.allocate_standard_buffers(n_equations, n_steps, initial_state)) {
        std::cerr << "Failed to allocate GPU buffers" << std::endl;
        return;
    }
    
    // Setup system parameters
    SystemParams params;
    params.dt = static_cast<float>(dt);
    params.n_equations = n_equations;
    setup_uniforms(system, params);
    
    // Time control
    TimeControl time_ctrl;
    time_ctrl.total_steps = n_steps;
    
    // Use program and bind buffers
    glUseProgram(program);
    buffer_mgr_.bind_buffers();
    
    // Integration loop
    solution.clear();
    solution.reserve(n_steps);
    
    for (int step = 0; step < n_steps; ++step) {
        params.t_current = static_cast<float>(t0 + step * dt);
        time_ctrl.current_step = step;
        
        // Update GPU parameters
        buffer_mgr_.update_system_params(params);
        buffer_mgr_.update_time_control(time_ctrl);
        
        // Dispatch compute shader - Mali G31 MP2 has 4 ALUs
        GLuint work_groups = (n_equations + 3) / 4;  // 4 threads per work group
        glDispatchCompute(work_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Read back current state
        auto current_state = buffer_mgr_.read_state_buffer();
        
        // Convert to double and store
        std::vector<double> step_solution(n_equations);
        for (int i = 0; i < n_equations; ++i) {
            step_solution[i] = static_cast<double>(current_state[i]);
        }
        solution.push_back(step_solution);
    }
    
    std::cout << "GPU Euler: Integration completed successfully" << std::endl;
}

 