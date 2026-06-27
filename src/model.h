#pragma once
#include "asset_common.h"
#include "math/vector4.h"
#include "containers/array.h"
#include "render_defs.h"

namespace nslib
{

enum mat_sampler_slot
{
    MAT_SAMPLER_SLOT_ALBEDO,
    MAT_SAMPLER_SLOT_NORMAL,
    MAT_SAMPLER_SLOT_COUNT
};

enum mat_sampler_type
{
    MAT_SAMPLER_TYPE_LINEAR_REPEAT,
    MAT_SAMPLER_TYPE_COUNT
};

enum geometry_topology : u8
{
    GEOMETRY_TOPOLOGY_POINT_LIST,
    GEOMETRY_TOPOLOGY_LINE_LIST,
    GEOMETRY_TOPOLOGY_LINE_STRIP,
    GEOMETRY_TOPOLOGY_TRIANGLE_LIST,
    GEOMETRY_TOPOLOGY_TRIANGLE_STRIP,
    GEOMETRY_TOPOLOGY_TRIANGLE_FAN
};

enum struct texture_usage : u8
{
    ALBEDO,
    NORMAL,
    GRAYSCALE,
    HDR,
};

enum texture_bits : u32
{
    TEXTURE_CUBEMAP_BIT = ASSET_USER_BASE_BIT
};

struct texture
{
    ASSET(TEXTURE, nsimg);
    void *pixels;
    uvec2 dims;
    u32 mip_levels;
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

// The parts of technique pass state that can be overwritten by a material
enum raster_override_state_flag
{
    RASTER_OVERRIDE_STATE_NONE = 0,
    RASTER_OVERRIDE_STATE_CULLING = make_flag(0),
    RASTER_OVERRIDE_STATE_STENCIL_TEST = make_flag(1),
    RASTER_OVERRIDE_STATE_STENCIL_MODE = make_flag(2),
    RASTER_OVERRIDE_STATE_BLEND_CONSTANTS = make_flag(3),
    RASTER_OVERRIDE_STATE_DEPTH_BIAS = make_flag(4),
    RASTER_OVERRIDE_STATE_ALL = (RASTER_OVERRIDE_STATE_CULLING | RASTER_OVERRIDE_STATE_STENCIL_TEST | RASTER_OVERRIDE_STATE_STENCIL_MODE |
                                 RASTER_OVERRIDE_STATE_BLEND_CONSTANTS | RASTER_OVERRIDE_STATE_DEPTH_BIAS),
};
using raster_override_state_flags = u8;

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
    // Only used by blend mode constant
    vec4 blend_constants;
    vec3 depth_bias;
};

enum polygon_mode
{
    POLYGON_MODE_FILL,
    POLYGON_MODE_LINE,
    POLYGON_MODE_POINT,
};

struct technique_pass
{
    // Shader id
    rid shader;
    // Blueprint pass id and subpass index
    rid bp_pass;
    idx_t bp_subpass;
    // Which layout the technique uses within the stream group. The stream group is specified in the blueprint pass.
    idx_t gsg_layout;
    // Default state is provided for each material referencing this technique - we use these values unless the material
    // specifically overrides it
    raster_state dflt_st{.rmask = RASTER_FLAG_CULL_BACK | RASTER_FLAG_DEPTH_TEST | RASTER_FLAG_DEPTH_WRITE,
                         .sm = STENCIL_MODE_OFF,
                         .blend_constants{1.0f},
                         .depth_bias{}};
    // This is a mask of all raster properties that CAN be overridden
    raster_override_state_flags can_override{RASTER_OVERRIDE_STATE_ALL};
    depth_mode dm{DEPTH_MODE_NORMAL};
    blend_mode bm{BLEND_MODE_OPAQUE};
    polygon_mode poly_mode{POLYGON_MODE_FILL};
    geometry_topology topology{GEOMETRY_TOPOLOGY_TRIANGLE_LIST};
};

struct technique
{
    ASSET(TECHNIQUE, tech);
    // Blueprint id
    rid bpid;
    // Pass per render blueprint pass
    array<technique_pass> passes;
    rtechnique_handle rhndl;
};

struct mat_blueprint_mapping
{
    rid bpid;
    rid tech_id;
};

enum material_bits
{
    MATERIAL_USE_COLOR_BIT = ASSET_USER_BASE_BIT,
};

struct texture_info
{
    rid id{};
    mat_sampler_type sampler{MAT_SAMPLER_TYPE_LINEAR_REPEAT};
};

// Material references textures and pipelines, which both must be uploaded to GPUa
struct material
{
    ASSET(MATERIAL, mat);
    vec4 col;

    // Render blueprint to technique mapping
    array<mat_blueprint_mapping> bp_techniques;

    // Techniuqe overrides (if override_mask contains bit for thing to override)
    // NOTE: The bits set in rmask here that don't pertain to bits in override_mask do nothing if set. For example,
    // setting DEPTH_TEST will never do anything because it's not something a material can override
    raster_state overrides;

    // Which state to override, if can be overridden
    raster_override_state_flags override_mask;

    // Textures used by material
    texture_info textures[MAT_SAMPLER_SLOT_COUNT];

    // Handle given back by renderer
    rmaterial_handle rhndl;
};

pup_func(material)
{
    pup_member(col);
    pup_member(bp_techniques);
    pup_member(textures);
}

struct gvert
{
    vec3 pos;
    vec3 norm;
    vec3 tan;
    vec2 uv;
    u32 col;
};

pup_func(gvert)
{
    pup_member(pos);
    pup_member(norm);
    pup_member(tan);
    pup_member(uv);
    pup_member(col);
}

struct gskinned_vert_info
{
    uvec4 bone_ids;
    vec4 bone_weights;
};

pup_func(gskinned_vert_info)
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
    idx_t mat_slot_ind;
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
    array<gvert> verts;
    array<gskinned_vert_info> skinned_verts_info;
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
u32 get_texture_pixel_count(const texture *tex);
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
void init_asset(geometry *geom);
void release_ram_data(geometry *geom);
void terminate_asset(geometry *geom);
void make_unit_rect(geometry *geom);
void make_unit_cube(geometry *geom);
void make_unit_sphere(geometry *geom, u32 precision);

idx_t find_subgeom_by_mat_slot(const geometry *geom, idx_t mat_slot);

} // namespace nslib
