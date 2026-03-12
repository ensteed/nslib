#pragma once
#include "containers/slot_pool.h"
#include "render_defs.h"
#include "math/primitives.h"
#include "math/vector4.h"

#define MPASS_TEXTURE_SA_CCF_DS(phndl, clear_col_float, clear_depth, clear_stencil)                                                        \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t{                                                                                                                                \
            .hndl = phndl,                                                                                                                 \
            .clear_val{                                                                                                                    \
                .type = mpass_clear_value::COLOR_TYPE_FLOAT,                                                                               \
                .fc{clear_col_float},                                                                                                      \
                .depth = clear_depth,                                                                                                      \
                .stencil = clear_stencil,                                                                                                  \
            },                                                                                                                             \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA_CCF(hndl, clear_col_float)                                                                                        \
    MPASS_TEXTURE_SA_CCF_DS(hndl, clear_col_float, DEFAULT_DEPTH_CLEAR, DEFAULT_STENCIL_CLEAR)

#define MPASS_TEXTURE_SA_DS(phndl, clear_depth, clear_stencil)                                                                             \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t{                                                                                                                                \
            .hndl = phndl,                                                                                                                 \
            .clear_val{                                                                                                                    \
                .depth = clear_depth,                                                                                                      \
                .stencil = clear_stencil,                                                                                                  \
            },                                                                                                                             \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA(hndl)                                                                                                             \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t = hndl,                                                                                                                         \
    }

#define MPASS_TEXTURE_SA_CCS_DS(phndl, clear_col_signed_int, clear_depth, clear_stencil)                                                   \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t{                                                                                                                                \
            .t = phndl,                                                                                                                    \
            .clear_val{                                                                                                                    \
                .type = COLOR_TYPE_SINT,                                                                                                   \
                .sc{clear_col_signed_int},                                                                                                 \
                .depth = clear_depth,                                                                                                      \
                .stencil = clear_stencil,                                                                                                  \
            },                                                                                                                             \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA_CCS(hndl, clear_col_signed_int)                                                                                   \
    MPASS_TEXTURE_SA_CCS_DS(hndl, clear_col_signed_int, DEFAULT_DEPTH_CLEAR, DEFAULT_STENCIL_CLEAR)

#define MPASS_TEXTURE_SA_CCU_DS(phndl, clear_col_unsigned_int, clear_depth, clear_stencil)                                                 \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t{                                                                                                                                \
            .t = phndl,                                                                                                                    \
            .clear_val{                                                                                                                    \
                .type = COLOR_TYPE_UINT,                                                                                                   \
                .uc{clear_col_unsigned_int},                                                                                               \
                .depth = clear_depth,                                                                                                      \
                .stencil = clear_stencil,                                                                                                  \
            },                                                                                                                             \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA_CCU(hndl, clear_col_unsigned_int)                                                                                 \
    MPASS_TEXTURE_SA_CCU_DS(hndl, clear_col_unsigned_int, DEFAULT_DEPTH_CLEAR, DEFAULT_STENCIL_CLEAR)

#define MPASS_BUFFER_SA(hndl)                                                                                                              \
    {                                                                                                                                      \
        .type = mslot_target_type::BUFFER,                                                                                                 \
        .t = hndl,                                                                                                                         \
    }

namespace nslib
{

struct renderer;
struct rbuffer_target_fif;
struct rtexture_target_fif;

enum rtechnique_dyn_state_flag
{
    RTECHNIQUE_DYN_STATE_FLAG_CULL_BACK = RTECHNIQUE_FLAG_CULL_BACK,
    RTECHNIQUE_DYN_STATE_FLAG_CULL_FRONT = RTECHNIQUE_FLAG_CULL_FRONT,
    RTECHNIQUE_DYN_STATE_FLAG_STENCIL_TEST = RTECHNIQUE_FLAG_STENCIL_TEST,
};
using rtechnique_dyn_state_flags = u32;

struct rstencil_op_state
{
    rstencil_op on_fail;
    rstencil_op on_pass;
    rstencil_op on_depth_fail;
    rcompare_op comp;
    u32 comp_mask;
    u32 write_mask;
    u32 ref;
};

struct rdepth_bias
{
    float const_factor;
    float slope_factor;
    float clamp;
};

op_eq_func(rdepth_bias) {
    return fequals(lhs.clamp, rhs.clamp) && fequals(lhs.const_factor, rhs.const_factor) && fequals(lhs.slope_factor, rhs.slope_factor);
}

op_neq_func(rdepth_bias);

struct rpline_dyn_state
{
    idx_t last_pline{INVALID_IDX};
    rtechnique_dyn_state_flags dflags;
    rstencil_op_state stencil_front;
    rstencil_op_state stencil_back;
    rdepth_bias depth_b;
    vec4 blend_ops;
    // Should almost never be set to something different
    rfront_face_winding ffw{DEFAULT_FRONT_FACE_WINDING};
};

struct mdraw_ssbo_data {
    idx_t inst;
    idx_t material;
    idx_t view;
    idx_t pass;
};

struct mdraw_params
{
    idx_t inst;
    rgeom_handle geom;
    idx_t subgeom;
    rmaterial_handle mat;
    rtechnique_handle tech;
};

struct mdraw_call
{
    idx_t inst;
    idx_t geom;
    idx_t subgeom;    
    idx_t mat;
    idx_t pl;
    // Dynamic pipeline state
    rpline_dyn_state dstate;
    vec4 scissor_override{};
    // Computed sort key for the draw call - used to sort the draw calls in a render job before submission.
    // Feel free to override for custom sorting
    u64 sort_key;
    u32 inst_count;
};

enum struct mslot_target_type
{
    INVALID,
    TEXTURE,
    BUFFER
};

enum struct rect_size_mode
{
    NORMALIZED,
    ABSOLUTE
};

struct mpass_clear_value
{
    // 0 to 1 color value
    enum color_type
    {
        COLOR_TYPE_FLOAT,
        COLOR_TYPE_SINT,
        COLOR_TYPE_UINT
    } type{COLOR_TYPE_FLOAT};

    union
    {
        vec4 fc;
        svec4 sc;
        uvec4 uc;
    };

    // Depth component
    f32 depth{DEFAULT_DEPTH_CLEAR};
    // Stencil component
    u32 stencil{DEFAULT_STENCIL_CLEAR};
};

struct rtexture_target_meta
{
    rtexture_target_handle hndl;
    mpass_clear_value clear_val{};
};

struct mpass_slot_assignment
{
    // Is this a buffer or texture assignment
    mslot_target_type type{};
    // This specifies the value any attachments set to clear on load will be cleared to.
    union
    {
        rtexture_target_meta t{};
        rbuffer_target_handle b;
    };
};

struct mpass_params
{
    // Render blueprint pass idx
    idx_t rbpp;
    // Which rectangle area type to use
    rect_size_mode rsm{rect_size_mode::NORMALIZED};
    union
    {
        // Coordinates x,y and width/height of the render area normalized to the framebuffer size
        rect norm_render_area{0.0f, 0.0f, 1.0f, 1.0f};
        // Coordinates x,y and width/height of the render area in absolute pixels
        srect render_area;
    };
    mpass_slot_assignment *assignments;
    sizet assignment_count;
    // Pass SSBO data
    const void *pass_sdata;
};

struct mpass
{
    // These need to match exactly in size with the rbp slot count
    static_array<mpass_slot_assignment, MAX_BP_PASS_SLOT_COUNT> slot_assignments;
    // Render blueprint pass idx
    idx_t rbpp;

    // Coordinates x,y and width/height of the render area normalized to the framebuffer size
    // Anything outside this area has no render operation
    rect_size_mode ra_size_mode{rect_size_mode::NORMALIZED};
    union
    {
        rect norm_render_area{0.0f, 0.0f, 1.0f, 1.0f};
        srect render_area;
    };
};

struct mview
{
    // Coordinates x,y and width/height of the viewport normalized to the framebuffer size
    // The scene is "squished" to fit
    rect_size_mode vp_size_mode{rect_size_mode::NORMALIZED};
    rect vp{0.0f, 0.0f, 1.0f, 1.0f};
    // How do we map the NDC to the depth buffer
    vec2 vp_depth_min_max{0.0f, 1.0f};

    // Coordinates x,y and width/height of a scissor normalized to the framebuffer size
    // The scene is cut off by the rasterizer to fit in this window
    rect_size_mode scissor_size_mode{rect_size_mode::NORMALIZED};
    union
    {
        rect norm_scissor{0.0f, 0.0f, 1.0f, 1.0f};
        srect scissor;
    };
};

struct mview_params
{
    // View
    mview vdata;
    // View SSBO data
    const void *view_sdata;
};

struct render_job_cb_params;
using render_job_cb = void(const render_job_cb_params &, void *user);

struct mrender_job
{
    // Manifest pass idx
    idx_t mp;
    // Manifest view idx
    idx_t mv;
    // Per job arena
    mem_arena arena;
    // Job draw calls
    array<mdraw_call> dcs;
    // Sorted draw calls - this is used to fill the instance SSBO
    array<idx_t> sorted_dcs;
    // Instanced draw calls - this is the pared down sorted_dcs with only unique draw calls. Will match sorted draw
    // calls exactly if there is nothing that can be instanced
    array<idx_t> instanced_dcs;
    // Callback draw function
    render_job_cb *cb;
    // Draw function param
    void *cb_user;
};

struct mrender_job_params
{
    idx_t pass;
    idx_t view;
    u32 max_draw_calls;
    render_job_cb *cb;
    void *cb_user;
};

struct mframe_params
{
    double dt;
};

struct rmanifest
{
    renderer *rndr;
    idx_t fif;
    render_blueprint_ref rbp;
    array<rbuffer_target_fif> buffers;
    array<rtexture_target_fif> textures;

    mframe_params fp;
    array<mpass> passes;
    array<mview> views;
    array<mrender_job> jobs;
};

struct rframe_begin_params
{
    // Blueprint handle for this frame
    render_blueprint_ref rbp;
    // Frame SSBO data
    const void *frame_sdata;
};

struct manifest_max_counts
{
    u32 passes;
    u32 views;
    u32 render_jobs;
    u32 draw_calls_per_job;
    u32 texture_targets;
    u32 buffer_targets;
};

// Draw funcs
#if defined USE_IMGUI
void draw_imgui(const render_job_cb_params &, void *);
#endif
void draw_geometry(const render_job_cb_params &, void *);

rmanifest *begin_render_frame(renderer *rndr, const rframe_begin_params &p);
bool end_render_frame(rmanifest *m);

void update_instance_data(rmanifest *rm, void *block, sizet block_size);
void update_material_data(rmanifest *rm, void *block, sizet block_size);

idx_t push_pass(rmanifest *m, const mpass_params &p);
u32 push_slot_assignment(rmanifest *m, idx_t pid, const mpass_slot_assignment &sa);
idx_t push_view(rmanifest *m, const mview_params &p);
idx_t push_render_job(rmanifest *m, const mrender_job_params &p);
u32 push_draw(rmanifest *m, const mdraw_params &dp);

} // namespace nslib
