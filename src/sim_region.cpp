#include "math/quaternion.h"
#include "sim_region.h"

namespace nslib
{
void set_camera_fov(camera_tbl *tbl, idx_t cam, f32 fov, camera_fov_type ft)
{
    tbl->entries[cam].fov = fov;
    tbl->entries[cam].fov_type = ft;
    mark_comp_updated(tbl, cam);
}

void set_camera_near_far(camera_tbl *tbl, idx_t cam, const vec2 &near_far)
{
    tbl->entries[cam].near_far = near_far;
    mark_comp_updated(tbl, cam);
}

void set_camera_aspect(camera_tbl *tbl, idx_t cam, f32 ar)
{
    tbl->entries[cam].aspect = ar;
    mark_comp_updated(tbl, cam);
}

void set_camera_proj_type(camera_tbl *tbl, idx_t cam, camera_proj_type pt)
{
    tbl->entries[cam].ptype = pt;
    mark_comp_updated(tbl, cam);
}

void update_transform_pos(transform_tbl *tbl, idx_t tform, const vec3 &pos)
{
    auto tf = &tbl->entries[tform];
    if (mark_comp_updated(tbl, tform)) {
        tf->prev = tf->current;
    }
    tf->current.world_pos = pos;
}

void update_transform_orientation(transform_tbl *tbl, idx_t tform, const quat &q)
{
    auto tf = &tbl->entries[tform];
    if (mark_comp_updated(tbl, tform)) {
        tf->prev = tf->current;
    }
    tf->current.orientation = q;
}

void update_transform_scale(transform_tbl *tbl, idx_t tform, const vec3 &s)
{
    auto tf = &tbl->entries[tform];
    if (mark_comp_updated(tbl, tform)) {
        tf->prev = tf->current;
    }
    tf->current.scale = s;
}

transform_trs interpolate_trs(const transform_trs &prev, const transform_trs &current, f32 alpha)
{
    return {
        .world_pos = math::lerp(prev.world_pos, current.world_pos, alpha),
        .orientation = math::nlerp(prev.orientation, current.orientation, alpha),
        .scale = math::lerp(prev.scale, current.scale, alpha),
    };
}

mat4 build_mat4_from_trs(const transform_trs &trs)
{
    return math::model_tform(trs.world_pos, trs.orientation, trs.scale);
}

mat4 interpolate_tranform(const transform &tf, f32 alpha)
{
    return build_mat4_from_trs(interpolate_trs(tf.prev, tf.current, alpha));
}

void init_static_model(static_mesh *sm, mem_arena *arena)
{
    arr_init(&sm->mat_mapping, arena);
}

void init_comp(static_mesh *sm, mem_arena *arena)
{
    // add_comp can copy a populated component in - only claim an arena if there isn't one
    if (!sm->mat_mapping.arena) {
        init_static_model(sm, arena);
    }
}

void terminate_static_model(static_mesh *sm)
{
    arr_terminate(&sm->mat_mapping);
}

intern void init_comp_db(comp_db *cdb, mem_arena *arena)
{
    arr_init(&cdb->comp_tables, arena);
}

intern void terminate_comp_db(comp_db *cdb)
{
    arr_terminate(&cdb->comp_tables);
}

sizet add_entities(sim_region *reg, sizet count)
{
    idx_t ind = reg->ents.size;
    arr_resize(&reg->ents, ind + count);
    for (idx_t i = 0; i < count; ++i) {
        reg->ents[ind + i].id = ++reg->last_id;
        reg->ents[ind + i].cdb = &reg->cdb;
        auto iter = hmap_insert(&reg->entmap, reg->ents[ind + i].id, ind + i);
        asrt(iter);
    }
    return ind;
}

entity *add_entity(sim_region *reg, const entity &copy)
{
    idx_t ind = reg->ents.size;
    arr_emplace_back(&reg->ents, copy);
    reg->ents[ind].id = ++reg->last_id;
    reg->ents[ind].cdb = &reg->cdb;
    auto iter = hmap_insert(&reg->entmap, reg->ents[ind].id, ind);
    asrt(iter);
    return &reg->ents[ind];
}

entity *add_entity(sim_region *reg, const char *name)
{
    idx_t ind = reg->ents.size;
    // Name lives in the same arena as the array that owns the entity
    arr_emplace_back(&reg->ents, entity{++reg->last_id, string(reg->ents.arena, name), &reg->cdb});
    auto iter = hmap_insert(&reg->entmap, reg->ents[ind].id, ind);
    asrt(iter);
    return &reg->ents[ind];
}

entity *get_entity(sim_region *reg, ent_id entity)
{
    auto fiter = hmap_find(&reg->entmap, entity);
    if (fiter) {
        return &reg->ents[fiter->val];
    }
    return nullptr;
}

bool remove_entity(sim_region *reg, ent_id entity)
{
    idx_t ent_ind{};
    auto rem = hmap_remove(&reg->entmap, entity, &ent_ind);
    if (!rem) {
        return rem;
    }
    asrt(ent_ind != INVALID_IDX);
    asrt(ent_ind < reg->ents.size);
    // Remove all components for this entity
    for (u32 cti = 0; cti < reg->cdb.comp_tables.size; ++cti) {
        reg->cdb.comp_tables[cti].rem_func(&reg->cdb, entity);
    }
    // Remove the entity
    if (arr_swap_remove(&reg->ents, ent_ind)) {
        if (ent_ind < reg->ents.size) {
            // Update the entry for this entity that has been swapped to have the correct index in to the entity array
            hmap_set(&reg->entmap, reg->ents[ent_ind].id, ent_ind);
        }
        return true;
    }
    return false;
}

bool remove_entity(sim_region *reg, entity *ent)
{
    return remove_entity(reg, ent->id);
}

void init_sim_region(sim_region *reg, mem_arena *arena)
{
    arr_init(&reg->ents, arena);
    init_comp_db(&reg->cdb, arena);
    hmap_init(&reg->entmap, arena);

    add_static_mesh_tbl(&reg->cdb);
    add_camera_tbl(&reg->cdb, 64);
    add_transform_tbl(&reg->cdb, 5000);
}

void terminate_sim_region(sim_region *reg)
{
    remove_transform_tbl(&reg->cdb);
    remove_camera_tbl(&reg->cdb);
    remove_static_mesh_tbl(&reg->cdb);

    hmap_terminate(&reg->entmap);
    terminate_comp_db(&reg->cdb);
    arr_terminate(&reg->ents);
}
} // namespace nslib
