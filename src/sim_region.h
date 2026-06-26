#pragma once

#include "math/matrix4.h"
#include "asset_id.h"
#include "containers/array.h"
#include "containers/hmap.h"

namespace nslib
{
enum comp_type
{
    COMP_TYPE_TRANSFORM,
    COMP_TYPE_CAMERA,
    COMP_TYPE_STATIC_MESH,
    COMP_TYPE_USER_BASE,
};

enum comp_bits : u8
{
    COMP_DIRTY_BIT,
    COMP_USER_BASE_BIT
};
using comp_flags = u64;

#define COMP(type)                                                                                                                         \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = COMP_TYPE_##type;                                                                                 \
    u32 ent_id;                                                                                                                            \
    u64 flags;

#define PUP_COMP_COMMON                                                                                                                    \
    pup_member(ent_id);                                                                                                                    \
    pup_member(flags)


struct transform_trs {
    vec3 world_pos;
    quat orientation;
    vec3 scale{1};
};

struct transform
{
    COMP(TRANSFORM)
    transform_trs prev;
    transform_trs current;
    mat4 cached;
    u32 rfif_dirty;
    // If in the active set, this will be the index in to the array
    idx_t active_idx{INVALID_IDX};
};

struct transform_system
{
    array<u32> active_set;
};

void init_comp_system(transform_system *sys, mem_arena *arena, sizet initial_capacity);
void terminate_comp_system(transform_system *sys);

struct material_subgeom_mapping
{
    asset_id mat_id;
    u32 sm_mat_slot;
};

struct static_mesh
{
    COMP(STATIC_MESH)
    asset_id geom_id;
    array<material_subgeom_mapping> mat_mapping{};
};

enum camera_proj_type : u8
{
    CAMERA_PROJ_TYPE_PERSPECTIVE,
    CAMERA_PROJ_TYPE_ORTHO,
};

enum camera_fov_type : u8
{
    CAMERA_FOV_TYPE_VERTICAL,
    CAMERA_FOV_TYPE_HORIZONTAL,
};

struct camera
{
    COMP(CAMERA)
    // Degrees for perspective. For ortho, world-space size (half-height or half-width depending on fov_type) — change this to zoom in/out.
    f32 fov;
    f32 aspect;
    vec2 near_far;
    mat4 proj;
    mat4 view;
    mat4 proj_view;
    camera_proj_type ptype{CAMERA_PROJ_TYPE_PERSPECTIVE};
    camera_fov_type fov_type{CAMERA_FOV_TYPE_VERTICAL};
};

pup_func(camera)
{
    PUP_COMP_COMMON;
    pup_member(fov);
    pup_member(aspect);
    pup_member(near_far);
    pup_member(proj);
    pup_member(view);
    pup_enum_member(camera_proj_type, u8, ptype);
    pup_enum_member(camera_fov_type, u8, fov_type);
}

template<typename T, typename SysData>
struct comp_table
{
    using CompType = T;
    using SysType = SysData;

    SysData sys;
    array<T> entries;
    hmap<u32, sizet> entc_hm;
};

struct comp_db;
using remove_func = bool(u32 ent_id, comp_db *cdb);

struct comp_table_entry
{
    void *tbl;
    remove_func *rem_func;
};

struct comp_db
{
    array<comp_table_entry> comp_tables;
};

struct entity
{
    u32 id;
    string name;
    comp_db *cdb;
};

struct null_system_type
{};

struct sim_region
{
    array<entity> ents;
    hmap<u32, sizet> entmap;
    comp_db cdb;
    u32 last_id{};
};

void set_camera_fov(camera *cam, f32 fov, camera_fov_type ft = CAMERA_FOV_TYPE_VERTICAL);
void set_camera_near_far(camera *cam, const vec2 &near_far);
void set_camera_aspect(camera *cam, f32 ar);
void set_camera_proj_type(camera *cam, camera_proj_type pt);

void update_transform_pos(transform *tf, transform_system *tfs,const vec3 &pos);
void update_transform_orientation(transform *tf, transform_system *tfs, const quat &q);
void update_transform_scale(transform *tf, transform_system *tfs, const vec3 &s);

transform_trs interpolate_trs(const transform_trs &prev, const transform_trs &current, f32 alpha);
mat4 build_mat4_from_trs(const transform_trs &trs);
mat4 interpolate_tranform(const transform &tf, f32 alpha);

template<typename SysData>
void init_comp_system(SysData *sys, mem_arena *arena, sizet initial_capacity) {
    // Do nothing by default
}

template<typename SysData>
void terminate_comp_system(SysData *sys) {
    // Do nothing by default
}

template<typename T, typename SysData>
void init_comp_tbl(comp_table<T, SysData> *tbl, mem_arena *arena, sizet initial_capacity)
{
    arr_init(&tbl->entries, arena, initial_capacity);
    hmap_init(&tbl->entc_hm, hash_type, arena);
    init_comp_system(&tbl->sys, arena, initial_capacity);
}

template<typename T, typename SysData>
void terminate_comp_tbl(comp_table<T, SysData> *tbl)
{
    terminate_comp_system(&tbl->sys);
    hmap_terminate(&tbl->entc_hm);
    arr_terminate(&tbl->entries);
}

template<typename T, typename SysData>
bool remove_comp(u32 ent_id, comp_db *cdb);

template<typename T, typename SysData>
comp_table<T, SysData> *add_comp_tbl(comp_db *cdb, sizet initial_capacity = 64)
{
    if ((T::type_id + 1) > cdb->comp_tables.size) {
        arr_resize(&cdb->comp_tables, T::type_id + 1);
    }
    if (!cdb->comp_tables[T::type_id].tbl) {
        auto ctbl = mem_calloc<comp_table<T, SysData>>(1, cdb->comp_tables.arena);
        init_comp_tbl(ctbl, cdb->comp_tables.arena, initial_capacity);
        cdb->comp_tables[T::type_id].tbl = ctbl;
        cdb->comp_tables[T::type_id].rem_func = remove_comp<T, SysData>;
    }
    return (comp_table<T, SysData> *)cdb->comp_tables[T::type_id].tbl;
}

template<typename T, typename SysData>
comp_table<T, SysData> *get_comp_tbl(comp_db *cdb)
{
    if (T::type_id < cdb->comp_tables.size) {
        return (comp_table<T, SysData> *)cdb->comp_tables[T::type_id].tbl;
    }
    return nullptr;
}

template<typename T, typename SysData>
const comp_table<T, SysData> *get_comp_tbl(const comp_db *cdb)
{
    if (T::type_id < cdb->comp_tables.size) {
        return (const comp_table<T, SysData> *)cdb->comp_tables[T::type_id].tbl;
    }
    return nullptr;
}

template<typename T, typename SysData>
bool remove_comp_tbl(comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T, SysData>(cdb);
    if (ctbl) {
        terminate_comp_tbl(ctbl);
        mem_free(ctbl, cdb->comp_tables.arena);
        cdb->comp_tables[T::type_id] = {};
        return true;
    }
    return false;
}

template<typename T, typename SysData>
T *add_comp(u32 ent_id, comp_table<T, SysData> *ctbl, const T &copy = {})
{
    T *ret{};
    sizet cid = ctbl->entries.size;
    auto item = hmap_insert(&ctbl->entc_hm, ent_id, cid);
    if (item) {
        arr_push_back(&ctbl->entries, copy);
        ctbl->entries[cid].ent_id = ent_id;
        ret = &ctbl->entries[cid];
    }
    return ret;
}

template<typename T, typename SysData>
T *add_comp(u32 ent_id, comp_db *cdb, const T &copy = {})
{
    auto ctbl = get_comp_tbl<T, SysData>(cdb);
    return add_comp<T, SysData>(ent_id, ctbl, copy);
}

template<typename T, typename SysData>
T *add_comp(entity *ent, const T &copy = {})
{
    return add_comp<T, SysData>(ent->id, ent->cdb, copy);
}

template<typename T, typename SysData>
T *get_comp(u32 ent_id, comp_table<T, SysData> *ctbl)
{
    auto fiter = hmap_find(&ctbl->entc_hm, ent_id);
    if (!fiter) {
        return nullptr;
    }
    return &ctbl->entries[fiter->val];
}

template<typename T, typename SysData>
T *get_comp(u32 ent_id, comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T, SysData>(cdb);
    return get_comp(ent_id, ctbl);
}

template<typename T, typename SysData>
T *get_comp(entity *ent)
{
    return get_comp<T, SysData>(ent->id, ent->cdb);
}

template<typename T, typename SysData>
sizet get_comp_ind(const T *comp, const comp_table<T, SysData> *ctbl)
{
    return (comp - ctbl->entries.data);
}

template<typename T, typename SysData>
sizet get_comp_ind(const T *comp, const comp_db *cdb)
{
    return get_comp_ind(comp, get_comp_tbl<T, SysData>(cdb));
}

template<typename T, typename SysData>
bool remove_comp(u32 ent_id, comp_table<T, SysData> *ctbl)
{
    auto fiter = hmap_find(&ctbl->entc_hm, ent_id);
    if (!fiter) return false;
    if (arr_swap_remove(&ctbl->entries, fiter->val)) {
        if (fiter->val < ctbl->entries.size) {
            mark_comp_dirty(&ctbl->entries[fiter->val]);
            hmap_set(&ctbl->entc_hm, ctbl->entries[fiter->val].ent_id, fiter->val);
        }
        hmap_erase(&ctbl->entc_hm, fiter);
        return true;
    }
    return false;
}

template<typename T, typename SysData>
bool remove_comp(u32 ent_id, comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T, SysData>(cdb);
    return remove_comp(ent_id, ctbl);
}

template<typename T, typename SysData>
bool remove_comp(entity *ent)
{
    return remove_comp<T, SysData>(ent->id, ent->cdb);
}

template<typename T>
void mark_comp_dirty(T *comp)
{
    set_flags(comp->flags, make_flag(COMP_DIRTY_BIT));
}

template<typename T>
bool is_comp_dirty(const T &comp)
{
    return test_flags(comp.flags, make_flag(COMP_DIRTY_BIT));
}

#define DEFINE_COMP_TBL_TYPE(comp_name, system_t)                                                                                          \
    using comp_name##_tbl = comp_table<comp_name, system_t>;                                                                               \
    inline comp_name##_tbl *add_##comp_name##_tbl(comp_db *cdb, sizet initial_capacity = 64)                                               \
    {                                                                                                                                      \
        return add_comp_tbl<comp_name, system_t>(cdb, initial_capacity);                                                                   \
    }                                                                                                                                      \
    inline comp_name##_tbl *get_##comp_name##_tbl(comp_db *cdb)                                                                            \
    {                                                                                                                                      \
        return get_comp_tbl<comp_name, system_t>(cdb);                                                                                     \
    }                                                                                                                                      \
    inline const comp_name##_tbl *get_##comp_name##_tbl(const comp_db *cdb)                                                                \
    {                                                                                                                                      \
        return get_comp_tbl<comp_name, system_t>(cdb);                                                                                     \
    }                                                                                                                                      \
    inline bool remove_##comp_name##_tbl(comp_db *cdb)                                                                                     \
    {                                                                                                                                      \
        return remove_comp_tbl<comp_name, system_t>(cdb);                                                                                  \
    }                                                                                                                                      \
    inline comp_name *add_##comp_name(u32 ent_id, comp_name##_tbl *ctbl, const comp_name &copy = {})                                       \
    {                                                                                                                                      \
        return add_comp<comp_name, system_t>(ent_id, ctbl, copy);                                                                          \
    }                                                                                                                                      \
    inline comp_name *add_##comp_name(u32 ent_id, comp_db *cdb, const comp_name &copy = {})                                                \
    {                                                                                                                                      \
        return add_comp<comp_name, system_t>(ent_id, cdb, copy);                                                                           \
    }                                                                                                                                      \
    inline comp_name *add_##comp_name(entity *ent, const comp_name &copy = {})                                                             \
    {                                                                                                                                      \
        return add_comp<comp_name, system_t>(ent, copy);                                                                                   \
    }                                                                                                                                      \
    inline comp_name *get_##comp_name(u32 ent_id, comp_name##_tbl *ctbl)                                                                   \
    {                                                                                                                                      \
        return get_comp<comp_name, system_t>(ent_id, ctbl);                                                                                \
    }                                                                                                                                      \
    inline comp_name *get_##comp_name(u32 ent_id, comp_db *cdb)                                                                            \
    {                                                                                                                                      \
        return get_comp<comp_name, system_t>(ent_id, cdb);                                                                                 \
    }                                                                                                                                      \
    inline comp_name *get_##comp_name(entity *ent)                                                                                         \
    {                                                                                                                                      \
        return get_comp<comp_name, system_t>(ent);                                                                                         \
    }                                                                                                                                      \
    inline sizet get_##comp_name##_ind(const comp_name *comp, const comp_name##_tbl *ctbl)                                                 \
    {                                                                                                                                      \
        return get_comp_ind<comp_name, system_t>(comp, ctbl);                                                                              \
    }                                                                                                                                      \
    inline sizet get_##comp_name##_ind(const comp_name *comp, const comp_db *cdb)                                                          \
    {                                                                                                                                      \
        return get_comp_ind<comp_name, system_t>(comp, cdb);                                                                               \
    }                                                                                                                                      \
    inline bool remove_##comp_name(u32 ent_id, comp_name##_tbl *ctbl)                                                                      \
    {                                                                                                                                      \
        return remove_comp<comp_name, system_t>(ent_id, ctbl);                                                                             \
    }                                                                                                                                      \
    inline bool remove_##comp_name(u32 ent_id, comp_db *cdb)                                                                               \
    {                                                                                                                                      \
        return remove_comp<comp_name, system_t>(ent_id, cdb);                                                                              \
    }                                                                                                                                      \
    inline bool remove_##comp_name(entity *ent)                                                                                            \
    {                                                                                                                                      \
        return remove_comp<comp_name, system_t>(ent);                                                                                      \
    }

DEFINE_COMP_TBL_TYPE(transform, transform_system);
DEFINE_COMP_TBL_TYPE(camera, null_system_type);
DEFINE_COMP_TBL_TYPE(static_mesh, null_system_type);

void init_static_model(static_mesh *sm, mem_arena *arena);
void terminate_static_model(static_mesh *sm);

sizet add_entities(sizet count, sim_region *reg);
entity *add_entity(const entity &copy, sim_region *reg);
entity *add_entity(const char *name, sim_region *reg);
entity *get_entity(u32 ent_id, sim_region *reg);
bool remove_entity(u32 ent_id, sim_region *reg);
bool remove_entity(entity *ent, sim_region *reg);

void init_sim_region(sim_region *reg, mem_arena *arena);
void terminate_sim_region(sim_region *reg);

} // namespace nslib
