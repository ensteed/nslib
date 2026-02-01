#include "renderer.h"
#include "render_manifest.h"

namespace nslib
{

mpass_id push_pass(rmanifest *m, rbp_pass_id pid)
{
    auto bp = get_render_blueprint(m->rndr, m->rbp);
    mpass_id ind = (mpass_id)m->passes.size;
    arr_resize(&m->passes, ind + 1);
    arr_resize(&m->passes[ind].slot_assignments, bp->passes[pid].slots.size);
    m->passes[ind].rbp_pid = pid;
    return ind;
}

mpass_id push_pass(rmanifest *m, rbp_pass_id pid, rpass_slot_assignment *assignments, sizet assignment_count)
{
    auto pind = push_pass(m, pid);
    asrt(assignment_count == m->passes[pind].slot_assignments.size);
    for (sizet i = 0; i < m->passes[pind].slot_assignments.size; ++i) {
        m->passes[pind].slot_assignments[i] = assignments[i];
    }
    return pind;
}

mview_id push_view(rmanifest *m, const mat4 &proj, const mat4 &cam)
{
    mview_id ind = (mview_id)m->views.size;
    arr_emplace_back(&m->views, proj, cam);
    return ind;
}

mrender_job_id push_render_job(rmanifest *m, const mrender_job &rj)
{
    mrender_job_id ind = (mrender_job_id)m->jobs.size;
    arr_push_back(&m->jobs, rj);
    return ind;
}

u32 push_draw(rmanifest *m, const mdraw_params &dp)
{
    u32 push_cnt{0};
    rtechnique_info *tptr = get_slot_item(&m->rndr->techniques, dp.technique);
    for (u32 i = 0; i < tptr->rpass_plines.size; ++i) {
        for (u32 rji = 0; rji < m->jobs.size; ++rji) {
            mrender_job *cur_rj = &m->jobs[rji];
            if (m->passes[cur_rj->pid].rbp_pid == tptr->rpass_plines[i].bp_pass) {
                u32 dc_ind = (u32)cur_rj->draw_calls.size;
                arr_resize(&cur_rj->draw_calls, dc_ind + 1);
                mdraw_call *cur_d = &cur_rj->draw_calls[dc_ind];
                cur_d->geom = dp.geom;
                cur_d->iid = dp.iid;
                cur_d->mat = dp.mat;
                cur_d->pipeline = (sizet)tptr->rpass_plines[i].pline;
                for (sizet texi = 0; texi < dp.tex_assignment_count; ++texi) {
                    cur_d->textures[dp.tex_assignments[texi].unit] = dp.tex_assignments[texi].tex;
                }
                ++push_cnt;
            }
        }
    }
    return push_cnt;
}

} // namespace nslib
