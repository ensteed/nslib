#version 450
struct material_block {
    uint tex_pool_idx;
    uint sampler_idx;
    uint tex_layer;
    uint use_color;
    vec4 col;
};

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_uv;
layout(location = 2) flat in uint material_idx;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 5) readonly buffer material_ssbo_data {
    material_block mats[];
} mat_ssbo;
layout(set = 0, binding = 6) uniform sampler samplers[1];
layout(set = 1, binding = 0) uniform texture2DArray tex_pools[2];

// Helper to pick the sampler
// We use a sampler object specifically here
vec4 sample_with_sampler(texture2DArray tex, uint sampler_idx, vec3 uvw) {
    switch(sampler_idx) {
    case 0: return texture(sampler2DArray(tex, samplers[0]), uvw);
        // case 1: return texture(sampler2DArray(tex, samplers[1]), uvw);
        // case 2: return texture(sampler2DArray(tex, samplers[2]), uvw);
        // case 3: return texture(sampler2DArray(tex, samplers[3]), uvw);
    default: return vec4(0,0,0,0);
    }
}

// Helper to pick the texture
vec4 get_final_color(uint texture_idx, uint sampler_idx, vec3 uvw) {
    switch(texture_idx) {
    case 0:  return sample_with_sampler(tex_pools[0],  sampler_idx, uvw);
    case 1:  return sample_with_sampler(tex_pools[1],  sampler_idx, uvw);
        // case 2:  return sample_with_sampler(tex_pools[2],  sampler_idx, uvw);
        // case 3:  return sample_with_sampler(tex_pools[3],  sampler_idx, uvw);
        // case 4:  return sample_with_sampler(tex_pools[4],  sampler_idx, uvw);
        // case 5:  return sample_with_sampler(tex_pools[5],  sampler_idx, uvw);
        // case 6:  return sample_with_sampler(tex_pools[6],  sampler_idx, uvw);
        // case 7:  return sample_with_sampler(tex_pools[7],  sampler_idx, uvw);
        // case 8:  return sample_with_sampler(tex_pools[8],  sampler_idx, uvw);
        // case 9:  return sample_with_sampler(tex_pools[9],  sampler_idx, uvw);
        // case 10: return sample_with_sampler(tex_pools[10], sampler_idx, uvw);
        // case 11: return sample_with_sampler(tex_pools[11], sampler_idx, uvw);
    default: return vec4(1, 0, 1, 1); // Error Magenta
    }
}

void main() {
    material_block mat = mat_ssbo.mats[material_idx];
    if (mat.use_color > 0) {
        out_color = mat.col;
        return;
    }
    out_color = get_final_color(mat.tex_pool_idx, mat.sampler_idx, vec3(frag_uv, mat.tex_layer));
}
