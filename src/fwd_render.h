#pragma once

#include "renderer.h"

namespace nslib
{

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

struct rmesh_create_info
{
    const char *name;
    const rmesh_vert_pos_col *pos_col;
    const rmesh_vert_norm_tan_uv *norm_tan_uv;
    // If weight ids are none null then the mesh is skinned
    const rmesh_vert_bone_weights_ids *weights_ids;
    sizet vert_count;

    const ind_t *inds;
    sizet ind_count;

    const rsubgeom_range *sm_info;
    sizet sm_count;

    rmesh_topology topology;
};


} // namespace nslib
