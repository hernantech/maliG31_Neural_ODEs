#pragma once
#include <string>
#include <vector>
#include <map>

struct RHSDefinition {
    std::string glsl_code;
    std::vector<std::string> uniform_names;
    int problem_type_id;
    std::string description;
};

class BuiltinRHSRegistry {
public:
    static BuiltinRHSRegistry& instance();
    
    void register_rhs(const std::string& name, 
                      const RHSDefinition& definition);
    
    RHSDefinition get_rhs(const std::string& name);
    std::vector<std::string> list_available();
    bool has_rhs(const std::string& name);
    
private:
    BuiltinRHSRegistry();  // Private constructor for singleton
    std::map<std::string, RHSDefinition> registry_;
    
    void register_builtin_systems();  // Register standard systems
}; 