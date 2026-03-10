#version 450
struct draw_block {
    uint inst_idx;
    uint material_idx;
    uint view_idx;
    uint pass_idx;
};

struct view_block {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    mat4 inv_view_proj;
};

struct pass_block {
    vec2 resolution;
    vec2 inv_resolution;
};

struct instance_block {
    mat4 model;
    mat4 prev_model;
};

struct material_block {
    vec4 col;
};

layout(set = 0, binding = 0) readonly buffer draw_ssbo_data {
    draw_block draws[];
} draw_ssbo;

layout(set = 0, binding = 1) readonly buffer view_ssbo_data {
    view_block views[];
} view_ssbo;

layout(set = 0, binding = 2) readonly buffer pass_ssbo_data {
    pass_block passes[];
} pass_ssbo;

layout(set = 0, binding = 4) readonly buffer instance_ssbo_data {
    instance_block instances[];
} inst_ssbo;

layout(set = 0, binding = 5) readonly buffer material_ssbo_data {
    material_block mats[];
} mat_ssbo;

layout(set = 0, binding = 3) uniform frame_ubo_data {
    float elapsed;
    float dt;
    uint frame_count;
    uint padding;
} frame_ubo;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_norms;
layout(location = 3) in vec3 in_tangs;
layout(location = 4) in vec2 in_uv;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_uv;

void main() {
    // Because GLSL stores matrices in column major, we reverse our multiplication order
    draw_block draw = draw_ssbo.draws[gl_InstanceIndex];
    gl_Position = vec4(in_pos, 1.0) * inst_ssbo.instances[draw.inst_idx].model * view_ssbo.views[draw.view_idx].view_proj;
    frag_color = in_color;
    frag_uv = in_uv;
}
