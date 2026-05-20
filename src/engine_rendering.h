#pragma once
#include "math/matrix4.h"
#include "model.h"

namespace nslib
{
// 20 million triangles... thats a lot - works on desktop
const u32 MAX_STATIC_TRIANGLE_COUNT = 2000000;
const u32 MAX_SKINNED_TRIANGLE_COUNT = 200000;
// Default ind buffer size (holding all of our inds) in ind count (not byte size)
const u32 MAX_TOTAL_GEOM_IND_COUNT = (MAX_STATIC_TRIANGLE_COUNT + MAX_SKINNED_TRIANGLE_COUNT) * 3;
// Default vert buffer size (holding all of our verts) in vert count (not byte size)
// Gemeni showed me that on average we will have 2 : 1 triangle to vert ratio
const u32 MAX_STATIC_GEOM_VERT_COUNT = MAX_STATIC_TRIANGLE_COUNT / 2;
const u32 MAX_SKINNED_GEOM_VERT_COUNT = MAX_SKINNED_TRIANGLE_COUNT / 2;


intern constexpr const char *MAIN_GEOM_STREAM_GP = "main";
intern const rres_id MAIN_GEOM_STREAM_GP_ID = hash_type(MAIN_GEOM_STREAM_GP);

struct mem_arena;
struct renderer;
struct sim_region;
struct rmanifest;

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
    RVERT_LAYOUT_STATIC_GEOM,
    RVERT_LAYOUT_SKINNED_GEOM,
    RVERT_LAYOUT_COUNT
};

struct instance_ssbo_data {
    mat4 model;
    mat4 prev_model;
};

struct material_ssbo_data {
    idx_t tex_pool_idx;
    idx_t sampler_idx;
    idx_t tex_layer;
    u32 use_col;
    vec4 col;
};

struct view_ssbo_data {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    mat4 inv_view_proj;
};

struct pass_ssbo_data {
    vec2 resolution;
    vec2 inv_resolution;
};

// This data is used in uniform buffer - needs to be aligned to 16 bytes
struct frame_ubo_data {
    float elapsed;
    float dt;
    u32 frame_count;
    u32 padding;
};

struct rgeom_vert_pos_col
{
    vec3 pos;
    u32 col;
};

struct rgeom_vert_norm_tan_uv
{
    vec3 norm;
    vec3 tangent;
    vec2 uv;
};

struct rgeom_vert_bone_weights_ids
{
    // TODO: Pack these to unorm weights and u8 bonde ids
    vec4 bone_weights;
    uvec4 bone_ids;
};

void update_and_draw_region(rmanifest *m, sim_region *sr, asset_cache *cg, material *def_material);

// Setup vert/index buffers (stream group) for this geometry type and get the runtime id for it
u32 setup_geometry_stream_group(renderer *rndr);

bool upload_geometry(renderer *rndr, u32 stream_gp, geometry *geom, mem_arena *arena);
u32 upload_geometries(renderer *rndr, u32 stream_gp, asset_pool<geometry> *geoms, mem_arena *scratch);

bool upload_texture(renderer *rndr, texture *tex, mem_arena *scratch);
u32 upload_textures(renderer *rndr, texture_pool *tex_pool, mem_arena *scratch);

bool upload_technique(renderer *rndr, technique *tech, shader_pool *sp, mem_arena *scratch);
u32 upload_techniques(renderer *rndr, technique_pool *tech_pool, shader_pool *sp, mem_arena *scratch);

bool upload_material(renderer *rndr, material *mat, texture_pool *tex_pool, mem_arena *scratch);
u32 upload_materials(renderer *rndr, material_pool *mat_pool, texture_pool *tex_pool, mem_arena *scratch);

bool upload_shader(renderer *rndr, shader *shdr, mem_arena *scratch);
u32 upload_shaders(renderer *rndr, shader_pool *shdr_pool, mem_arena *scratch);

} // namespace nslib
