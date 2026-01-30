#pragma once
#include "containers/slot_pool.h"
#include "render_defs.h"
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
    pipeline_id pl;
    static_array<rtexture_handle, RMATERIAL_TEXTURE_COUNT> textures;
    instance_id iid;
};

struct rpass_slot_assignment
{
    rres_id res_id{INVALID_IND};
    u32 res_ind{INVALID_ID};
};

struct mpass
{
    u32 slot_assignments[MAX_BP_PASS_SLOT_COUNT];
    rbp_pass_id pid;
};

struct mview
{
    mat4 proj;
    mat4 cam;
};

struct render_job_cb_params
{
    mpass_id pid;
    mview_id vid;
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

struct mframe_params {
    double dt;
};

struct rmanifest
{
    renderer *rndr;
    render_blueprint_handle rbp;
    mframe_params fp;
    array<mpass> passes;
    array<mview> views;
    array<mrender_job> jobs;
};

mpass_id push_pass(rmanifest *m, rbp_pass_id pid, rpass_slot_assignment *assignments, sizet assignment_count);
mview_id push_view(rmanifest *m, const mat4 &proj, const mat4 &cam);
mrender_job_id push_render_job(rmanifest *m, const mrender_job &rj);
u32 push_draw(rmanifest *m, const mdraw_params &dp);

} // namespace nslib
