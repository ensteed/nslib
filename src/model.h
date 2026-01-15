#pragma once
#include "asset_common.h"
#include "math/vector4.h"
#include "containers/array.h"
#include "render_handles.h"

namespace nslib
{

enum mat_sampler_slot
{
    MAT_SAMPLER_SLOT_DIFFUSE,
    MAT_SAMPLER_SLOT_NORMAL,
    MAT_SAMPLER_SLOT_COUNT
};

enum struct mesh_topology : u8
{
    TRIANGLE_STRIP
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
    ASSET(TEXTURE);
    void* pixels;
    uvec3 dims;
    texture_usage usage;
};

// Material references textures and pipelines, which both must be uploaded to GPUa
struct material
{
    ASSET(MATERIAL);
    vec4 col;
    asset_id technique;
    static_array<asset_id, MAT_SAMPLER_SLOT_COUNT> textures{.size = MAT_SAMPLER_SLOT_COUNT};
    rmaterial_handle rndr_hndl;
};

pup_func(material)
{
    pup_member(col);
    pup_member(technique);
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

struct submesh_range
{
    // Indice offset
    u32 offset;
    // Indice count
    u32 count;
    small_str mat_slot_name;
    u32 mat_slot_ind;
};

pup_func(submesh_range)
{
    pup_member(offset);
    pup_member(count);
    pup_member(mat_slot_name);
    pup_member(mat_slot_ind);
}

struct mesh
{
    ASSET(MESH);
    array<u32> inds;
    array<mvert> verts;
    array<mskinned_vert_info> skinned_verts_info;
    array<submesh_range> sm_info;
    mesh_topology topology;

    rmesh_handle rhndl;
};

pup_func(mesh)
{
    pup_member(inds);
    pup_member(verts);
    pup_member(skinned_verts_info);
    pup_member(sm_info);
    pup_enum_member(mesh_topology, u8, topology);
}

void init_texture(texture *tex, const string &name, mem_arena *arena);
void release_texture_ram_data(texture *tex);
void terminate_texture(texture *tex);
sizet get_texture_memsize(const texture *tex);
u32 get_texture_layer_pixel_count(const texture *tex);
bool load_texture(texture *tex, const char *path, cstr *err);

void init_material(material *mat, const string &name, mem_arena *arena);
void terminate_material(material *mat);

void make_rect(mesh *msh, const string &name, mem_arena *arena);
void make_cube(mesh *msh, const string &name, mem_arena *arena);

void init_mesh(mesh *msh, const string &name, mem_arena *arena);
void release_mesh_ram_data(mesh *msh);
void terminate_mesh(mesh *msh);

} // namespace nslib
