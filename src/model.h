#pragma once
#include "robj_common.h"
#include "math/vector4.h"
#include "containers/array.h"
#include "render_handles.h"

namespace nslib
{

inline constexpr sizet JOINTS_PER_VERTEX = 4;

enum mat_sampler_slot
{
    MAT_SAMPLER_SLOT_DIFFUSE,
    MAT_SAMPLER_SLOT_NORMAL,
    MAT_SAMPLER_SLOT_COUNT
};

struct texture
{
    ROBJ(TEXTURE);
    byte_array pixels;
    uvec2 size;
    u8 channels;
};

// Material references textures and pipelines, which both must be uploaded to GPUa
struct material
{
    ROBJ(MATERIAL);
    vec4 col;
    rid technique;
    static_array<rid, MAT_SAMPLER_SLOT_COUNT> textures{.size=MAT_SAMPLER_SLOT_COUNT};
    rmaterial_handle rndr_hndl;
};

pup_func(material)
{
    pup_member(col);
    pup_member(technique);
    pup_member(textures);
}

struct mvert {
    vec3 pos;
    vec3 norm;
    vec3 tan;
    vec2 uv;
    u32 col;
};

pup_func(mvert) {
    pup_member(pos);
    pup_member(norm);
    pup_member(tan);
    pup_member(uv);
    pup_member(col);
}

struct mskinned_vert_info {
    u16 bone_ids[JOINTS_PER_VERTEX];
    float bone_weights[JOINTS_PER_VERTEX];
};

pup_func(mskinned_vert_info) {
    pup_member(bone_ids);
    pup_member(bone_weights);
}

struct submesh_range {
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
    ROBJ(MESH);
    array<u32> inds;
    array<mvert> verts;
    array<mskinned_vert_info> skinned_verts_info;
    array<submesh_range> sm_info;
    rmesh_handle rhndl;
};

pup_func(mesh)
{
    pup_member(inds);
    pup_member(verts);
    pup_member(skinned_verts_info);
    pup_member(sm_info);
}

void init_texture(texture *tex, const string &name, mem_arena *arena);
void release_texture_ram_data(texture *tex);
void terminate_texture(texture *tex);
sizet get_texture_memsize(const texture *tex);
u32 get_texture_pixel_count(const texture *tex);
bool load_texture(texture *tex, const char *path, cstr *err);

void init_material(material *mat, const string &name, mem_arena *arena);
void terminate_material(material *mat);

void make_rect(mesh *msh, const string &name, mem_arena *arena);
void make_cube(mesh *msh, const string &name, mem_arena *arena);

void init_mesh(mesh *msh, const string &name, mem_arena *arena);
void release_mesh_ram_data(mesh *msh);
void terminate_mesh(mesh *msh);

} // namespace nslib
