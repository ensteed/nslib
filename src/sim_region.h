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
    COMP_TYPE_USER
};

enum comp_flag : u32
{
    COMP_FLAG_DIRTY = make_flag(0),
    COMP_FLAG_USER_BASE = make_flag(1),
};
using comp_flags = u64;

enum transform_flag : u32
{
};

#define COMP(type)                                                                                                                         \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = COMP_TYPE_##type;                                                                                 \
    u32 ent_id;                                                                                                                            \
    u64 flags;

#define PUP_COMP_COMMON                                                                                                                    \
    pup_member(ent_id);                                                                                                                    \
    pup_member(flags)

struct transform
{
    COMP(TRANSFORM)
    mat4 cached_prev;
    mat4 cached;
    vec3 world_pos;
    quat orientation;
    vec3 scale{1};
    u32 rfif_dirty;
};

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

template<typename T>
struct comp_table
{
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

void set_transform_pos(transform *tf, const vec3 &pos);
void set_transform_orientation(transform *tf, const quat &q);
void set_transform_scale(transform *tf, const vec3 &s);

template<typename T>
void init_comp_tbl(comp_table<T> *tbl, mem_arena *arena, sizet initial_capacity)
{
    arr_init(&tbl->entries, arena, initial_capacity);
    hmap_init(&tbl->entc_hm, hash_type, arena);
}

template<typename T>
void terminate_comp_tbl(comp_table<T> *tbl)
{
    hmap_terminate(&tbl->entc_hm);
    arr_terminate(&tbl->entries);
}

template<typename T>
bool remove_comp(u32 ent_id, comp_db *cdb);

template<typename T>
comp_table<T> *add_comp_tbl(comp_db *cdb, sizet initial_capacity = 64)
{
    if ((T::type_id + 1) > cdb->comp_tables.size) {
        arr_resize(&cdb->comp_tables, T::type_id + 1);
    }
    if (!cdb->comp_tables[T::type_id].tbl) {
        auto ctbl = mem_calloc<comp_table<T>>(1, cdb->comp_tables.arena);
        init_comp_tbl(ctbl, cdb->comp_tables.arena, initial_capacity);
        cdb->comp_tables[T::type_id].tbl = ctbl;
        cdb->comp_tables[T::type_id].rem_func = remove_comp<T>;
    }
    return (comp_table<T> *)cdb->comp_tables[T::type_id].tbl;
}

template<typename T>
comp_table<T> *get_comp_tbl(comp_db *cdb)
{
    if (T::type_id < cdb->comp_tables.size) {
        return (comp_table<T> *)cdb->comp_tables[T::type_id].tbl;
    }
    return nullptr;
}

template<typename T>
const comp_table<T> *get_comp_tbl(const comp_db *cdb)
{
    if (T::type_id < cdb->comp_tables.size) {
        return (comp_table<T> *)cdb->comp_tables[T::type_id].tbl;
    }
    return nullptr;
}

template<typename T>
bool remove_comp_tbl(comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T>(cdb);
    if (ctbl) {
        terminate_comp_tbl(ctbl);
        mem_free(ctbl, cdb->comp_tables.arena);
        cdb->comp_tables[T::type_id] = {};
        return true;
    }
    return false;
}

void init_comp_db(comp_db *cdb, mem_arena *arena);
void terminate_comp_db(comp_db *cdb);

template<typename T>
T *add_comp(u32 ent_id, comp_table<T> *ctbl, const T &copy = {})
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

template<typename T>
T *add_comp(u32 ent_id, comp_db *cdb, const T &copy = {})
{
    auto ctbl = get_comp_tbl<T>(cdb);
    return add_comp<T>(ent_id, ctbl, copy);
}

template<typename T>
T *add_comp(entity *ent, const T &copy = {})
{
    return add_comp<T>(ent->id, ent->cdb, copy);
}

template<typename T>
T *get_comp(u32 ent_id, comp_table<T> *ctbl)
{
    auto fiter = hmap_find(&ctbl->entc_hm, ent_id);
    if (!fiter) {
        return nullptr;
    }
    return &ctbl->entries[fiter->val];
}

template<typename T>
T *get_comp(u32 ent_id, comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T>(cdb);
    return get_comp(ent_id, ctbl);
}

template<typename T>
T *get_comp(entity *ent)
{
    return get_comp<T>(ent->id, ent->cdb);
}

template<typename T>
sizet get_comp_ind(const T *comp, const comp_table<T> *ctbl)
{
    return (comp - ctbl->entries.data);
}

template<typename T>
sizet get_comp_ind(const T *comp, const comp_db *cdb)
{
    return get_comp_ind(comp, get_comp_tbl<T>(cdb));
}

template<typename T>
bool remove_comp(u32 ent_id, comp_table<T> *ctbl)
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

template<typename T>
bool remove_comp(u32 ent_id, comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T>(cdb);
    return remove_comp(ent_id, ctbl);
}

template<typename T>
bool remove_comp(entity *ent)
{
    return remove_comp<T>(ent->id, ent->cdb);
}

template<typename T>
void mark_comp_dirty(T *comp)
{
    set_flags(comp->flags, COMP_FLAG_DIRTY);
}

template<typename T>
bool is_comp_dirty(const T &comp)
{
    return test_flags(comp.flags, COMP_FLAG_DIRTY);
}

using transform_tbl = comp_table<transform>;
using camera_tbl = comp_table<camera>;
using static_mesh_tbl = comp_table<static_mesh>;

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
