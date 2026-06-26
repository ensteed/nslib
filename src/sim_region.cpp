#include "sim_region.h"

namespace nslib
{

void set_camera_fov(camera *cam, f32 fov, camera_fov_type ft)
{
    cam->fov = fov;
    cam->fov_type = ft;
    mark_comp_dirty(cam);
}

void set_camera_near_far(camera *cam, const vec2 &near_far)
{
    cam->near_far = near_far;
    mark_comp_dirty(cam);
}

void set_camera_aspect(camera *cam, f32 ar)
{
    cam->aspect = ar;
    mark_comp_dirty(cam);
}

void set_camera_proj_type(camera *cam, camera_proj_type pt)
{
    cam->ptype = pt;
    mark_comp_dirty(cam);
}

intern void handle_dirty_proc(transform *tf)
{
    if (!is_comp_dirty(tf)) {
        tf->prev = tf->current;
        mark_comp_dirty(tf);
    }
}

void set_transform_pos(transform *tf, const vec3 &pos)
{
    handle_dirty_proc(tf);
    tf->current.world_pos = pos;
}

void set_transform_orientation(transform *tf, const quat &q)
{
    handle_dirty_proc(tf);
    tf->current.orientation = q;
}

void set_transform_scale(transform *tf, const vec3 &s)
{
    mark_comp_dirty(tf);
    tf->current.scale = s;
}

void init_static_model(static_mesh *sm, mem_arena *arena)
{
    arr_init(&sm->mat_mapping, arena);
}

void terminate_static_model(static_mesh *sm)
{
    arr_terminate(&sm->mat_mapping);
}

void init_comp_db(comp_db *cdb, mem_arena *arena)
{
    arr_init(&cdb->comp_tables, arena);
}

void terminate_comp_db(comp_db *cdb)
{
    arr_terminate(&cdb->comp_tables);
}

sizet add_entities(sizet count, sim_region *reg)
{
    sizet ind = reg->ents.size;
    arr_resize(&reg->ents, ind + count);
    for (sizet i = 0; i < count; ++i) {
        reg->ents[ind + i].id = ++reg->last_id;
        reg->ents[ind + i].cdb = &reg->cdb;
        auto iter = hmap_insert(&reg->entmap, reg->ents[ind + i].id, ind + i);
        asrt(iter);
    }
    return ind;
}

entity *add_entity(const entity &copy, sim_region *reg)
{
    sizet ind = reg->ents.size;
    arr_emplace_back(&reg->ents, copy);
    reg->ents[ind].id = ++reg->last_id;
    reg->ents[ind].cdb = &reg->cdb;
    auto iter = hmap_insert(&reg->entmap, reg->ents[ind].id, ind);
    asrt(iter);
    return &reg->ents[ind];
}

entity *add_entity(const char *name, sim_region *reg)
{
    sizet ind = reg->ents.size;
    arr_emplace_back(&reg->ents, entity{++reg->last_id, name, &reg->cdb});
    auto iter = hmap_insert(&reg->entmap, reg->ents[ind].id, ind);
    asrt(iter);
    return &reg->ents[ind];
}

entity *get_entity(u32 ent_id, sim_region *reg)
{
    auto fiter = hmap_find(&reg->entmap, ent_id);
    if (fiter) {
        return &reg->ents[fiter->val];
    }
    return nullptr;
}

bool remove_entity(u32 ent_id, sim_region *reg)
{
    sizet ent_ind{};

    auto rem = hmap_remove(&reg->entmap, ent_id, &ent_ind);
    if (!rem) {
        return rem;
    }
    asrt(ent_ind != INVALID_ID);
    asrt(ent_ind < reg->ents.size);
    // Remove all components for this entity
    for (u32 cti = 0; cti < reg->cdb.comp_tables.size; ++cti) {
        reg->cdb.comp_tables[cti].rem_func(ent_id, &reg->cdb);
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

bool remove_entity(entity *ent, sim_region *reg)
{
    return remove_entity(ent->id, reg);
}

void init_sim_region(sim_region *reg, mem_arena *arena)
{
    arr_init(&reg->ents, arena);
    init_comp_db(&reg->cdb, arena);
    hmap_init(&reg->entmap, hash_type, arena);

    add_comp_tbl<static_mesh>(&reg->cdb);
    add_comp_tbl<camera>(&reg->cdb, 64);
    add_comp_tbl<transform>(&reg->cdb, 5000);
}

void terminate_sim_region(sim_region *reg)
{
    remove_comp_tbl<transform>(&reg->cdb);
    remove_comp_tbl<camera>(&reg->cdb);
    remove_comp_tbl<static_mesh>(&reg->cdb);

    hmap_terminate(&reg->entmap);
    terminate_comp_db(&reg->cdb);
    arr_terminate(&reg->ents);
}
} // namespace nslib
