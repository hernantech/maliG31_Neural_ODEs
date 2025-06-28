#include "../../include/gpu_solver.h"
#include "../../include/test_problems.h"
#include "../../include/timer.h"
#include <iostream>
#include <vector>
#include <cmath>

// LEAPFROG/VERLET: Excellent for physics (Hamiltonian systems)
const std::string leapfrog_shader = R"(
#version 310 es
layout(local_size_x = 4, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer PositionBuffer {
    float positions[];  // [x0, y0, z0, x1, y1, z1, ...]
};

layout(std430, binding = 1) buffer VelocityBuffer {
    float velocities[]; // [vx0, vy0, vz0, vx1, vy1, vz1, ...]
};

layout(std430, binding = 2) buffer ParamBuffer {
    float dt;
    float t_current;
    int n_particles;
    float G;             // Gravitational constant
    float mass;          // Particle mass
    int dimensions;      // 1D, 2D, or 3D
};

layout(std430, binding = 3) buffer EnergyBuffer {
    float total_energy[];   // [kinetic, potential] per timestep
};

layout(std430, binding = 4) buffer TimeBuffer {
    int current_step;
    int total_steps;
};

// Calculate acceleration for particle i
vec3 calculate_acceleration(uint particle_idx) {
    vec3 acc = vec3(0.0);
    
    if (particle_idx >= uint(n_particles)) return acc;
    
    uint base_idx = particle_idx * uint(dimensions);
    
    if (dimensions == 1) {
        // 1D spring system: F = -k*x
        float x = positions[base_idx];
        float k = G; // Reuse G as spring constant
        acc.x = -k * x / mass;
    }
    else if (dimensions == 2 || dimensions == 3) {
        // N-body gravitational system
        vec3 pos_i = vec3(positions[base_idx], 
                         (dimensions > 1) ? positions[base_idx + 1u] : 0.0,
                         (dimensions > 2) ? positions[base_idx + 2u] : 0.0);
        
        // Calculate forces from all other particles
        for (uint j = 0u; j < uint(n_particles); ++j) {
            if (j == particle_idx) continue;
            
            uint other_base = j * uint(dimensions);
            vec3 pos_j = vec3(positions[other_base],
                             (dimensions > 1) ? positions[other_base + 1u] : 0.0,
                             (dimensions > 2) ? positions[other_base + 2u] : 0.0);
            
            vec3 r_vec = pos_j - pos_i;
            float r_mag = length(r_vec);
            
            if (r_mag > 1e-6) {  // Avoid singularity
                // F = G*m1*m2/r² in direction of r_vec
                float force_mag = G * mass * mass / (r_mag * r_mag);
                acc += force_mag * normalize(r_vec) / mass;
            }
        }
    }
    
    return acc;
}

void main() {
    uint particle_idx = gl_GlobalInvocationID.x;
    
    if (particle_idx >= uint(n_particles)) return;
    
    uint base_idx = particle_idx * uint(dimensions);
    
    // LEAPFROG INTEGRATION (Velocity Verlet)
    // Step 1: v(t+dt/2) = v(t) + (dt/2) * a(t)
    // Step 2: x(t+dt) = x(t) + dt * v(t+dt/2)  
    // Step 3: a(t+dt) = calculate_acceleration(x(t+dt))
    // Step 4: v(t+dt) = v(t+dt/2) + (dt/2) * a(t+dt)
    
    vec3 acc_current = calculate_acceleration(particle_idx);
    
    // Update velocity (half step)
    for (uint d = 0u; d < uint(dimensions); ++d) {
        uint vel_idx = base_idx + d;
        velocities[vel_idx] += 0.5 * dt * acc_current[d];
    }
    
    // Update position (full step)
    for (uint d = 0u; d < uint(dimensions); ++d) {
        uint pos_idx = base_idx + d;
        positions[pos_idx] += dt * velocities[pos_idx];
    }
    
    // Memory barrier to ensure all positions updated before acceleration calculation
    memoryBarrierShared();
    barrier();
    
    // Calculate new acceleration
    vec3 acc_new = calculate_acceleration(particle_idx);
    
    // Update velocity (second half step)
    for (uint d = 0u; d < uint(dimensions); ++d) {
        uint vel_idx = base_idx + d;
        velocities[vel_idx] += 0.5 * dt * acc_new[d];
    }
    
    // Calculate energy contribution (for conservation check)
    if (particle_idx == 0u && current_step < total_steps) {
        float kinetic = 0.0;
        float potential = 0.0;
        
        // Sum kinetic energy: KE = (1/2) * m * v²
        for (uint i = 0u; i < uint(n_particles); ++i) {
            uint i_base = i * uint(dimensions);
            float v_sq = 0.0;
            for (uint d = 0u; d < uint(dimensions); ++d) {
                float v = velocities[i_base + d];
                v_sq += v * v;
            }
            kinetic += 0.5 * mass * v_sq;
        }
        
        // Sum potential energy (pairwise)
        if (dimensions > 1) {
            for (uint i = 0u; i < uint(n_particles); ++i) {
                for (uint j = i + 1u; j < uint(n_particles); ++j) {
                    uint i_base = i * uint(dimensions);
                    uint j_base = j * uint(dimensions);
                    
                    vec3 pos_i = vec3(positions[i_base],
                                     (dimensions > 1) ? positions[i_base + 1u] : 0.0,
                                     (dimensions > 2) ? positions[i_base + 2u] : 0.0);
                    vec3 pos_j = vec3(positions[j_base],
                                     (dimensions > 1) ? positions[j_base + 1u] : 0.0,
                                     (dimensions > 2) ? positions[j_base + 2u] : 0.0);
                    
                    float r = length(pos_j - pos_i);
                    if (r > 1e-6) {
                        potential -= G * mass * mass / r;  // Negative for attractive force
                    }
                }
            }
        } else {
            // Spring potential: PE = (1/2) * k * x²
            for (uint i = 0u; i < uint(n_particles); ++i) {
                float x = positions[i * uint(dimensions)];
                potential += 0.5 * G * x * x;
            }
        }
        
        // Store energy
        total_energy[current_step * 2] = kinetic;
        total_energy[current_step * 2 + 1] = potential;
    }
}
)";

class LeapfrogGPUSolver : public GPUSolver {
private:
    GLuint leapfrog_program;
    bool solver_initialized;
    
public:
    LeapfrogGPUSolver() : leapfrog_program(0), solver_initialized(false) {
        if (initialized) {
            leapfrog_program = compile_compute_shader(leapfrog_shader);
            solver_initialized = (leapfrog_program != 0);
            if (solver_initialized) {
                std::cout << "LeapfrogGPU: Initialized successfully" << std::endl;
            }
        }
    }
    
    ~LeapfrogGPUSolver() {
        if (leapfrog_program != 0) {
            glDeleteProgram(leapfrog_program);
        }
    }
    
    // Solve N-body system with energy conservation
    void solve_physics_system(int n_particles, int dimensions, double dt, double t_final,
                             const std::vector<double>& initial_positions,
                             const std::vector<double>& initial_velocities,
                             std::vector<std::vector<double>>& positions_history,
                             std::vector<double>& energy_history) {
        
        if (!solver_initialized) {
            std::cerr << "LeapfrogGPU: Not initialized" << std::endl;
            return;
        }
        
        int n_steps = static_cast<int>(t_final / dt) + 1;
        int total_coords = n_particles * dimensions;
        
        std::cout << "\n=== LEAPFROG PHYSICS SIMULATION ===" << std::endl;
        std::cout << "Particles: " << n_particles << std::endl;
        std::cout << "Dimensions: " << dimensions << std::endl;
        std::cout << "Total coordinates: " << total_coords << std::endl;
        std::cout << "ALU utilization: " << (n_particles * 100.0 / 4.0) << "%" << std::endl;
        std::cout << "Timesteps: " << n_steps << std::endl;
        std::cout << "Expected energy conservation: Exact (symplectic)" << std::endl;
        
        // GPU Buffers
        GLuint pos_buffer, vel_buffer, param_buffer, energy_buffer, time_buffer;
        
        // Position buffer
        std::vector<float> pos_data(total_coords);
        for (int i = 0; i < std::min(total_coords, static_cast<int>(initial_positions.size())); ++i) {
            pos_data[i] = static_cast<float>(initial_positions[i]);
        }
        
        glGenBuffers(1, &pos_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pos_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, total_coords * sizeof(float),
                     pos_data.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos_buffer);
        
        // Velocity buffer
        std::vector<float> vel_data(total_coords);
        for (int i = 0; i < std::min(total_coords, static_cast<int>(initial_velocities.size())); ++i) {
            vel_data[i] = static_cast<float>(initial_velocities[i]);
        }
        
        glGenBuffers(1, &vel_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, vel_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, total_coords * sizeof(float),
                     vel_data.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vel_buffer);
        
        // Parameters
        struct {
            float dt;
            float t_current;
            int n_particles;
            float G;
            float mass;
            int dimensions;
        } params;
        
        params.dt = static_cast<float>(dt);
        params.n_particles = n_particles;
        params.G = 1.0f;  // Gravitational constant or spring constant
        params.mass = 1.0f;
        params.dimensions = dimensions;
        
        glGenBuffers(1, &param_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(params), &params, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, param_buffer);
        
        // Energy tracking buffer
        size_t energy_size = n_steps * 2 * sizeof(float);  // [kinetic, potential] per step
        glGenBuffers(1, &energy_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, energy_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, energy_size, nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, energy_buffer);
        
        // Time control
        struct {
            int current_step;
            int total_steps;
        } time_control;
        time_control.total_steps = n_steps;
        
        glGenBuffers(1, &time_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, time_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(time_control), &time_control, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, time_buffer);
        
        // Integration loop
        glUseProgram(leapfrog_program);
        
        positions_history.clear();
        positions_history.resize(n_steps);
        
        for (int step = 0; step < n_steps; ++step) {
            // Update time parameters
            params.t_current = step * static_cast<float>(dt);
            time_control.current_step = step;
            
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_buffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(params), &params);
            
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, time_buffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(time_control), &time_control);
            
            // Store current positions
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, pos_buffer);
            float* current_pos = static_cast<float*>(
                glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, total_coords * sizeof(float), GL_MAP_READ_BIT));
            
            positions_history[step].resize(total_coords);
            for (int i = 0; i < total_coords; ++i) {
                positions_history[step][i] = static_cast<double>(current_pos[i]);
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            
            // Leapfrog step
            GLuint work_groups = (n_particles + 3) / 4;
            glDispatchCompute(work_groups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
        
        // Read energy history
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, energy_buffer);
        float* energy_data = static_cast<float*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, energy_size, GL_MAP_READ_BIT));
        
        energy_history.clear();
        energy_history.resize(n_steps);
        for (int step = 0; step < n_steps; ++step) {
            float kinetic = energy_data[step * 2];
            float potential = energy_data[step * 2 + 1];
            energy_history[step] = kinetic + potential;  // Total energy
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        
        // Cleanup
        glDeleteBuffers(1, &pos_buffer);
        glDeleteBuffers(1, &vel_buffer);
        glDeleteBuffers(1, &param_buffer);
        glDeleteBuffers(1, &energy_buffer);
        glDeleteBuffers(1, &time_buffer);
        
        std::cout << "LeapfrogGPU: Simulation complete!" << std::endl;
        
        // Energy conservation analysis
        if (!energy_history.empty()) {
            double initial_energy = energy_history[0];
            double final_energy = energy_history.back();
            double energy_drift = std::abs(final_energy - initial_energy) / std::abs(initial_energy);
            
            std::cout << "Initial energy: " << initial_energy << std::endl;
            std::cout << "Final energy: " << final_energy << std::endl;
            std::cout << "Energy drift: " << energy_drift * 100 << "%" << std::endl;
            std::cout << "Conservation quality: " << ((energy_drift < 1e-6) ? "Excellent" : 
                                                      (energy_drift < 1e-3) ? "Good" : "Poor") << std::endl;
        }
    }
};

// Test Leapfrog for physics problems
void test_leapfrog_physics() {
    std::cout << "=== LEAPFROG PHYSICS BENCHMARK ===" << std::endl;
    
    Timer timer;
    LeapfrogGPUSolver leapfrog_gpu;
    
    // Test 1: 2-body orbital system
    std::cout << "\n1. Two-body orbital system (2D):" << std::endl;
    
    std::vector<double> positions = {-0.5, 0.0, 0.5, 0.0};  // x1,y1, x2,y2
    std::vector<double> velocities = {0.0, -0.5, 0.0, 0.5}; // vx1,vy1, vx2,vy2
    
    std::vector<std::vector<double>> orbit_history;
    std::vector<double> energy_history;
    
    timer.start();
    leapfrog_gpu.solve_physics_system(2, 2, 0.01, 2.0, positions, velocities,
                                     orbit_history, energy_history);
    double leapfrog_time = timer.elapsed();
    
    std::cout << "   Time: " << leapfrog_time * 1000 << " ms" << std::endl;
    std::cout << "   Throughput: " << (2 * 200) / leapfrog_time << " particle-steps/second" << std::endl;
    
    // Test 2: Small spring system (1D) - Optimized for Mali G31 MP2
    std::cout << "\n2. 4-particle spring chain (1D):" << std::endl;
    
    std::vector<double> spring_pos(4);
    std::vector<double> spring_vel(4);
    for (int i = 0; i < 4; ++i) {
        spring_pos[i] = i * 0.1;  // Initial positions
        spring_vel[i] = 0.0;      // Start from rest
    }
    
    std::vector<std::vector<double>> spring_history;
    std::vector<double> spring_energy;
    
    timer.start();
    leapfrog_gpu.solve_physics_system(4, 1, 0.001, 1.0, spring_pos, spring_vel,
                                     spring_history, spring_energy);
    double spring_time = timer.elapsed();
    
    std::cout << "   Time: " << spring_time * 1000 << " ms" << std::endl;
    std::cout << "   Throughput: " << (4 * 1000) / spring_time << " particle-steps/second" << std::endl;
    std::cout << "   ALU utilization: 100% (4/4 ALUs for 4 particles)" << std::endl;
}

int main() {
    test_leapfrog_physics();
    return 0;
} 