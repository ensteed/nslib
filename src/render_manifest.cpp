#include "platform.h"
#include "renderer.h"
#include "vkr_utils.h"
#include "render_manifest.h"

#ifdef USE_IMGUI
    #include "imgui/imgui.h"
    #include "imgui/imgui_impl_sdl3.h"
    #include "imgui/imgui_impl_vulkan.h"
#endif

namespace nslib
{

intern constexpr f32 WINDOW_RESIZE_DEBOUNCE_DURATION = 0.05;

struct track_rdraw_dyn_state
{
    idx_t last_pline{INVALID_IDX};
    idx_t last_material{INVALID_IDX};
    rdraw_dyn_state dstate;
};

struct render_job_cb_params
{
    // Manifest pass
    const mpass *mp;
    // Manifest view
    const mview *mv;
    // Current framebuffer
    const vkr_framebuffer *fb;
    // Renderer source data
    const rgeometry_pool *geometry;
    const rmaterial_pool *materials;
    const rpipeline_cache *plines;
    // Geometry stream group for this pass
    const geom_stream_group *geom_gp;
    const global_descriptor_info *desc_info{};
    // Frame in flight index
    idx_t fif;
    // Dynamic state tracked at the job level - each draw function should update this as it goes
    track_rdraw_dyn_state *dyn_state;
    // Draw functions for extended dynamic state (needed for rpi has to use extension)
    const vkr_eds1_fptrs *fns;

    // Job draw calls
    const array<mdraw_call> *dcs;
    const array<idx_t> *instanced_dcs;
    u64 cmd_buf;
};

enum pline_dstate_update_flag
{
    PLINE_DSTATE_UPDATE_NONE = 0,
    PLINE_DSTATE_UPDATE_CULL_MODE = make_flag(0),
    PLINE_DSTATE_UPDATE_FRONT_FACE = make_flag(1),
    PLINE_DSTATE_UPDATE_STENCIL_TEST_ENABLE = make_flag(2),
    PLINE_DSTATE_UPDATE_STENCIL_OP_FRONT = make_flag(3),
    PLINE_DSTATE_UPDATE_STENCIL_OP_BACK = make_flag(4),
    PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_FRONT = make_flag(5),
    PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_BACK = make_flag(6),
    PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_FRONT = make_flag(7),
    PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_BACK = make_flag(8),
    PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_FRONT = make_flag(9),
    PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_BACK = make_flag(10),
    PLINE_DSTATE_UPDATE_DEPTH_BIAS = make_flag(11),
    PLINE_DSTATE_UPDATE_BLEND_CONSTANTS = make_flag(12),
    PLINE_DSTATE_UPDATE_ALL = PLINE_DSTATE_UPDATE_CULL_MODE | PLINE_DSTATE_UPDATE_FRONT_FACE | PLINE_DSTATE_UPDATE_STENCIL_TEST_ENABLE |
                              PLINE_DSTATE_UPDATE_STENCIL_OP_FRONT | PLINE_DSTATE_UPDATE_STENCIL_OP_BACK |
                              PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_FRONT | PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_BACK |
                              PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_FRONT | PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_BACK |
                              PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_FRONT | PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_BACK |
                              PLINE_DSTATE_UPDATE_DEPTH_BIAS | PLINE_DSTATE_UPDATE_BLEND_CONSTANTS,
};

enum mdraw_sort_key_shift : u32
{
    MDRAW_SORT_KEY_SHIFT_SUBGEOM = 0,
    MDRAW_SORT_KEY_SHIFT_GEOM = 8,
    MDRAW_SORT_KEY_SHIFT_MAT = 32,
    MDRAW_SORT_KEY_SHIFT_PL = 48,
};

enum mdraw_sort_key_mask : u64
{
    MDRAW_SORT_KEY_MASK_SUBGEOM = 0xffull,
    MDRAW_SORT_KEY_MASK_GEOM = 0xffffffull,
    MDRAW_SORT_KEY_MASK_MAT = 0xffffull,
    MDRAW_SORT_KEY_MASK_PL = 0xffffull,
};

intern u64 pack_mdraw_sort_key(const mdraw_call &dc)
{
    asrt(((u64)dc.subgeom & ~MDRAW_SORT_KEY_MASK_SUBGEOM) == 0);
    asrt(((u64)dc.geom & ~MDRAW_SORT_KEY_MASK_GEOM) == 0);
    asrt(((u64)dc.mat & ~MDRAW_SORT_KEY_MASK_MAT) == 0);
    asrt(((u64)dc.pl & ~MDRAW_SORT_KEY_MASK_PL) == 0);

    return ((u64)dc.pl << MDRAW_SORT_KEY_SHIFT_PL) | ((u64)dc.mat << MDRAW_SORT_KEY_SHIFT_MAT) |
           ((u64)dc.geom << MDRAW_SORT_KEY_SHIFT_GEOM) | ((u64)dc.subgeom << MDRAW_SORT_KEY_SHIFT_SUBGEOM);
}

intern void setup_pline_dynamic_state(VkCommandBuffer cb, const vkr_eds1_fptrs &fns, const mdraw_call &dc, track_rdraw_dyn_state *state)
{
    // Early out if material and pipeline didn't change
    if (dc.pl == state->last_pline && dc.mat == state->last_material) return;
    bool state_valid = is_valid(state->last_pline);
    u32 state_update_mask = state_valid ? PLINE_DSTATE_UPDATE_NONE : PLINE_DSTATE_UPDATE_ALL;

    VkCullModeFlags cur_cm = get_vk_cullmode(dc.dstate.dflags);
    VkFrontFace cur_ff = get_vk_front_face(dc.dstate.ffw);
    VkBool32 cur_stest_enable = test_flags(dc.dstate.dflags, RTECHNIQUE_DYN_STATE_FLAG_STENCIL_TEST);

    VkStencilOpState cur_st_opstate_front{};
    fill_vk_stencil_op_state(&cur_st_opstate_front, dc.dstate.stencil_front);

    VkStencilOpState cur_st_opstate_back{};
    fill_vk_stencil_op_state(&cur_st_opstate_back, dc.dstate.stencil_back);

    if (state_valid) {
        VkCullModeFlags prev_cm = get_vk_cullmode(state->dstate.dflags);
        VkFrontFace prev_ff = get_vk_front_face(state->dstate.ffw);
        VkBool32 prev_stest_enable = test_flags(state->dstate.dflags, RTECHNIQUE_DYN_STATE_FLAG_STENCIL_TEST);

        VkStencilOpState prev_st_opstate_front{};
        fill_vk_stencil_op_state(&prev_st_opstate_front, state->dstate.stencil_front);

        VkStencilOpState prev_st_opstate_back{};
        fill_vk_stencil_op_state(&prev_st_opstate_back, state->dstate.stencil_back);

        state_update_mask |= (prev_cm != cur_cm) ? PLINE_DSTATE_UPDATE_CULL_MODE : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_ff != cur_ff) ? PLINE_DSTATE_UPDATE_FRONT_FACE : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_stest_enable != cur_stest_enable) ? PLINE_DSTATE_UPDATE_STENCIL_TEST_ENABLE : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |=
            (prev_st_opstate_front.failOp != cur_st_opstate_front.failOp || prev_st_opstate_front.passOp != cur_st_opstate_front.passOp ||
             prev_st_opstate_front.depthFailOp != cur_st_opstate_front.depthFailOp ||
             prev_st_opstate_front.compareOp != cur_st_opstate_front.compareOp)
                ? PLINE_DSTATE_UPDATE_STENCIL_OP_FRONT
                : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |=
            (prev_st_opstate_back.failOp != cur_st_opstate_back.failOp || prev_st_opstate_back.passOp != cur_st_opstate_back.passOp ||
             prev_st_opstate_back.depthFailOp != cur_st_opstate_back.depthFailOp ||
             prev_st_opstate_back.compareOp != cur_st_opstate_back.compareOp)
                ? PLINE_DSTATE_UPDATE_STENCIL_OP_BACK
                : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_st_opstate_front.compareMask != cur_st_opstate_front.compareMask)
                                 ? PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_FRONT
                                 : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_st_opstate_back.compareMask != cur_st_opstate_back.compareMask)
                                 ? PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_BACK
                                 : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_st_opstate_front.writeMask != cur_st_opstate_front.writeMask)
                                 ? PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_FRONT
                                 : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_st_opstate_back.writeMask != cur_st_opstate_back.writeMask) ? PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_BACK
                                                                                               : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_st_opstate_front.reference != cur_st_opstate_front.reference) ? PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_FRONT
                                                                                                 : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (prev_st_opstate_back.reference != cur_st_opstate_back.reference) ? PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_BACK
                                                                                               : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |= (state->dstate.depth_b != dc.dstate.depth_b) ? PLINE_DSTATE_UPDATE_DEPTH_BIAS : PLINE_DSTATE_UPDATE_NONE;

        state_update_mask |=
            (state->dstate.blend_consts != dc.dstate.blend_consts) ? PLINE_DSTATE_UPDATE_BLEND_CONSTANTS : PLINE_DSTATE_UPDATE_NONE;
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_CULL_MODE)) {
        fns.vkCmdSetCullMode(cb, cur_cm);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_FRONT_FACE)) {
        fns.vkCmdSetFrontFace(cb, cur_ff);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_TEST_ENABLE)) {
        fns.vkCmdSetStencilTestEnable(cb, cur_stest_enable);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_OP_FRONT)) {
        fns.vkCmdSetStencilOp(cb,
                              VK_STENCIL_FACE_FRONT_BIT,
                              cur_st_opstate_front.failOp,
                              cur_st_opstate_front.passOp,
                              cur_st_opstate_front.depthFailOp,
                              cur_st_opstate_front.compareOp);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_OP_BACK)) {
        fns.vkCmdSetStencilOp(cb,
                              VK_STENCIL_FACE_BACK_BIT,
                              cur_st_opstate_back.failOp,
                              cur_st_opstate_back.passOp,
                              cur_st_opstate_back.depthFailOp,
                              cur_st_opstate_back.compareOp);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_FRONT)) {
        vkCmdSetStencilCompareMask(cb, VK_STENCIL_FACE_FRONT_BIT, dc.dstate.stencil_front.comp_mask);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_COMPARE_MASK_BACK)) {
        vkCmdSetStencilCompareMask(cb, VK_STENCIL_FACE_BACK_BIT, dc.dstate.stencil_back.comp_mask);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_FRONT)) {
        vkCmdSetStencilWriteMask(cb, VK_STENCIL_FACE_FRONT_BIT, dc.dstate.stencil_front.write_mask);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_WRITE_MASK_BACK)) {
        vkCmdSetStencilWriteMask(cb, VK_STENCIL_FACE_BACK_BIT, dc.dstate.stencil_back.write_mask);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_FRONT)) {
        vkCmdSetStencilReference(cb, VK_STENCIL_FACE_FRONT_BIT, dc.dstate.stencil_front.ref);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_STENCIL_REFERENCE_BACK)) {
        vkCmdSetStencilReference(cb, VK_STENCIL_FACE_BACK_BIT, dc.dstate.stencil_back.ref);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_DEPTH_BIAS)) {
        vkCmdSetDepthBias(cb, dc.dstate.depth_b.const_factor, dc.dstate.depth_b.slope_factor, dc.dstate.depth_b.clamp);
    }

    if (test_flags(state_update_mask, PLINE_DSTATE_UPDATE_BLEND_CONSTANTS)) {
        vkCmdSetBlendConstants(cb, dc.dstate.blend_consts.data);
    }
    state->dstate = dc.dstate;
    state->last_pline = dc.pl;
    state->last_material = dc.mat;
}

void draw_geometry(const render_job_cb_params &p, void *)
{
    asrt(p.mp);
    asrt(p.mv);
    asrt(p.fb);
    asrt(p.geometry);
    asrt(p.materials);
    asrt(p.plines);
    asrt(p.geom_gp);
    asrt(p.desc_info);
    asrt(p.dyn_state);
    asrt(p.dcs);
    asrt(p.instanced_dcs);
    asrt(p.fns);

    VkCommandBuffer cb = (VkCommandBuffer)p.cmd_buf;
    // VIEWPORT
    VkViewport viewport = (p.mv->vp_size_mode == rect_size_mode::NORMALIZED)
                              ? get_vk_viewport(p.mv->vp, p.mv->vp_depth_min_max, p.fb->meta.dims)
                              : get_vk_viewport(p.mv->vp, p.mv->vp_depth_min_max);
    vkCmdSetViewport(cb, 0, 1, &viewport);

    // SCISSOR
    VkRect2D scissor = (p.mv->scissor_size_mode == rect_size_mode::NORMALIZED)
                           ? get_vk_rect_from_normalized(p.mv->norm_scissor, p.fb->meta.dims)
                           : get_vk_rect(p.mv->scissor);
    vkCmdSetScissor(cb, 0, 1, &scissor);

    // Bind pass vert/indice buffers
    VkBuffer vbufs[MAX_VERT_BINDINGS * MAX_GEOMETRY_LAYOUT_COUNT]{};
    VkDeviceSize offsets[MAX_VERT_BINDINGS * MAX_GEOMETRY_LAYOUT_COUNT]{};
    u32 bi_count{0};
    for (u32 i = 0; i < p.geom_gp->layouts.size; ++i) {
        for (u32 j = 0; j < p.geom_gp->layouts[i].vert_streams.size; ++j, ++bi_count) {
            vbufs[bi_count] = p.geom_gp->layouts[i].vert_streams[j].buffer.hndl;
            offsets[bi_count] = 0;
        }
    }
    vkCmdBindVertexBuffers(cb, 0, bi_count, vbufs, offsets);
    vkCmdBindIndexBuffer(cb, p.geom_gp->indice_stream.buffer.hndl, 0, get_vk_index_type(sizeof(ind_t)));

    // Bind global descriptor sets
    VkDescriptorSet desc_sets[RDSET_LAYOUT_COUNT]{};
    desc_sets[RDSET_LAYOUT_MAIN_DATA] = p.desc_info->main_data[p.fif];
    desc_sets[RDSET_LAYOUT_IMAGES] = p.desc_info->images;
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, p.desc_info->pline_layout, 0, RDSET_LAYOUT_COUNT, desc_sets, 0, nullptr);

    u32 inst_draw_id = 0;
    for (u32 dci = 0; dci < p.instanced_dcs->size; ++dci) {
        const mdraw_call *dc = &(*p.dcs)[(*p.instanced_dcs)[dci]];
        const rgeom_info *geom = &p.geometry->slots[dc->geom].item;
        const rmaterial_info *mat = &p.materials->slots[dc->mat].item;
        const rpipeline_entry *pl = &p.plines->items.slots[dc->pl].item;

        asrt(geom && mat && pl);
        if (p.dyn_state->last_pline != dc->pl) {
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)pl->gpu_d);
        }
        setup_pline_dynamic_state(cb, *p.fns, *dc, p.dyn_state);

        const rsubgeom_range *cur_r = &geom->subgeom_vert_ind_counts[dc->subgeom];
        u32 voffset = geom->vert_offset + cur_r->offset;
        u32 ioffset = geom->ind_offset + cur_r->offset;
        vkCmdDrawIndexed(cb, cur_r->count, dc->inst_count, ioffset, voffset, inst_draw_id);
        
        inst_draw_id += dc->inst_count;
    }
}

#if defined USE_IMGUI
void draw_imgui(const render_job_cb_params &p, void *user)
{
    auto img_data = (ImDrawData*)user;
    ImGui_ImplVulkan_RenderDrawData(img_data, (VkCommandBuffer)p.cmd_buf);
}
#endif

intern bool window_resize_continue_check(renderer *rndr, frame_context *cur_fif)
{
    if (window_resized_this_frame(rndr->vk.cfg.window)) {
        cur_fif->swapchain_resize = WINDOW_RESIZE_DEBOUNCE_DURATION;
    }

    if (cur_fif->swapchain_resize > 0.0f) {
        cur_fif->swapchain_resize -= rndr->pt.dt;
        if (cur_fif->swapchain_resize > 0.0f) {
            return false;
        }
        handle_window_resize(rndr);
        cur_fif->swapchain_resize = false;
    }
    return true;
}

// We can let this "leak" as it doesn't leak due to using frame linear allocator
intern rmanifest *create_manifest(renderer *rndr, idx_t fif)
{
    rmanifest *m = mem_calloc<rmanifest>(1, &rndr->manifest_flinear);
    m->fif = fif;
    arr_init(&m->jobs, &rndr->manifest_flinear, 24);
    arr_init(&m->passes, &rndr->manifest_flinear, 12);
    arr_init(&m->views, &rndr->manifest_flinear, 12);

    // Initialize our manifest textures and buffers with the global ones (well, globabl to the renderer)
    arr_init(&m->textures, &rndr->manifest_flinear);
    arr_resize(&m->textures, rndr->rtargets.textures.slots.size);
    for (u32 i = 0; i < m->textures.size; ++i) {
        m->textures[i] = rndr->rtargets.textures.slots[i].item.frames[fif];
    }

    // Buffers
    arr_init(&m->buffers, &rndr->manifest_flinear);
    arr_resize(&m->buffers, rndr->rtargets.buffers.slots.size);
    for (u32 i = 0; i < m->buffers.size; ++i) {
        m->buffers[i] = rndr->rtargets.buffers.slots[i].item.frames[fif];
    }

    return m;
}

intern u64 hash_framebuffer_config(const vkr_framebuffer_cfg &cfg)
{
    u64 h = 0;
    hash_combine(&h, hash_type(&cfg.meta, sizeof(cfg.meta)));
    hash_combine(&h, hash_type(cfg.atts, cfg.att_count * sizeof(VkImageView)));
    return h;
}

intern const vkr_framebuffer *get_or_create_framebuffer(renderer *rndr,
                                                        VkClearValue *cv,
                                                        sizet *cv_size,
                                                        VkRenderPass vk_rpass,
                                                        const mpass &mp,
                                                        const rbp_pass &rbp_pass,
                                                        const rmanifest &m,
                                                        u32 fif)
{
    //////////////////
    // Build config //
    //////////////////
    VkImageView atts[MAX_FRAMEBUFFER_ATTACHMENT_COUNT]{};
    vkr_framebuffer_cfg cfg{};
    cfg.meta.layers = 1;
    cfg.meta.rpass = vk_rpass;

    // The slot attachments might not be in the same order - ie slot 0 -> att 3, slot 1 -> att 1, slot 2 -> no att, slot
    // 3 ->att 0, slot 4 -> att 2, so we set the cfg att count to the highest att ind we find + 1. But we keep track of
    // the att_cnt also so we can assert that att_cnt == cfg.att_count at the end - so this is purely for debug info basically
    u32 att_cnt{0};

    for (u32 si = 0; si < mp.slot_assignments.size; ++si) {
        auto cur_sl = &mp.slot_assignments[si];
        bool t_cond = cur_sl->type == mslot_target_type::TEXTURE && is_valid(cur_sl->t.hndl);
        bool b_cond = cur_sl->type == mslot_target_type::BUFFER && is_valid(cur_sl->b);
        asrt(t_cond || b_cond);

        // If is attachment, we add to framebuffer
        u32 att_ind = rbp_pass.slots[si].att_ind;
        if (is_valid(att_ind)) {
            asrt(att_ind < MAX_FRAMEBUFFER_ATTACHMENT_COUNT);
            const rtexture_target_fif *cur_t = &m.textures[cur_sl->t.hndl.si];

            // Can't use the value from cfg - swapchain images don't have correct data in the cfg field as they were
            // never actually created..
            uvec2 tex_dims = cur_t->image.dims.xy;

            // The frame buffer can only be as big as the smallest texture - so that's what we set it to
            if (cfg.meta.dims.x == 0 || tex_dims.x < cfg.meta.dims.x) {
                cfg.meta.dims.x = tex_dims.x;
            }
            if (cfg.meta.dims.y == 0 || tex_dims.y < cfg.meta.dims.y) {
                cfg.meta.dims.y = tex_dims.y;
            }

            if (att_ind >= cfg.att_count) {
                cfg.att_count = att_ind + 1;
            }
            atts[att_ind] = cur_t->view;

            // Set clear value
            rformat tex_format = get_rformat(cur_t->image.format);
            cv[*cv_size] = get_vk_clear_value(cur_sl->t.clear_val, tex_format);
            ++(*cv_size);

            ++att_cnt;
        }
    }
    asrt(cfg.att_count == att_cnt);
    cfg.atts = atts;

    ///////////////////
    // Create or get //
    ///////////////////
    key_t key = hash_framebuffer_config(cfg);
    auto fiter = hmap_find(&rndr->fb_cache.key_lut, key);
    if (fiter) {
        auto slitem = get_slot_item(&rndr->fb_cache.items, fiter->val);
        asrt(slitem);
        return &slitem->gpu_d;
    }
    ilog("Creating new framebuffer for unique hash %lu", key);
    auto new_slot = acquire_slot(&rndr->fb_cache.items);
    asrt(is_valid(new_slot) && "Out of framebuffer slots");
    int result = vkr_init_framebuffer(&new_slot.item->gpu_d, cfg, &rndr->vk);
    asrt(result == err_code::VKR_NO_ERROR);
    hmap_insert(&rndr->fb_cache.key_lut, key, new_slot.hndl);
    return &new_slot.item->gpu_d;
}

struct rbp_slot_usage_info
{
    const rbp_resource_requirement *first{};
    const rbp_resource_requirement *last{};
};

intern void gather_pass_slot_usage_info(rbp_slot_usage_info *infos, const rbp_pass &rbpp)
{
    // Record first/last requirement per slot so we can place a single pre-pass barrier and finalize state post-pass.
    for (u32 subi = 0; subi < rbpp.subpasses.size; ++subi) {
        const rbp_subpass *sub = &rbpp.subpasses[subi];
        for (u32 resi = 0; resi < sub->resources.size; ++resi) {
            const rbp_resource_requirement *req = &sub->resources[resi];
            asrt(req->slot_ind < rbpp.slots.size);
            rbp_slot_usage_info *info = &infos[req->slot_ind];
            if (!info->first) info->first = req;
            info->last = req;
        }
    }
}

intern rtexture_state get_required_texture_state(const rbp_pass &rbpp, const rbp_resource_requirement &req, const rtexture_state &cur_st)
{
    rtexture_state req_st{};
    bool is_att = is_usage_attachment(rbpp.slots[req.slot_ind].usage);
    req_st.layout = is_att ? get_baked_initial_vk_layout(rbpp, req) : get_vk_layout_from_requirement(rbpp, req, false);
    bool undef = (req_st.layout == VK_IMAGE_LAYOUT_UNDEFINED);
    if (undef) req_st.layout = cur_st.layout;
    req_st.access = (is_att && undef) ? VK_ACCESS_NONE : get_vk_access_from_requirement(rbpp, req);
    req_st.stage = (is_att && undef) ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : get_vk_stage_from_requirement(rbpp, req);
    return req_st;
}

intern rtexture_state get_updated_texture_state(const rbp_pass &rbpp, const rbp_resource_requirement &req)
{
    rtexture_state updated{};
    bool is_attachment = is_usage_attachment(rbpp.slots[req.slot_ind].usage);
    updated.layout = get_vk_layout_from_requirement(rbpp, req, is_attachment);
    updated.access = get_vk_access_from_requirement(rbpp, req);
    updated.stage = get_vk_stage_from_requirement(rbpp, req);
    return updated;
}

intern rbuffer_state get_updated_buffer_state(const rbp_pass &rbpp, const rbp_resource_requirement &req)
{
    rbuffer_state updated{};
    updated.access = get_vk_access_from_requirement(rbpp, req);
    updated.stage = get_vk_stage_from_requirement(rbpp, req);
    return updated;
}

intern void emit_manifest_pass_barriers(rmanifest *m, const rbp_pass &rbpp, idx_t rjid, VkCommandBuffer buf, u32 fif)
{
    const mrender_job &rj = m->jobs[rjid];
    const mpass &mp = m->passes[rj.mp];

    rbp_slot_usage_info slot_usage[MAX_BP_PASS_SLOT_COUNT]{};
    gather_pass_slot_usage_info(slot_usage, rbpp);

    static_array<VkImageMemoryBarrier, MAX_BP_PASS_SLOT_COUNT> image_barriers{};
    static_array<VkBufferMemoryBarrier, MAX_BP_PASS_SLOT_COUNT> buffer_barriers{};
    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;

    for (u32 slot_ind = 0; slot_ind < rbpp.slots.size; ++slot_ind) {
        const rbp_resource_requirement *first = slot_usage[slot_ind].first;
        if (!first) {
            continue;
        }

        const mpass_slot_assignment &assignment = mp.slot_assignments[slot_ind];
        if (assignment.type == mslot_target_type::TEXTURE) {
            asrt(is_valid(assignment.t.hndl));
            asrt(assignment.t.hndl.si < m->textures.size);
            auto cur_t = &m->textures[assignment.t.hndl.si];
            rtexture_state *cur_st = &cur_t->state;
            rtexture_state req_st = get_required_texture_state(rbpp, *first, *cur_st);

            bool use_bookends = rbpp.use_subpass_bookends && is_usage_attachment(rbpp.slots[slot_ind].usage);
            bool layout_mismatch = cur_st->layout != req_st.layout && req_st.layout != VK_IMAGE_LAYOUT_UNDEFINED;
            bool access_stage_mismatch = cur_st->access != req_st.access || cur_st->stage != req_st.stage;
            // Bookend dependencies cover external memory visibility for attachments. We still need a barrier
            // for layout transitions when the current layout doesn't match the render pass initial layout.
            if (layout_mismatch || (!use_bookends && access_stage_mismatch)) {
#ifdef LOG_IMAGE_MEM_BARRIER
                ilog("%s: cur{l:%d a:%d s:%d}  req{l:%d a:%d s:%d}",
                     cur_t->name,
                     cur_st->layout,
                     cur_st->access,
                     cur_st->stage,
                     req_st.layout,
                     req_st.access,
                     req_st.stage);
#endif

                // Single barrier per resource into the first required state for this pass.
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = cur_st->layout;
                barrier.newLayout = req_st.layout;
                barrier.srcAccessMask = cur_st->access;
                barrier.dstAccessMask = req_st.access;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = cur_t->image.hndl;
                barrier.subresourceRange = cur_t->iv_cfg->srange;
                arr_push_back(&image_barriers, barrier);

                src_stage_mask |= normalize_vk_stage_mask(cur_st->stage);
                dst_stage_mask |= normalize_vk_stage_mask(req_st.stage);
            }
            // Keep the manifest in sync so later passes see the in-pass state even if no barrier was needed.
            *cur_st = req_st;
        }
        else if (assignment.type == mslot_target_type::BUFFER) {
            asrt(is_valid(assignment.b));
            asrt(assignment.b.si < m->buffers.size);
            auto cur_b = &m->buffers[assignment.b.si];
            rbuffer_state *cur_st = &cur_b->state;
            rbuffer_state req_st = get_updated_buffer_state(rbpp, *first);

            if (cur_st->access != req_st.access || cur_st->stage != req_st.stage) {
#ifdef LOG_BUFFER_MEM_BARRIER
                ilog("%s: cur{a:%d s:%d}  req{a:%d s:%d}", cur_b->name, cur_st->access, cur_st->stage, req_st.access, req_st.stage);
#endif

                // Single barrier per resource into the first required state for this pass.
                VkBufferMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.srcAccessMask = cur_st->access;
                barrier.dstAccessMask = req_st.access;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = cur_b->buffer.hndl;
                barrier.offset = 0;
                barrier.size = VK_WHOLE_SIZE;
                arr_push_back(&buffer_barriers, barrier);

                src_stage_mask |= normalize_vk_stage_mask(cur_st->stage);
                dst_stage_mask |= normalize_vk_stage_mask(req_st.stage);
            }
            // Keep the manifest in sync so later passes see the in-pass state even if no barrier was needed.
            *cur_st = req_st;
        }
    }

    if (image_barriers.size > 0 || buffer_barriers.size > 0) {
        // Collapse all barriers into a single pipeline barrier for this pass.
        if (src_stage_mask == 0) {
            src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        if (dst_stage_mask == 0) {
            dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
#if defined(LOG_PIPELINE_BARRIER)
        ilog("Emit pre pass barrier (pi %u from type %s) for rj %u with %u im and %u buf barriers (src:%d dst:%d)",
             rj.pid,
             rbpp.name,
             rjid,
             image_barriers.size,
             buffer_barriers.size,
             src_stage_mask,
             dst_stage_mask);
#endif
        vkCmdPipelineBarrier(buf,
                             src_stage_mask,
                             dst_stage_mask,
                             0,
                             0,
                             nullptr,
                             (u32)buffer_barriers.size,
                             buffer_barriers.data,
                             (u32)image_barriers.size,
                             image_barriers.data);
    }
}

intern void update_manifest_pass_states(rmanifest *m, const rbp_pass &rbp_pass, const mpass &mp, u32 fif)
{
    rbp_slot_usage_info slot_usage[MAX_BP_PASS_SLOT_COUNT]{};
    gather_pass_slot_usage_info(slot_usage, rbp_pass);

    for (u32 slot_ind = 0; slot_ind < rbp_pass.slots.size; ++slot_ind) {
        const rbp_resource_requirement *last = slot_usage[slot_ind].last;
        const mpass_slot_assignment &assignment = mp.slot_assignments[slot_ind];
        if (assignment.type == mslot_target_type::TEXTURE) {
            // Final state after the pass completes (attachments use final layout).
            asrt(is_valid(assignment.t.hndl));
            asrt(assignment.t.hndl.si < m->textures.size);
            m->textures[assignment.t.hndl.si].state = get_updated_texture_state(rbp_pass, *last);
        }
        else if (assignment.type == mslot_target_type::BUFFER) {
            // Final buffer access/stage after the pass completes.
            asrt(is_valid(assignment.b));
            asrt(assignment.b.si < m->buffers.size);
            m->buffers[assignment.b.si].state = get_updated_buffer_state(rbp_pass, *last);
        }
    }
}

intern void update_global_target_state(rmanifest *m, u32 fif)
{
    for (u32 i = 0; i < m->textures.size; ++i) {
        m->rndr->rtargets.textures.slots[i].item.frames[fif].state = m->textures[i].state;
    }
    for (u32 i = 0; i < m->buffers.size; ++i) {
        m->rndr->rtargets.buffers.slots[i].item.frames[fif].state = m->buffers[i].state;
    }
}

intern void sort_draw_list(mrender_job *rjob)
{
    sizet n = rjob->dcs.size;
    if (n <= 1) return;

    // Initialize index arrays
    u32 *src = rjob->sorted_dcs.data;
    u32 *tmp = rjob->instanced_dcs.data;
    for (u32 i = 0; i < n; ++i)
        src[i] = i;

    for (u32 byte = 0; byte < 8; ++byte) {
        u32 shift = byte * 8;

        u32 counts[256]{};
        for (sizet i = 0; i < n; ++i)
            ++counts[(u8)(rjob->dcs.data[src[i]].sort_key >> shift)];

        u32 total = 0;
        for (u32 b = 0; b < 256; ++b) {
            u32 c = counts[b];
            counts[b] = total;
            total += c;
        }

        for (sizet i = 0; i < n; ++i)
            tmp[counts[(u8)(rjob->dcs.data[src[i]].sort_key >> shift)]++] = src[i];

        u32 *swap = src;
        src = tmp;
        tmp = swap;
    }

    // src is the final sorted index array (8 swaps = even, so src == original alloc)
    rjob->sorted_dcs.data = src;
    rjob->sorted_dcs.size = n;

    rjob->instanced_dcs.size = 1;
    rjob->instanced_dcs[0] = rjob->sorted_dcs[0];

    for (u32 i = 1; i < n; ++i) {
        if (rjob->dcs[rjob->sorted_dcs[i]].sort_key != rjob->dcs[rjob->sorted_dcs[i - 1]].sort_key) {
            rjob->instanced_dcs[rjob->instanced_dcs.size] = rjob->sorted_dcs[i];
            ++rjob->instanced_dcs.size;
        }
        else {
            ++rjob->dcs[*arr_back(&rjob->instanced_dcs)].inst_count;
        }
    }

    // for (u32 i = 1; i < n; ++i) {
    //     if (rjob->dcs[rjob->sorted_dcs[i]].sort_key != rjob->dcs[rjob->sorted_dcs[last_match]].sort_key) {
    //         rjob->sorted_dcs[last_match+1] = rjob->sorted_dcs[i];
    //         last_match = i;
    //         ++rjob->sorted_dcs.size;
    //     }
    //     else {
    //         ++rjob->dcs[rjob->sorted_dcs[last_match]].inst_count;
    //     }
    // }
}

intern void update_draw_ssbo(rmanifest *m, mrender_job *cur_rj, sizet job_ssbo_base)
{
    sizet blocksz = m->rndr->desc_info.draw_ssbo.block_size;
    for (u32 i = 0; i < cur_rj->sorted_dcs.size; ++i) {
        const mdraw_call &dc = cur_rj->dcs[cur_rj->sorted_dcs[i]];
        sizet buf_offset = blocksz * (m->fif * m->rndr->desc_info.draw_ssbo.fif_block_count + job_ssbo_base + i);
        void *dst = (void *)((sizet)m->rndr->desc_info.draw_ssbo.buffer.mem_info.pMappedData + buf_offset);
        mdraw_ssbo_data dd{
            .inst = dc.inst,
            .material = dc.mat,
            .view = cur_rj->mv,
            .pass = cur_rj->mp,
        };
        memcpy(dst, &dd, blocksz);
    }
}

intern bool execute_manifest(rmanifest *m, VkCommandBuffer buf, idx_t fif)
{
    int err = vkr_begin_cmd_buf(buf, {});
    if (err != err_code::VKR_NO_ERROR) {
        return false;
    }

    track_rdraw_dyn_state dyn_state{};

    // We place all draw call data for all jobs in a single SSBO, so this is the base offset for the current job's draw call data
    sizet job_ssbo_base = 0;
    for (u32 rji = 0; rji < m->jobs.size; ++rji) {
        auto cur_rj = &m->jobs[rji];
        auto mp = &m->passes[cur_rj->mp];
        auto mv = &m->views[cur_rj->mv];
        auto rbp_pass = &m->rbp.item->passes[mp->rbpp];
        auto vk_rpass = (VkRenderPass)rbp_pass->vk_handle;

        // Must have all slots assigned
        asrt(rbp_pass->slots.size == mp->slot_assignments.size);

        // Sort it baby boo
        sort_draw_list(cur_rj);

        update_draw_ssbo(m, cur_rj, job_ssbo_base);
        job_ssbo_base += cur_rj->dcs.size;

        // Create all needed barriers for the current pass resources (according to what we have for the current state)
        emit_manifest_pass_barriers(m, *rbp_pass, cur_rj->mp, buf, fif);

        // Setup framebuffer and clear vals by looping over slots
        static_array<VkClearValue, MAX_BP_PASS_SLOT_COUNT> att_clear_vals{};
        const vkr_framebuffer *fb =
            get_or_create_framebuffer(m->rndr, att_clear_vals.data, &att_clear_vals.size, vk_rpass, *mp, *rbp_pass, *m, fif);

        // RENDER AREA
        VkRect2D ra = (mp->ra_size_mode == rect_size_mode::NORMALIZED) ? get_vk_rect_from_normalized(mp->norm_render_area, fb->meta.dims)
                                                                       : get_vk_rect(mp->render_area);
        vkr_cmd_begin_rpass(buf, vk_rpass, fb, ra, att_clear_vals.data, att_clear_vals.size);

        if (cur_rj->cb) {
            auto gsg = find_geometry_stream_group(m->rndr, rbp_pass->geom_streams_group);
            render_job_cb_params p{};
            p.cmd_buf = (u64)buf;
            p.mp = mp;
            p.mv = mv;
            p.fb = fb;
            p.geometry = &m->rndr->geometry;
            p.materials = &m->rndr->materials;
            p.plines = &m->rndr->pline_cache;
            p.geom_gp = is_valid(gsg) ? &m->rndr->geom_groups[gsg] : nullptr;
            p.desc_info = &m->rndr->desc_info;
            p.fif = fif;
            p.dyn_state = &dyn_state;
            p.dcs = &cur_rj->dcs;
            p.instanced_dcs = &cur_rj->instanced_dcs;
            p.fns = &m->rndr->vk.inst.device.eds1_fns;
            cur_rj->cb(p, cur_rj->cb_user);
        }
        else {
            wlog("No draw function assigned for render-job %d (pass-id:%u view-id:%u rbp-pass:%s vkpass:%p blueprint:%s)",
                 rji,
                 cur_rj->mp,
                 cur_rj->mv,
                 rbp_pass->name,
                 rbp_pass->vk_handle,
                 m->rbp.item->name);
        }
        vkr_cmd_end_rpass(buf);

        // Update our working copy manifest states - we will copy these over to our renderer states once done executing
        // the manifest
        update_manifest_pass_states(m, *rbp_pass, *mp, fif);
    }
    vkr_end_cmd_buf(buf);
    return true;
}

#define SET_DSTATE_VAL(val, flag)                                                                                                          \
    ret.val = (test_flags(tech.can_override, flag) && test_flags(mat.override_mask, flag)) ? mat.dstate.val : tech.dstate.val

#define SET_DSTATE_RASTER_FLAGS(override_flag, raster_flags)                                                                               \
    ret.dflags |= (test_flags(tech.can_override, override_flag) && test_flags(mat.override_mask, override_flag))                           \
                      ? (mat.dstate.dflags & (raster_flags))                                                                               \
                      : (tech.dstate.dflags & (raster_flags))

intern rdraw_dyn_state get_dynamic_state(const rmaterial_info &mat, const rtechnique_pass_entry &tech)
{
    rdraw_dyn_state ret{};
    SET_DSTATE_RASTER_FLAGS(RDRAW_STATE_OVERRIDE_FLAG_CULLING, RTECHNIQUE_DYN_STATE_FLAG_CULL_FRONT | RTECHNIQUE_DYN_STATE_FLAG_CULL_BACK);
    SET_DSTATE_RASTER_FLAGS(RDRAW_STATE_OVERRIDE_FLAG_STENCIL_TEST, RTECHNIQUE_DYN_STATE_FLAG_STENCIL_TEST);
    SET_DSTATE_VAL(stencil_front, RDRAW_STATE_OVERRIDE_FLAG_STENCIL_OP_FRONT);
    SET_DSTATE_VAL(stencil_back, RDRAW_STATE_OVERRIDE_FLAG_STENCIL_OP_BACK);
    SET_DSTATE_VAL(blend_consts, RDRAW_STATE_OVERRIDE_FLAG_BLEND_CONSTANTS);
    SET_DSTATE_VAL(depth_b, RDRAW_STATE_OVERRIDE_FLAG_DEPTH_BIAS);
    return ret;
}


void update_view_data(rmanifest *m, idx_t view, const void *view_data)
{
    sizet blocksz = m->rndr->desc_info.view_ssbo.block_size;
    sizet buf_offset = blocksz * (m->fif * m->rndr->desc_info.view_ssbo.fif_block_count + view);
    void *dst = (u8*)m->rndr->desc_info.view_ssbo.buffer.mem_info.pMappedData + buf_offset;
    memcpy(dst, view_data, blocksz);
}

void update_instance_data(rmanifest *m, idx_t inst, const void *instance_data)
{
    sizet blocksz = m->rndr->desc_info.instance_ssbo.block_size;
    sizet buf_offset = blocksz * (m->fif * m->rndr->desc_info.instance_ssbo.fif_block_count + inst);
    void *dst = (u8*)m->rndr->desc_info.instance_ssbo.buffer.mem_info.pMappedData + buf_offset;
    memcpy(dst, instance_data, blocksz);
}

void update_material_data(rmanifest *m, rmaterial_handle mh, const void *data)
{
    auto minfo = get_slot_item(m->rndr->materials, mh);
    sizet blocksz = m->rndr->desc_info.material_ssbo.chunk_size;
    void *dst = vkr_get_chunk_ptr(&m->rndr->desc_info.material_ssbo,minfo->mat_ssbo, m->fif);
    memcpy(dst, data, blocksz);
}

idx_t push_pass(rmanifest *m, const mpass_params &p)
{
    idx_t pind = (idx_t)m->passes.size;
    arr_resize(&m->passes, pind + 1);
    m->passes[pind].rbpp = p.rbpp;
    if (p.assignments && p.assignment_count != 0) {
        asrt(p.assignment_count == m->rbp.item->passes[p.rbpp].slots.size);
        arr_resize(&m->passes[pind].slot_assignments, p.assignment_count);
        for (sizet i = 0; i < m->passes[pind].slot_assignments.size; ++i) {
            m->passes[pind].slot_assignments[i] = p.assignments[i];
        }
    }
    // SSBO per pass data update
    sizet blocksz = m->rndr->desc_info.pass_ssbo.block_size;
    sizet buf_offset = blocksz * (m->fif * m->rndr->desc_info.pass_ssbo.fif_block_count + pind);
    void *dst = (void *)((sizet)m->rndr->desc_info.pass_ssbo.buffer.mem_info.pMappedData + buf_offset);
    if (p.pass_sdata) {
        memcpy(dst, p.pass_sdata, blocksz);
    }
    else {
        memset(dst, 0, blocksz);
    }
    return pind;
}

u32 push_slot_assignment(rmanifest *m, idx_t pid, const mpass_slot_assignment &sa)
{
    // We cannot add more assignments than slots!
    asrt(m->passes[pid].slot_assignments.size < m->rbp.item->passes[m->passes[pid].rbpp].slots.size);

    u32 sa_ind = m->passes[pid].slot_assignments.size++;
    m->passes[pid].slot_assignments[sa_ind] = sa;
    return sa_ind;
}

idx_t push_view(rmanifest *m, const mview_params &p)
{
    idx_t ind = (idx_t)m->views.size;
    arr_push_back(&m->views, p.vdata);

    // SSBO per pass data update
    sizet blocksz = m->rndr->desc_info.view_ssbo.block_size;
    sizet buf_offset = blocksz * (m->fif * m->rndr->desc_info.view_ssbo.fif_block_count + ind);
    void *dst = (void *)((sizet)m->rndr->desc_info.view_ssbo.buffer.mem_info.pMappedData + buf_offset);
    if (p.view_sdata) {
        memcpy(dst, p.view_sdata, blocksz);
    }
    else {
        memset(dst, 0, blocksz);
    }
    return ind;
}

idx_t push_render_job(rmanifest *m, const mrender_job_params &p)
{
    idx_t ind = (idx_t)m->jobs.size;
    auto rj = arr_emplace_back(&m->jobs, p.pass, p.view, mem_arena{}, array<mdraw_call>{}, array<idx_t>{}, array<idx_t>{}, p.cb, p.cb_user);
    if (p.max_draw_calls > 0) {
        sizet memsz = calculate_render_job_needed_capacity(p.max_draw_calls, m->rndr->desc_info.draw_ssbo.block_size);
        // We don't need logging every frame for these - massive slow down
        rj->arena.flags |= make_flag(MEM_ARENA_DISABLE_INIT_LOG_BIT) | make_flag(MEM_ARENA_DISABLE_TERMINATE_LOG_BIT);
        init_linear_arena(&rj->arena, memsz, &m->rndr->manifest_flinear, "rjob_arena");
        arr_init(&rj->dcs, &rj->arena, p.max_draw_calls);
        arr_init(&rj->sorted_dcs, &rj->arena, p.max_draw_calls);
        arr_init(&rj->instanced_dcs, &rj->arena, p.max_draw_calls);
    }
    return ind;
}

u32 push_draw(rmanifest *m, const mdraw_params &dp)
{
    u32 push_cnt{0};
    idx_t fif = get_fif_ind(m->rndr);
    rtechnique_info *tptr = get_slot_item(&m->rndr->techniques, dp.tech);
    rmaterial_info *mptr = get_slot_item(&m->rndr->materials, dp.mat);

    for (u32 i = 0; i < tptr->rpass_plines.size; ++i) {
        auto cur_pl = &tptr->rpass_plines[i];
        auto dstate = get_dynamic_state(*mptr, *cur_pl);
        for (u32 rji = 0; rji < m->jobs.size; ++rji) {
            mrender_job *cur_rj = &m->jobs[rji];
            mpass *cur_mp = &m->passes[cur_rj->mp];
            if (cur_mp->rbpp == cur_pl->bp_pass) {
                u32 dc_ind = (u32)cur_rj->dcs.size;
                arr_resize(&cur_rj->dcs, dc_ind + 1);
                mdraw_call *cur_d = &cur_rj->dcs[dc_ind];

                cur_d->subgeom = dp.subgeom;
                cur_d->inst = dp.inst;
                cur_d->geom = dp.geom.si;
                cur_d->mat = dp.mat.si;
                cur_d->pl = cur_pl->pline.si;
                cur_d->sort_key = pack_mdraw_sort_key(*cur_d);
                cur_d->dstate = dstate;
                cur_d->inst_count = 1;
                ++push_cnt;
            }
        }
    }
    return push_cnt;
}

rmanifest *begin_render_frame(renderer *rndr, const rframe_begin_params &p)
{
    PROFILE_SCOPE("begin_render_frame");
    ptimer_split(&rndr->pt);
    auto dev = &rndr->vk.inst.device;

    // Update finished frames which is used to get the current frame
    idx_t fif = get_fif_ind(rndr);
    auto *cur_fif = &rndr->fifs[fif];

    // Window resize
    if (!window_resize_continue_check(rndr, cur_fif)) {
        return nullptr;
    }

    // We wait until this FIF's fence has been triggered before rendering the frame. FIF fences are created in a
    // triggered state so there will be no waiting on the first time. We then reset the fence (aka set it to
    // untriggered) and it is passed to the vkQueueSubmit call to trigger it again. So if not the first time rendering
    // this FIF, we are waiting for the vkQueueSubmit from the previous time this FIF was rendered to complete
    int vk_res = vkWaitForFences(dev->hndl, 1, &cur_fif->in_flight, VK_TRUE, UINT64_MAX);
    asrt(vk_res == VK_SUCCESS);

    /////////////////////////////////
    // Acquire Swapchain Image Ind //
    /////////////////////////////////
    // Acquire the image, signal the image_avail semaphore once the image has been acquired. We get the index back, but
    // that doesn't mean the image is ready. The image is only ready (on the GPU side) once the image avail semaphore is triggered
    vk_res =
        vkAcquireNextImageKHR(dev->hndl, dev->swapchain.swapchain, UINT64_MAX, cur_fif->image_avail, VK_NULL_HANDLE, &cur_fif->cur_im_ind);

    // If the image is out of date we need to recreate the swapchain and our caller needs to exit early as
    // well. It seems that on some platforms, if the result from above is out of date or suboptimal, the semaphore
    // associated with it will never get triggered. So if we were to continue and just resize at the end of frame it
    // wouldn't work because the queue submit would never fire as it depends on this image available semaphore.
    // At least.. i think?
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR) {
        cur_fif->swapchain_resize = WINDOW_RESIZE_DEBOUNCE_DURATION;
        return nullptr;
    }
    asrt(vk_res == VK_SUCCESS || vk_res == VK_SUBOPTIMAL_KHR);

    // Reset command pool
    for (u32 ti = 0; ti < cur_fif->thread_pools.size; ++ti) {
        vk_res = vkResetCommandPool(dev->hndl, cur_fif->thread_pools[ti].pool, {});
        asrt(vk_res == VK_SUCCESS);
    }

    reset_arena(&rndr->manifest_flinear);
    // vkr_reset_linear_arenas(&rndr->vk, fif);

    /////////////////////
    // Reset FIF Fence //
    /////////////////////
    // Here we reset the fence for the current frame fence as we know we are going to call queue submit which is the
    // only thing that will trigger the fence - so this is why this reset needs to come here (rather than right after
    // waiting) because if we return early due to swapchain resize, and we had reset the fence, then the next time our
    // frame came around we would just be stuck waiting forever
    vk_res = vkResetFences(dev->hndl, 1, &cur_fif->in_flight);
    asrt(vk_res == VK_SUCCESS);

    // Update our special swapchain handle
    auto sw = &rndr->vk.inst.device.swapchain;
    auto sw_hndl = find_rtexture_target(rndr, SWAPCHAIN_ID);
    auto swapchain = get_rtexture_target(rndr, sw_hndl);
    swapchain->frames[fif].view = sw->image_views[cur_fif->cur_im_ind];
    swapchain->frames[fif].image = sw->images[cur_fif->cur_im_ind];
    swapchain->frames[fif].state = {};

// Start GUI frame
#ifdef USE_IMGUI
    ImGui_ImplVulkan_NewFrame();
#endif

    rmanifest *m = create_manifest(rndr, fif);
    m->rndr = rndr;
    m->rbp = p.rbp;

    // UBO per pass frame update - the block size was already aligned to UBO min offset so no need to do any alignment
    // funny business here
    sizet blocksz = m->rndr->desc_info.frame_ubo.block_size;
    sizet buf_offset = fif * blocksz * m->rndr->desc_info.frame_ubo.fif_block_count;
    void *dst = (void *)((sizet)m->rndr->desc_info.frame_ubo.buffer.mem_info.pMappedData + buf_offset);
    if (p.frame_sdata) {
        memcpy(dst, p.frame_sdata, blocksz);
    }
    else {
        memset(dst, 0, blocksz);
    }
    return m;
}

bool end_render_frame(rmanifest *m)
{
    PROFILE_SCOPE("end_render_frame");
    asrt(m);
    asrt(is_valid(m->rbp));
    auto dev = &m->rndr->vk.inst.device;
    u32 fif = get_fif_ind(m->rndr);
    auto *cur_frame = &m->rndr->fifs[fif];


    // The command buf index struct has an ind struct into the pool the cmd buf comes from, and then an ind into the buffer
    // The ind into the pool has an ind into the queue family (as that contains our array of command pools) and then and
    // ind to the command pool
    // auto fb = &dev->swapchain.fbs[cur_frame->cur_im_ind];
    // asrt(fb && "Invalid framebuffer");

    ////////////////////////////
    // Record Command Buffers //
    ////////////////////////////
    // Just use buf 0 for now
    auto buf = cur_frame->thread_pools[0].buf;
    bool result = execute_manifest(m, buf, fif);
    if (!result) {
        return false;
    }

    //////////////////////////////////
    // Submit command buffer to GPU //
    //////////////////////////////////
    // Get the info ready to submit our command buffer to the queue. We need to wait until the image avail semaphore has
    // signaled, and then we need to trigger the render finished signal once the the command buffer completes
    VkSubmitInfo submit_info{};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &cur_frame->image_avail;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buf;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m->rndr->vk.inst.device.swapchain.renders_finished[cur_frame->cur_im_ind];
    s32 vk_res = vkQueueSubmit(dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE], 1, &submit_info, cur_frame->in_flight);
    asrt(vk_res == VK_SUCCESS);

    ///////////////////
    // Present Image //
    ///////////////////
    // Once the rendering signal has fired, present the image (show it on screen)
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m->rndr->vk.inst.device.swapchain.renders_finished[cur_frame->cur_im_ind];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &dev->swapchain.swapchain;
    present_info.pImageIndices = &cur_frame->cur_im_ind;
    present_info.pResults = nullptr; // Optional - check for individual swaps
    vk_res = vkQueuePresentKHR(dev->qfams[VKR_QUEUE_FAM_TYPE_PRESENT].qs[VKR_RENDER_QUEUE], &present_info);

    // Update global state from manifest
    update_global_target_state(m, fif);

    // This purely helps with smoothness - it works fine without recreating the swapchain here and instead doing it on
    // the next frame, but it seems to resize more smoothly doing it here
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR || vk_res == VK_SUBOPTIMAL_KHR) {
        cur_frame->swapchain_resize = WINDOW_RESIZE_DEBOUNCE_DURATION;
    }
    else {
        asrt(vk_res == VK_SUCCESS);
        ++m->rndr->finished_frames;
    }
    return true;
}

} // namespace nslib
