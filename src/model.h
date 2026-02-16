#pragma once
#include "asset_common.h"
#include "math/vector4.h"
#include "containers/array.h"
#include "render_defs.h"

namespace nslib
{

enum mat_sampler_slot
{
    MAT_SAMPLER_SLOT_DIFFUSE,
    MAT_SAMPLER_SLOT_NORMAL,
    MAT_SAMPLER_SLOT_COUNT
};

enum struct geometry_topology : u8
{
    POINT_LIST,
    LINE_LIST,
    LINE_STRIP,
    TRIANGLE_LIST,
    TRIANGLE_STRIP,
    TRIANGLE_FAN
};

enum struct texture_usage : u8
{
    ALBEDO,
    NORMAL,
    GRAYSCALE,
    HDR
};

struct texture
{
    ASSET(TEXTURE, nsimg);
    void *pixels;
    uvec3 dims;
    texture_usage usage;
    rtexture_handle rhndl;
};

enum raster_flag
{
    RASTER_FLAG_NONE,
    RASTER_FLAG_CULL_FRONT = make_flag(0),
    RASTER_FLAG_CULL_BACK = make_flag(1),
    RASTER_FLAG_DEPTH_TEST = make_flag(2),
    RASTER_FLAG_DEPTH_WRITE = make_flag(3),
    RASTER_FLAG_STENCIL_TEST = make_flag(4),
};
using raster_flags = u8;

enum raster_state_flag
{
    RASTER_STATE_NONE = 0,
    RASTER_STATE_CULLING = make_flag(0),
    RASTER_STATE_DEPTH_TEST = make_flag(1),
    RASTER_STATE_DEPTH_WRITE = make_flag(2),
    RASTER_STATE_DEPTH_MODE = make_flag(3),
    RASTER_STATE_STENCIL_TEST = make_flag(4),
    RASTER_STATE_STENCIL_MODE = make_flag(5),
    RASTER_STATE_BLEND_CONSTANTS = make_flag(6),
    RASTER_STATE_ALL = (RASTER_STATE_CULLING | RASTER_STATE_DEPTH_TEST | RASTER_STATE_DEPTH_WRITE | RASTER_STATE_DEPTH_MODE |
                        RASTER_STATE_STENCIL_TEST | RASTER_STATE_STENCIL_MODE | RASTER_STATE_BLEND_CONSTANTS),
};
using raster_state_flags = u8;

enum shader_stage_type
{
    SHADER_STAGE_TYPE_VERTEX,
    SHADER_STAGE_TYPE_FRAGMENT,
    SHADER_STAGE_TYPE_COMPUTE,
    SHADER_STAGE_TYPE_COUNT,
};

enum blend_mode
{
    BLEND_MODE_OPAQUE,
    BLEND_MODE_MASKED,
    BLEND_MODE_TRANSPARENT,
    BLEND_MODE_ADDITIVE,
    // Used blend mode constants
    BLEND_MODE_CONSTANT,
};

enum depth_mode
{
    DEPTH_MODE_NORMAL,
    DEPTH_MODE_ALWAYS,
    DEPTH_MODE_BEHIND,
    DEPTH_MODE_MATCH,
    DEPTH_MODE_OFF,
};

enum stencil_mode
{
    STENCIL_MODE_OFF,
    STENCIL_MODE_WRITE,
    STENCIL_MODE_CLIP_INSIDE,
    STENCIL_MODE_CLIP_OUTSIDE,
};

struct shader_stage
{
    small_str entry_point;
    shader_stage_type stype;
    byte_array src;
};

struct shader
{
    ASSET(SHADER, efx);
    array<shader_stage> stages;
    rshader_handle rhndl;
};

struct raster_state
{
    raster_flags rmask;
    stencil_mode sm;
    depth_mode dm;
    // Only used by blend mode constant
    vec4 blend_constants;
};

struct technique_pass
{
    asset_id shader;
    // Default state is provided for each material referencing this technique - we use these values unless the material
    // specifically overrides it
    raster_state dflt_st{.rmask = RASTER_FLAG_CULL_BACK | RASTER_FLAG_DEPTH_TEST | RASTER_FLAG_DEPTH_WRITE,
                         .sm = STENCIL_MODE_OFF,
                         .dm = DEPTH_MODE_NORMAL,
                         .blend_constants{1.0f}};
    // This is a mask of all raster properties that CAN be overridden
    raster_state_flags can_override{RASTER_STATE_ALL};
    blend_mode bm{BLEND_MODE_OPAQUE};
};

struct technique
{
    ASSET(TECHNIQUE, tech);

    // Pass per render blueprint pass
    hmap<rres_id, technique_pass> passes;
    rtechnique_handle rhndl;
};

// Material references textures and pipelines, which both must be uploaded to GPUa
struct material
{
    ASSET(MATERIAL, mat);
    vec4 col;

    // Render blueprint to technique mapping
    hmap<rres_id, asset_id> bp_techniques;

    // Techniuqe overrides (if override_mask contains bit for thing to override)
    raster_state overrides;

    // Which state to override, if can be overridden
    raster_state_flags override_mask;

    // Textures used by material
    static_array<asset_id, MAT_SAMPLER_SLOT_COUNT> textures{.size = MAT_SAMPLER_SLOT_COUNT};

    // Handle given back by renderer
    rmaterial_handle rhndl;
};

pup_func(material)
{
    pup_member(col);
    pup_member(bp_techniques);
    pup_member(textures);
}

struct mvert
{
    vec3 pos;
    vec3 norm;
    vec3 tan;
    vec2 uv;
    u32 col;
};

pup_func(mvert)
{
    pup_member(pos);
    pup_member(norm);
    pup_member(tan);
    pup_member(uv);
    pup_member(col);
}

struct mskinned_vert_info
{
    uvec4 bone_ids;
    vec4 bone_weights;
};

pup_func(mskinned_vert_info)
{
    pup_member(bone_ids);
    pup_member(bone_weights);
}

struct subgeom_range
{
    // Indice offset
    u32 offset;
    // Indice count
    u32 count;
    small_str mat_slot_name;
    u32 mat_slot_ind;
};

pup_func(subgeom_range)
{
    pup_member(offset);
    pup_member(count);
    pup_member(mat_slot_name);
    pup_member(mat_slot_ind);
}

struct geometry
{
    ASSET(GEOMETRY, geom);
    array<u32> inds;
    array<mvert> verts;
    array<mskinned_vert_info> skinned_verts_info;
    array<subgeom_range> sm_info;
    geometry_topology topology;

    rgeom_handle rhndl;
};

pup_func(geometry)
{
    pup_member(inds);
    pup_member(verts);
    pup_member(skinned_verts_info);
    pup_member(sm_info);
    pup_enum_member(geometry_topology, u8, topology);
}

// TEXTURE
void init_asset(texture *tex);
void release_ram_data(texture *tex);
void terminate_asset(texture *tex);
sizet get_texture_memsize(const texture *tex);
u32 get_texture_layer_pixel_count(const texture *tex);
const char *load_texture(texture *tex, const char *path);

// SHADER
void init_asset(shader *shdr);
void release_ram_data(shader *shdr);
void terminate_asset(shader *shdr);
const char *load_shader(shader *shdr, const char *path);
const char *get_shader_stage_str(shader_stage_type stype);

// TECHNIQUE
void init_asset(technique *tech);
void terminate_asset(technique *tech);

// MATERIAL
void init_asset(material *mat);
void terminate_asset(material *mat);

// GEOMETRY
void make_rect(geometry *geom);
void make_cube(geometry *geom);
void init_asset(geometry *geom);
void release_ram_data(geometry *geom);
void terminate_asset(geometry *geom);

} // namespace nslib
