#include "sim_snapshot.h"

namespace nslib
{

void enqueue_draws(rmanifest *m)
{
    for (auto s = slot_pool_begin(&m->rndr->drawables); is_valid(s); s = slot_pool_next(&m->rndr->drawables, s)) {
        // Add a draw for each subgeom entry in the drawable
        for (u32 subg_i = 0; subg_i < s.item->items.size; ++subg_i) {
            auto cur_subg = &s.item->items[subg_i];
            auto mat = get_slot_item(&m->rndr->materials, cur_subg->mat);
            if (!mat) continue;
            // Loop over the techniques
            for (u32 tech_i = 0; tech_i < mat->techs.size; ++tech_i) {
                mdraw_params mdp{.transform_idx = s.item->tform.sid.si,
                                 .geom = cur_subg->geom,
                                 .subgeom = cur_subg->subgeom,
                                 .mat = cur_subg->mat,
                                 .tech = mat->techs[tech_i],
                                 .layers = s.item->layers};
                push_draw(m, mdp);
            }
        }
    }
}

void build_manifest(rmanifest *m, const sim_snapshot &ss)
{
    for (u32 view_i = 0; view_i < ss.viewport_ss_count; ++view_i) {
        auto cur_v = &ss.viewports[view_i];
        idx_t mview_i = push_view(m, {.vdata = cur_v->vdata, .view_sdata = &cur_v->sdata});
        for (u32 pass_i = 0; pass_i < cur_v->passes.size; ++pass_i) {
            auto cur_pass = &cur_v->passes[pass_i];
            mpass_params mp{
                .rbp = cur_v->rbp,
                .rbpp = cur_pass->rbpp,
                .norm_render_area = cur_pass->rndr_area,
                .assignments = cur_pass->targets.data,
                .assignment_count = cur_pass->targets.size,
                .pass_sdata = nullptr,
            };
            auto mpass_i = push_pass(m, mp);

            pass_ssbo_data pdata{};
            pdata.resolution = m->passes[mpass_i].dims;
            pdata.inv_resolution = 1.0f / pdata.resolution;
            update_pass_data(m, mpass_i, &pdata);

            mrender_job_params jp{.pass = mpass_i,
                                  .view = mview_i,
                                  .max_draw_calls = get_slot_used_count(m->rndr->drawables),
                                  .cb = cur_pass->draw_cb ? cur_pass->draw_cb : draw_geometry,
                                  .cb_user = cur_pass->cb_user};
            push_render_job(m, jp);
        }
    }
    enqueue_draws(m);
}

} // namespace nslib
