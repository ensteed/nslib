#pragma once
#include "render_shader_data.h"
#include "renderer.h"

namespace nslib
{
struct drawable_ss_update
{
    rdrawable_handle hndl;
    rdrawable d;
};

struct transform_ss_update
{
    rtransform_handle hndl;
    mat4 transform;
};

struct material_ss_update
{
    rmaterial_handle hndl;
    rdraw_dyn_state dstate;
    rdraw_state_override_flags override_mask;
    static_array<rmaterial_bp_technique, MAX_BP_COUNT> bp_techs;
};

struct viewport_ss_entry_pass {
    idx_t rbpp;    
    rect rndr_area;
    static_array<mpass_slot_assignment, MAX_BP_PASS_SLOT_COUNT> targets;
    render_job_cb* draw_cb; // optional; invalid = use the pass default
    void *cb_user;
};

struct viewport_ss_entry {
    rblueprint_handle rbp;
    mview vdata;
    view_ssbo_data sdata;
    static_array<viewport_ss_entry_pass, MAX_VIEWPORT_PASS_COUNT> passes;
};

struct sim_snapshot
{
    mem_arena arena;
    s64 step_anchor_ns;
    u32 step_count;
    f32 dt;
    f64 elapsed;
    void *imgui_data;

    material_ss_update *mat_updates;
    u32 mat_update_count;

    drawable_ss_update *draw_updates;
    u32 draw_update_count;

    transform_ss_update *tf_updates;
    u32 tf_update_count;

    viewport_ss_entry *viewports;
    u32 viewport_ss_count;
};

void build_manifest(rmanifest *m, const sim_snapshot &ss);
} // namespace nslib
