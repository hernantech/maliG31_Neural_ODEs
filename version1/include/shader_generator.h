#pragma once
#include <string>
#include <vector>
#include <map>
#include "builtin_rhs_registry.h"

class ShaderGenerator {
public:
    ShaderGenerator();
    
    std::string generate_euler_shader(const RHSDefinition& rhs);
    std::string generate_rk45_shader(const RHSDefinition& rhs);
    
    // Generate shader from builtin RHS name
    std::string generate_euler_shader_builtin(const std::string& rhs_name);
    
private:
    std::string load_template(const std::string& template_name);
    std::string substitute_rhs(const std::string& template_code, 
                              const RHSDefinition& rhs);
    std::string generate_uniform_declarations(const std::vector<std::string>& uniform_names);
    
    std::string template_path_;
}; 