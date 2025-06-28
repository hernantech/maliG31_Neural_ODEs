#include <iostream>
#include "../include/shader_generator.h"

int main() {
    try {
        ShaderGenerator gen;
        std::string shader = gen.generate_euler_shader_builtin("exponential");
        
        std::cout << "=== GENERATED EULER SHADER FOR EXPONENTIAL DECAY ===" << std::endl;
        std::cout << shader << std::endl;
        std::cout << "=== END SHADER ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 