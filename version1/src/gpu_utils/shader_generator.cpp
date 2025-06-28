#include "../../include/shader_generator.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

ShaderGenerator::ShaderGenerator() {
    // Set template path relative to binary location
    template_path_ = "shaders/templates/";
}

std::string ShaderGenerator::generate_euler_shader(const RHSDefinition& rhs) {
    std::string template_code = load_template("euler_template.glsl");
    return substitute_rhs(template_code, rhs);
}

std::string ShaderGenerator::generate_rk45_shader(const RHSDefinition& rhs) {
    // For now, just return Euler - RK45 template would be more complex
    return generate_euler_shader(rhs);
}

std::string ShaderGenerator::generate_euler_shader_builtin(const std::string& rhs_name) {
    auto& registry = BuiltinRHSRegistry::instance();
    RHSDefinition rhs = registry.get_rhs(rhs_name);
    return generate_euler_shader(rhs);
}

std::string ShaderGenerator::load_template(const std::string& template_name) {
    std::string full_path = template_path_ + template_name;
    std::ifstream file(full_path);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader template: " + full_path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ShaderGenerator::substitute_rhs(const std::string& template_code, 
                                           const RHSDefinition& rhs) {
    std::string result = template_code;
    
    // Generate uniform declarations
    std::string uniform_decls = generate_uniform_declarations(rhs.uniform_names);
    
    // Replace template placeholders
    size_t pos = result.find("{{USER_UNIFORMS}}");
    if (pos != std::string::npos) {
        result.replace(pos, 17, uniform_decls);  // 17 = length of "{{USER_UNIFORMS}}"
    }
    
    pos = result.find("{{RHS_FUNCTION}}");
    if (pos != std::string::npos) {
        result.replace(pos, 16, rhs.glsl_code);  // 16 = length of "{{RHS_FUNCTION}}"
    }
    
    return result;
}

std::string ShaderGenerator::generate_uniform_declarations(const std::vector<std::string>& uniform_names) {
    std::stringstream ss;
    for (const auto& name : uniform_names) {
        ss << "    float " << name << ";\n";
    }
    return ss.str();
} 