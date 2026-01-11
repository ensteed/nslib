#version 450
layout(set = 0, binding = 0) uniform frame_ubo_data {
    mat4 proj_view;  
} frame_ubo;

layout(set = 1, binding = 0) uniform material_ubo_data {
    vec4 color;
    vec4 misc; // use x component for now as a vertex material multiplier
} mat_ubo;

//push constants block
layout( push_constant ) uniform PC
{
  mat4 transform;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_norms;
layout(location = 3) in vec3 in_tangs;
layout(location = 4) in vec2 in_uv;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_uv;

void main() {
    // Because GLSL stores matrices in column major, we reverse our multiplication order
    gl_Position = vec4(in_pos, 1.0) * pc.transform * frame_ubo.proj_view;
    frag_color = mat_ubo.color + mat_ubo.misc.x * in_color;
    frag_uv = in_uv;
}
