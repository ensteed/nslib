#pragma once
#include "renderer.h"

namespace nslib
{
struct drawable_update
{
    rdrawable_handle hndl;
    rdrawable d;
};

struct transform_update
{
    rtransform_handle hndl;
    mat4 transform;
};

struct material_update
{
    rmaterial_handle hndl;
    rdraw_dyn_state dstate;
    rdraw_state_override_flags override_mask;
    static_array<rmaterial_bp_technique, MAX_BP_COUNT> bp_techs;
};

struct viewport_snapshot {
    
};

struct sim_snapshot
{
    mem_arena arena;
    s64 step_anchor_ns;
    u32 step_count;
    f32 dt;
    f64 elapsed;
    void *imgui_data;
    rblueprint_ref rbp; // becomes a handle after step 4

    material_update *mat_updates;
    u32 mat_update_count;

    drawable_update *draw_updates;
    u32 draw_update_count;

    transform_update *tf_updates;
    u32 tf_update_count;
};
} // namespace nslib
