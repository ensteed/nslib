#pragma once

#include "model.h"
#include "render_defs.h"

namespace nslib
{
// 20 million triangles... thats a lot - works on desktop
const u32 MAX_STATIC_TRIANGLE_COUNT = 2000000;
const u32 MAX_SKINNED_TRIANGLE_COUNT = 200000;
// Default ind buffer size (holding all of our inds) in ind count (not byte size)
const u32 MAX_TOTAL_MESH_IND_COUNT = (MAX_STATIC_TRIANGLE_COUNT + MAX_SKINNED_TRIANGLE_COUNT) * 3;
// Default vert buffer size (holding all of our verts) in vert count (not byte size)
// Gemeni showed me that on average we will have 2 : 1 triangle to vert ratio
const u32 MAX_STATIC_MESH_VERT_COUNT = MAX_STATIC_TRIANGLE_COUNT / 2;
const u32 MAX_SKINNED_MESH_VERT_COUNT = MAX_SKINNED_TRIANGLE_COUNT / 2;
    

struct mem_arena;
struct renderer;
enum rvert_stream
{
    RVERT_STREAM_POS_COL,
    RVERT_STREAM_NORM_TAN_UV,
    RVERT_STREAM_SKINNED_POS_COL,
    RVERT_STREAM_SKINNED_NORM_TAN_UV,
    RVERT_STREAM_SKINNED_BONES_WEIGHT_ID,
    RVERT_STREAM_COUNT,
};

enum rvert_layout : u32
{
    RVERT_LAYOUT_STATIC_MESH,
    RVERT_LAYOUT_SKINNED_MESH,
    RVERT_LAYOUT_COUNT
};

struct rmesh_vert_pos_col
{
    vec3 pos;
    u32 col;
};

struct rmesh_vert_norm_tan_uv
{
    vec3 norm;
    vec3 tangent;
    vec2 uv;
};

struct rmesh_vert_bone_weights_ids
{
    // TODO: Pack these to unorm weights and u8 bonde ids
    vec4 bone_weights;
    uvec4 bone_ids;
};

// Setup vert/index buffers (stream group) for this geometry type and get the runtime id for it
rres_handle setup_geometry_stream_group(renderer *rndr);

bool upload_geometry(renderer *rndr, rres_handle stream_gp, mesh *mesh, mem_arena *arena);
u32 upload_geometry(renderer *rndr, rres_handle stream_gp, asset_pool<mesh> *meshes, mem_arena *arena);

void upload_textures(renderer *rndr, texture_pool *tex_pool, mem_arena *arena);

} // namespace nslib
