#pragma once
#include "basic_types.h"
#include "math/vector4.h"
#include "math/matrix4.h"

namespace nslib
{
// GPU block layouts shared between the engine and the shaders. The renderer core treats these as opaque bytes sized by
// the configured block sizes, so it does not need to include this header.

struct transform_ssbo_data
{
    mat4 model;
    mat4 prev_model;
};

struct material_ssbo_data
{
    idx_t tex_pool_idx;
    idx_t sampler_idx;
    idx_t tex_layer;
    u32 use_col;
    vec4 col;
};

struct view_ssbo_data
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    mat4 inv_view_proj;
};

struct pass_ssbo_data
{
    vec2 resolution;
    vec2 inv_resolution;
};

// This data is used in uniform buffer - needs to be aligned to 16 bytes
struct frame_ubo_data
{
    f64 sim_elapsed;
    f32 sim_dt;
    u32 sim_frame_count;
    f64 render_elapsed;
    f32 render_dt;
    u32 render_frame_count;
};

} // namespace nslib
