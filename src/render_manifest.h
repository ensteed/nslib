#pragma once
#include "containers/slot_pool.h"
#include "render_defs.h"
#include "math/primitives.h"
#include "math/matrix4.h"

namespace nslib
{

struct renderer;

struct rtexture_assignment
{
    u32 unit;
    rtexture_handle tex;
};

struct mdraw_params
{
    rgeom_handle geom;
    rmaterial_handle mat;
    rtechnique_handle technique;
    const rtexture_assignment *tex_assignments;
    sizet tex_assignment_count;
    instance_id iid;
};

struct mdraw_call
{
    rgeom_handle geom;
    rmaterial_handle mat;
    gpu_handle pipeline;
    static_array<rtexture_handle, RMATERIAL_TEXTURE_COUNT> textures;
    instance_id iid;
    // Mostly for UI
    vec4 scissor_override{};
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

struct mpass_slot_assignment
{
    // Is this a buffer or texture assignment
    mslot_target_type type{};
    // This specifies the value any attachments set to clear on load will be cleared to.
    union
    {
        struct
        {
            rtexture_target_handle t;
            mpass_clear_value clear_val{};
        };
        rbuffer_target_handle b;
    };
};

#define MPASS_TEXTURE_SA_CCF_DS(hndl, clear_col_float, clear_depth, clear_stencil)                                                         \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t = hndl,                                                                                                                         \
        .clear_val{                                                                                                                        \
            .type = mpass_clear_value::COLOR_TYPE_FLOAT,                                                                                   \
            .fc{clear_col_float},                                                                                                          \
            .depth = clear_depth,                                                                                                          \
            .stencil = clear_stencil,                                                                                                      \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA_CCF(hndl, clear_col_float)                                                                                        \
    MPASS_TEXTURE_SA_CCF_DS(hndl, clear_col_float, DEFAULT_DEPTH_CLEAR, DEFAULT_STENCIL_CLEAR)

#define MPASS_TEXTURE_SA_DS(hndl, clear_depth, clear_stencil)                                                                              \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t = hndl,                                                                                                                         \
        .clear_val{                                                                                                                        \
            .depth = clear_depth,                                                                                                          \
            .stencil = clear_stencil,                                                                                                      \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA(hndl)                                                                                                             \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t = hndl,                                                                                                                         \
    }

#define MPASS_TEXTURE_SA_CCS_DS(hndl, clear_col_signed_int, clear_depth, clear_stencil)                                                    \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t = hndl,                                                                                                                         \
        .clear_val{                                                                                                                        \
            .type = COLOR_TYPE_SINT,                                                                                                       \
            .sc{clear_col_signed_int},                                                                                                     \
            .depth = clear_depth,                                                                                                          \
            .stencil = clear_stencil,                                                                                                      \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA_CCS(hndl, clear_col_signed_int)                                                                                   \
    MPASS_TEXTURE_SA_CCS_DS(hndl, clear_col_signed_int, DEFAULT_DEPTH_CLEAR, DEFAULT_STENCIL_CLEAR)

#define MPASS_TEXTURE_SA_CCU_DS(hndl, clear_col_unsigned_int, clear_depth, clear_stencil)                                                  \
    {                                                                                                                                      \
        .type = mslot_target_type::TEXTURE,                                                                                                \
        .t = hndl,                                                                                                                         \
        .clear_val{                                                                                                                        \
            .type = COLOR_TYPE_UINT,                                                                                                       \
            .uc{clear_col_unsigned_int},                                                                                                   \
            .depth = clear_depth,                                                                                                          \
            .stencil = clear_stencil,                                                                                                      \
        },                                                                                                                                 \
    }

#define MPASS_TEXTURE_SA_CCU(hndl, clear_col_unsigned_int)                                                                                 \
    MPASS_TEXTURE_SA_CCU_DS(hndl, clear_col_unsigned_int, DEFAULT_DEPTH_CLEAR, DEFAULT_STENCIL_CLEAR)

#define MPASS_BUFFER_SA(hndl)                                                                                                              \
    {                                                                                                                                      \
        .type = mslot_target_type::BUFFER,                                                                                                 \
        .t = hndl,                                                                                                                         \
    }

struct mpass
{
    // These need to match exactly in size with the rbp slot count
    static_array<mpass_slot_assignment, MAX_BP_PASS_SLOT_COUNT> slot_assignments;
    rbp_pass_id rbp_pid;

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
    mat4 proj;
    mat4 cam;

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

struct render_job_cb_params
{
    u64 cmd_buf;
    const array<mdraw_call> *draw_calls;
};

using render_job_cb = void(const render_job_cb_params &, void *user);

struct mrender_job
{
    mpass_id pid;
    mview_id vid;
    array<mdraw_call> draw_calls;
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
    render_blueprint_handle rbp;
    array<rbuffer_target> buffers;
    array<rtexture_target> textures;

    mframe_params fp;
    array<mpass> passes;
    array<mview> views;
    array<mrender_job> jobs;
};

mpass_id push_pass(rmanifest *m, rbp_pass_id pid);
mpass_id push_pass(rmanifest *m, rbp_pass_id pid, mpass_slot_assignment *assignments, sizet assignment_count);
u32 push_slot_assignment(rmanifest *m, mpass_id pid, const mpass_slot_assignment &sa);
mview_id push_view(rmanifest *m, const mview &view);
mrender_job_id push_render_job(rmanifest *m, mpass_id pass, mview_id view, render_job_cb *cb, void *cb_params);
u32 push_draw(rmanifest *m, const mdraw_params &dp);

} // namespace nslib
