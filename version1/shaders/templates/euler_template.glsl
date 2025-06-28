#version 310 es
layout(local_size_x = 4, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer StateBuffer {
    float current_state[];  // [eq0, eq1, eq2, ..., eq_N-1]
};

layout(std430, binding = 1) buffer ParamBuffer {
    float dt;
    float t_current;
    int n_equations;
    {{USER_UNIFORMS}}  // Template substitution point for user parameters
};

layout(std430, binding = 2) buffer ResultBuffer {
    float time_series[];  // [t0_eq0, t0_eq1, ..., t1_eq0, t1_eq1, ...]
};

layout(std430, binding = 3) buffer TimeBuffer {
    int current_step;
    int total_steps;
};

// User-defined RHS function - will be substituted at runtime
{{RHS_FUNCTION}}

void main() {
    uint eq_idx = gl_GlobalInvocationID.x;
    
    if (eq_idx >= uint(n_equations)) return;
    
    // EXPLICIT EULER: Single stage, embarrassingly parallel!
    // y_{n+1} = y_n + dt * f(t_n, y_n)
    
    float y_current = current_state[eq_idx];
    float dydt = evaluate_rhs(eq_idx, y_current, t_current);
    float y_new = y_current + dt * dydt;
    
    // Update state for next timestep
    current_state[eq_idx] = y_new;
    
    // Store in time series (if recording)
    if (current_step >= 0 && current_step < total_steps) {
        uint result_idx = uint(current_step) * uint(n_equations) + eq_idx;
        time_series[result_idx] = y_new;
    }
} 