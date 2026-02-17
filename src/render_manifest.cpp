#include "renderer.h"
#include "render_manifest.h"

namespace nslib
{

mpass_idx push_pass_helper(rmanifest *m, rbp_pass_idx pid, mpass_slot_assignment *assignments, sizet assignment_count)
{
    mpass_idx pind = (mpass_idx)m->passes.size;
    arr_resize(&m->passes, pind + 1);
    m->passes[pind].rbp_pid = pid;
    if (assignment_count && assignment_count != 0) {
        auto bp = get_render_blueprint(m->rndr, m->rbp);
        asrt(assignment_count == bp->passes[pid].slots.size);
        arr_resize(&m->passes[pind].slot_assignments, assignment_count);
        for (sizet i = 0; i < m->passes[pind].slot_assignments.size; ++i) {
            m->passes[pind].slot_assignments[i] = assignments[i];
        }
    }
    return pind;
}

mpass_idx push_pass(rmanifest *m, rbp_pass_idx pid, const rect &norm_render_area, mpass_slot_assignment *assignments, sizet assignment_count)
{
    auto pind = push_pass_helper(m, pid, assignments, assignment_count);
    m->passes[pind].ra_size_mode = rect_size_mode::NORMALIZED;
    m->passes[pind].norm_render_area = norm_render_area;
    return pind;
}

mpass_idx push_pass(rmanifest *m, rbp_pass_idx pid, const srect &render_area, mpass_slot_assignment *assignments, sizet assignment_count) {
    auto pind = push_pass_helper(m, pid, assignments, assignment_count);
    m->passes[pind].ra_size_mode = rect_size_mode::ABSOLUTE;
    m->passes[pind].render_area = render_area;
    return pind;
    
}

u32 push_slot_assignment(rmanifest *m, mpass_idx pid, const mpass_slot_assignment &sa)
{
    // We cannot add more assignments than slots!
    auto bp = get_render_blueprint(m->rndr, m->rbp);
    asrt(m->passes[pid].slot_assignments.size < bp->passes[m->passes[pid].rbp_pid].slots.size);

    u32 sa_ind = m->passes[pid].slot_assignments.size++;
    m->passes[pid].slot_assignments[sa_ind] = sa;
    return sa_ind;
}

mview_idx push_view(rmanifest *m, const mview &view)
{
    mview_idx ind = (mview_idx)m->views.size;
    arr_push_back(&m->views, view);
    return ind;
}

mrender_job_idx push_render_job(rmanifest *m, mpass_idx pass, mview_idx view, render_job_cb *cb, void *cb_params)
{
    mrender_job_idx ind = (mrender_job_idx)m->jobs.size;
    auto rj = arr_emplace_back(&m->jobs, pass, view, array<mdraw_call>{}, cb, cb_params);
    arr_init(&rj->draw_calls, m->jobs.arena, 64);
    return ind;
}

u32 push_draw(rmanifest *m, const mdraw_params &dp)
{
    u32 push_cnt{0};
    rtechnique_info *tptr = get_slot_item(&m->rndr->techniques, dp.technique.hndl);
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
