#pragma once

#include "math/matrix4.h"
#include "containers/slot_allocator.h"
#include "math/primitives.h"
#include "rid.h"
#include "render_defs.h"
#include "containers/slot_pool.h"
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
    COMP_USER_BASE_BIT
};

using comp_flags = u64;
using ent_id = u32;

#define COMP(type)                                                                                                                         \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = COMP_TYPE_##type;                                                                                 \
    ent_id entity;                                                                                                                         \
    u64 flags;                                                                                                                             \
    u32 step_updated;                                                                                                                      \
    idx_t active_idx{INVALID_IDX}; // If in the active set, this will be the index in to the array

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
    inline const comp_name##_tbl *get_##comp_name##_tbl(const comp_db &cdb)                                                                \
    {                                                                                                                                      \
        return get_comp_tbl<comp_name, system_t>(cdb);                                                                                     \
    }                                                                                                                                      \
    inline bool remove_##comp_name##_tbl(comp_db *cdb)                                                                                     \
    {                                                                                                                                      \
        return remove_comp_tbl<comp_name, system_t>(cdb);                                                                                  \
    }                                                                                                                                      \
    inline idx_t add_##comp_name(comp_name##_tbl *ctbl, ent_id entity, const comp_name &copy = {})                                         \
    {                                                                                                                                      \
        return add_comp<comp_name, system_t>(ctbl, entity, copy);                                                                          \
    }                                                                                                                                      \
    inline idx_t add_##comp_name(comp_db *cdb, ent_id entity, const comp_name &copy = {})                                                  \
    {                                                                                                                                      \
        return add_comp<comp_name, system_t>(cdb, entity, copy);                                                                           \
    }                                                                                                                                      \
    inline idx_t add_##comp_name(entity *ent, const comp_name &copy = {})                                                                  \
    {                                                                                                                                      \
        return add_comp<comp_name, system_t>(ent, copy);                                                                                   \
    }                                                                                                                                      \
    inline idx_t find_##comp_name(const comp_name##_tbl *ctbl, ent_id entity)                                                              \
    {                                                                                                                                      \
        return find_comp<comp_name, system_t>(ctbl, entity);                                                                               \
    }                                                                                                                                      \
    inline idx_t find_##comp_name(const comp_db &cdb, ent_id entity)                                                                       \
    {                                                                                                                                      \
        return find_comp<comp_name, system_t>(cdb, entity);                                                                                \
    }                                                                                                                                      \
    inline idx_t find_##comp_name(const entity &ent)                                                                                       \
    {                                                                                                                                      \
        return find_comp<comp_name, system_t>(ent);                                                                                        \
    }                                                                                                                                      \
    inline comp_name *get_##comp_name(comp_name##_tbl *ctbl, idx_t comp_idx)                                                               \
    {                                                                                                                                      \
        return get_comp(ctbl, comp_idx);                                                                                                   \
    }                                                                                                                                      \
    inline const comp_name *get_##comp_name(const comp_name##_tbl &ctbl, idx_t comp_idx)                                                   \
    {                                                                                                                                      \
        return get_comp(ctbl, comp_idx);                                                                                                   \
    }                                                                                                                                      \
    inline sizet get_##comp_name##_ind(const comp_name##_tbl *ctbl, const comp_name &comp)                                                 \
    {                                                                                                                                      \
        return get_comp_ind<comp_name, system_t>(ctbl, comp);                                                                              \
    }                                                                                                                                      \
    inline sizet get_##comp_name##_ind(const comp_db &cdb, const comp_name &comp)                                                          \
    {                                                                                                                                      \
        return get_comp_ind<comp_name, system_t>(cdb, comp);                                                                               \
    }                                                                                                                                      \
    inline bool remove_##comp_name(comp_name##_tbl *ctbl, ent_id entity)                                                                   \
    {                                                                                                                                      \
        return remove_comp<comp_name, system_t>(ctbl, entity);                                                                             \
    }                                                                                                                                      \
    inline bool remove_##comp_name(comp_db *cdb, ent_id entity)                                                                            \
    {                                                                                                                                      \
        return remove_comp<comp_name, system_t>(cdb, entity);                                                                              \
    }                                                                                                                                      \
    inline bool remove_##comp_name(entity *ent)                                                                                            \
    {                                                                                                                                      \
        return remove_comp<comp_name, system_t>(ent);                                                                                      \
    }

#define PUP_COMP_COMMON                                                                                                                    \
    pup_member(entity);                                                                                                                    \
    pup_member(flags)

struct transform_trs
{
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
    rtransform_handle rhndl;
};

struct transform_system
{
    // Nothing here yet
};

void init_comp_system(transform_system *sys, mem_arena *arena, sizet initial_capacity);
void terminate_comp_system(transform_system *sys);

struct material_subgeom_mapping
{
    rid mat_id;
    u32 sm_mat_slot;
};

struct static_mesh
{
    COMP(STATIC_MESH)
    rid geom_id;
    array<material_subgeom_mapping> mat_mapping{};
    rdrawable_handle rhndl;
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
    camera_proj_type ptype{CAMERA_PROJ_TYPE_PERSPECTIVE};
    camera_fov_type fov_type{CAMERA_FOV_TYPE_VERTICAL};

    // Cached - remade on update no need to serialize
    mat4 proj;
    mat4 view;
    mat4 proj_view;
    mat4 inv_proj_view;
};

pup_func(camera)
{
    PUP_COMP_COMMON;
    pup_member(fov);
    pup_member(aspect);
    pup_member(near_far);
    pup_member(proj);
    pup_enum_member(camera_proj_type, u8, ptype);
    pup_enum_member(camera_fov_type, u8, fov_type);
}

struct comp_active_set
{
    array<idx_t> items;
    // A pointer to the simulation step variable. If null, items won't be placed in the active set on updating.
    // Basically mark_comp_updated just immediately returns false. When using comp_table with a comp_db, add_comp_tbl
    // will set this to point at the comp_db's current step.
    const u32 *cur_step;
};

enum view_target_type
{
    VIEW_TARGET_TYPE_TEXTURE,
    VIEW_TARGET_TYPE_BUFFER,
};

struct view_target_clear_value
{
    vec4 color;
    f32 depth;
    u32 stencil;
};

pup_func(view_target_clear_value)
{
    pup_member(color);
    pup_member(depth);
    pup_member(stencil);
}

struct view_target_assignment
{
    small_str rbp_slot_name;
    view_target_type type;
    rid target_id;
    view_target_clear_value clear_val;
};

pup_func(view_target_assignment)
{
    pup_member(rbp_slot_name);
    pup_member(type);
    pup_member(target_id);
    pup_member(clear_val);
}

struct scene_viewport_pass
{
    rid bp_pass;
    rid draw_cb; // optional; invalid = use the pass default
    rect render_area{0.0f, 0.0f, 1.0f, 1.0f};
    static_array<view_target_assignment, MAX_BP_PASS_SLOT_COUNT> targets;
};

pup_func(scene_viewport_pass)
{
    pup_member(bp_pass);
    pup_member(draw_cb);
    pup_member(targets);
}

struct scene_viewport
{
    rid bp;
    ent_id camera;
    rect vp{0.0f, 0.0f, 1.0f, 1.0f};
    vec2 depth_mm;
    rect scissor{0.0f, 0.0f, 1.0f, 1.0f};
    u32 layer_mask;
    static_array<scene_viewport_pass, MAX_VIEWPORT_PASS_COUNT> passes;
};

pup_func(scene_viewport)
{
    pup_member(bp);
    pup_member(camera);
    pup_member(passes);
}

template<typename T, typename SysData>
struct comp_table
{
    using CompType = T;
    using SysType = SysData;

    SysData sys;
    array<T> entries;
    hmap<ent_id, sizet> entc_hm;
    comp_active_set aset;
};

struct comp_table_cfg
{
    const u32 *cur_step;
    mem_arena *arena;
    sizet start_capacity;
};

struct comp_db;
using remove_func = bool(comp_db *cdb, ent_id entity);

struct comp_table_entry
{
    void *tbl;
    remove_func *rem_func;
};

struct comp_db
{
    array<comp_table_entry> comp_tables;
    u32 cur_step;
};

struct entity
{
    ent_id id;
    string name;
    comp_db *cdb;
};

struct null_system_type
{};

struct sim_region
{
    array<entity> ents;
    hmap<ent_id, idx_t> entmap;
    comp_db cdb;
    ent_id last_id{};
};

template<typename TableType>
void init_comp_system(const TableType *tbl, typename TableType::SysType *system)
{
    // Do nothing by default
}

template<typename TableType>
void terminate_comp_system(const TableType *tbl, typename TableType::SysType *sys)
{
    // Do nothing by default
}

template<typename T, typename SysData>
void init_comp_tbl(comp_table<T, SysData> *tbl, const comp_table_cfg &cfg)
{
    arr_init(&tbl->entries, cfg.arena, cfg.start_capacity);
    arr_init(&tbl->aset.items, cfg.arena, cfg.start_capacity);
    hmap_init(&tbl->entc_hm, cfg.arena);
    tbl->aset.cur_step = cfg.cur_step;
    init_comp_system(tbl, &tbl->sys);
}

template<typename T, typename SysData>
void terminate_comp_tbl(comp_table<T, SysData> *tbl)
{
    terminate_comp_system(tbl, &tbl->sys);
    hmap_terminate(&tbl->entc_hm);
    arr_terminate(&tbl->aset.items);
    arr_terminate(&tbl->entries);
}

template<typename T, typename SysData>
bool remove_comp(comp_db *cdb, ent_id entity);

template<typename T, typename SysData>
comp_table<T, SysData> *add_comp_tbl(comp_db *cdb, sizet start_capacity = 64)
{
    if ((T::type_id + 1) > cdb->comp_tables.size) {
        arr_resize(&cdb->comp_tables, T::type_id + 1);
    }
    if (!cdb->comp_tables[T::type_id].tbl) {
        auto ctbl = mem_calloc<comp_table<T, SysData>>(1, cdb->comp_tables.arena);
        init_comp_tbl(ctbl, {.cur_step = &cdb->cur_step, .arena = cdb->comp_tables.arena, .start_capacity = start_capacity});
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
const comp_table<T, SysData> *get_comp_tbl(const comp_db &cdb)
{
    if (T::type_id < cdb.comp_tables.size) {
        return (const comp_table<T, SysData> *)cdb.comp_tables[T::type_id].tbl;
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

// Per component construction hook - components that own containers give them an arena here. Mirrors
// init_asset. Default is a no-op for components that own nothing.
template<typename T>
inline void init_comp(T *, mem_arena *)
{}
void init_comp(static_mesh *sm, mem_arena *arena);

template<typename T, typename SysData>
idx_t add_comp(comp_table<T, SysData> *ctbl, ent_id entity, const T &copy = {})
{
    idx_t cid = (idx_t)ctbl->entries.size;
    auto item = hmap_insert(&ctbl->entc_hm, entity, (sizet)cid);
    if (!item) {
        return INVALID_IDX;
    }
    arr_push_back(&ctbl->entries, copy);
    auto comp = &ctbl->entries[cid];
    comp->entity = entity;
    // The component's owned containers come out of the same arena as the table that holds it
    init_comp(comp, ctbl->entries.arena);
    return cid;
}

template<typename T, typename SysData>
idx_t add_comp(comp_db *cdb, ent_id entity, const T &copy = {})
{
    auto ctbl = get_comp_tbl<T, SysData>(cdb);
    return add_comp<T, SysData>(ctbl, entity, copy);
}

template<typename T, typename SysData>
idx_t add_comp(entity *ent, const T &copy = {})
{
    return add_comp<T, SysData>(ent->cdb, ent->id, copy);
}

template<typename T, typename SysData>
idx_t find_comp(const comp_table<T, SysData> *ctbl, ent_id entity)
{
    auto fiter = hmap_find(&ctbl->entc_hm, entity);
    if (!fiter) {
        return INVALID_IDX;
    }
    return (idx_t)fiter->val;
}

template<typename T, typename SysData>
idx_t find_comp(const comp_db &cdb, ent_id entity)
{
    auto ctbl = get_comp_tbl<T, SysData>(cdb);
    return find_comp(ctbl, entity);
}

template<typename T, typename SysData>
idx_t find_comp(const entity &ent)
{
    return find_comp<T, SysData>(*ent.cdb, ent.id);
}

template<typename T, typename SysData>
T *get_comp(comp_table<T, SysData> *ctbl, idx_t comp_idx)
{
    asrt(comp_idx < ctbl->entries.size);
    return &ctbl->entries[comp_idx];
}

template<typename T, typename SysData>
const T *get_comp(const comp_table<T, SysData> &ctbl, idx_t comp_idx)
{
    asrt(comp_idx < ctbl.entries.size);
    return &ctbl.entries[comp_idx];
}

template<typename T, typename SysData>
sizet get_comp_ind(const comp_table<T, SysData> *ctbl, const T &comp)
{
    return ((&comp) - ctbl->entries.data);
}

template<typename T, typename SysData>
sizet get_comp_ind(const comp_db &cdb, const T &comp)
{
    return get_comp_ind(get_comp_tbl<T, SysData>(cdb), comp);
}

template<typename T, typename SysData>
bool remove_comp(comp_table<T, SysData> *ctbl, ent_id entity)
{
    auto fiter = hmap_find(&ctbl->entc_hm, entity);
    if (!fiter) return false;
    if (arr_swap_remove(&ctbl->entries, fiter->val)) {
        if (fiter->val < ctbl->entries.size) {
            hmap_set(&ctbl->entc_hm, ctbl->entries[fiter->val].entity, fiter->val);
        }
        hmap_erase(&ctbl->entc_hm, fiter);
        return true;
    }
    return false;
}

template<typename T, typename SysData>
bool remove_comp(comp_db *cdb, ent_id entity)
{
    auto ctbl = get_comp_tbl<T, SysData>(cdb);
    return remove_comp(ctbl, entity);
}

template<typename T, typename SysData>
bool remove_comp(entity *ent)
{
    return remove_comp<T, SysData>(ent->cdb, ent->id);
}

DEFINE_COMP_TBL_TYPE(transform, transform_system);
DEFINE_COMP_TBL_TYPE(camera, null_system_type);
DEFINE_COMP_TBL_TYPE(static_mesh, null_system_type);

template<typename CompTable>
bool mark_comp_updated(CompTable *tbl, idx_t comp_idx)
{
    if (!tbl->aset.cur_step) return false;
    auto comp = &tbl->entries[comp_idx];
    // Was this the first update during this step
    bool first_this_step = comp->step_updated != *tbl->aset.cur_step;
    comp->step_updated = *tbl->aset.cur_step;
    if (!is_valid(comp->active_idx)) {
        comp->active_idx = (idx_t)tbl->aset.items.size;
        arr_push_back(&tbl->aset.items, comp_idx);
    }
    return first_this_step;
}

void init_static_model(static_mesh *sm, mem_arena *arena);
void terminate_static_model(static_mesh *sm);

sizet add_entities(sim_region *reg, sizet count);
entity *add_entity(sim_region *reg, const entity &copy);
entity *add_entity(sim_region *reg, const char *name);
entity *get_entity(sim_region *reg, ent_id entity);
bool remove_entity(sim_region *reg, ent_id entity);
bool remove_entity(sim_region *reg, entity *ent);

void init_sim_region(sim_region *reg, mem_arena *arena);
void terminate_sim_region(sim_region *reg);

void set_camera_fov(camera_tbl *tbl, idx_t cam, f32 fov, camera_fov_type ft = CAMERA_FOV_TYPE_VERTICAL);
void set_camera_near_far(camera_tbl *tbl, idx_t cam, const vec2 &near_far);
void set_camera_aspect(camera_tbl *tbl, idx_t cam, f32 ar);
void set_camera_proj_type(camera_tbl *tbl, idx_t cam, camera_proj_type pt);

void update_transform_pos(transform_tbl *tbl, idx_t tform, const vec3 &pos);
void update_transform_orientation(transform_tbl *tbl, idx_t tform, const quat &q);
void update_transform_scale(transform_tbl *tbl, idx_t tform, const vec3 &s);

transform_trs interpolate_trs(const transform_trs &prev, const transform_trs &current, f32 alpha);
mat4 build_mat4_from_trs(const transform_trs &trs);
mat4 interpolate_tranform(const transform &tf, f32 alpha);

} // namespace nslib
