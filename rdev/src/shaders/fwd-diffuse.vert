#version 450

layout(set = 0, binding = 0) buffer instance_ssbo_data {
  mat4 model;
  mat4 prev_model;
} inst_ssbo;

layout(set = 0, binding = 1) buffer material_ssbo_data {
  vec4 col;
} mat_ssbo;

layout(set = 0, binding = 2) uniform frame_ubo_data {
  mat4 view;
  mat4 proj;
  mat4 view_proj;
  mat4 inv_view_proj;
  float elapsed;
  float dt;
  uint frame_count;
  uint padding;
  vec2 resolution;
  vec2 inv_resolution;
} frame_ubo;

layout(set = 0, binding = 3) uniform draw_ubo_data {
    uint material_idx;
    uint instance_idx;
    // These padd the struct to 32 total bytes and allow for a few extra values to go to each draw instance
    ivec2 suser;
    vec4 fuser;
} draw_ubo;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_norms;
layout(location = 3) in vec3 in_tangs;
layout(location = 4) in vec2 in_uv;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_uv;

void main() {
    // Because GLSL stores matrices in column major, we reverse our multiplication order
    gl_Position = vec4(in_pos, 1.0) * frame_ubo.view_proj;
    frag_color = in_color;
    frag_uv = in_uv;
}
