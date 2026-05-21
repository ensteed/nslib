#include "render_manifest.h"
#include "sim_region.h"
#include "engine_rendering.h"
#include "model.h"
#include "renderer.h"

namespace nslib
{

intern rstencil_op_state get_stencil_op_state(stencil_mode mode)
{
    rstencil_op_state ret{};
    switch (mode) {
    case STENCIL_MODE_OFF:
        break;
    case STENCIL_MODE_WRITE:
        ret.on_fail = RSTENCIL_OP_REPLACE;
        ret.on_pass = RSTENCIL_OP_REPLACE;
        ret.on_depth_fail = RSTENCIL_OP_REPLACE;
        ret.comp = RCOMPARE_OP_ALWAYS;
        ret.comp_mask = 0xff;
        ret.write_mask = 0xff;
        ret.ref = 1;
        break;
    case STENCIL_MODE_CLIP_INSIDE:
        ret.on_fail = RSTENCIL_OP_KEEP;
        ret.on_pass = RSTENCIL_OP_KEEP;
        ret.on_depth_fail = RSTENCIL_OP_KEEP;
        ret.comp = RCOMPARE_OP_EQUAL;
        ret.comp_mask = 0xff;
        ret.write_mask = 0x00;
        ret.ref = 1;
        break;
    case STENCIL_MODE_CLIP_OUTSIDE:
        ret.on_fail = RSTENCIL_OP_KEEP;
        ret.on_pass = RSTENCIL_OP_KEEP;
        ret.on_depth_fail = RSTENCIL_OP_KEEP;
        ret.comp = RCOMPARE_OP_NOT_EQUAL;
        ret.comp_mask = 0xff;
        ret.write_mask = 0x00;
        ret.ref = 1;
        break;
    default:
        asrt_break("Failed to handle stencil mode case");
        break;
    }
    return ret;
}

intern idx_t get_rsampler(mat_sampler_type stype)
{
    switch (stype) {
    case (MAT_SAMPLER_TYPE_LINEAR_REPEAT):
        return RSAMPLER_TYPE_LINEAR_REPEAT;
    default:
        return INVALID_IDX;
    }
}

intern rdraw_state_override_flags get_dynamic_state_override_flags(raster_override_state_flags raster_flags)
{
    rdraw_state_override_flags ret{};
    set_flag_from_bool(ret, RDRAW_STATE_OVERRIDE_FLAG_CULLING, test_flags(raster_flags, RASTER_OVERRIDE_STATE_CULLING));
    set_flag_from_bool(ret, RDRAW_STATE_OVERRIDE_FLAG_STENCIL_TEST, test_flags(raster_flags, RASTER_OVERRIDE_STATE_STENCIL_TEST));
    set_flag_from_bool(ret, RDRAW_STATE_OVERRIDE_FLAG_STENCIL_OP_FRONT, test_flags(raster_flags, RASTER_OVERRIDE_STATE_STENCIL_MODE));
    set_flag_from_bool(ret, RDRAW_STATE_OVERRIDE_FLAG_STENCIL_OP_BACK, test_flags(raster_flags, RASTER_OVERRIDE_STATE_STENCIL_MODE));
    set_flag_from_bool(ret, RDRAW_STATE_OVERRIDE_FLAG_BLEND_CONSTANTS, test_flags(raster_flags, RASTER_OVERRIDE_STATE_BLEND_CONSTANTS));
    set_flag_from_bool(ret, RDRAW_STATE_OVERRIDE_FLAG_DEPTH_BIAS, test_flags(raster_flags, RASTER_OVERRIDE_STATE_DEPTH_BIAS));
    return ret;
}

intern rdraw_dyn_state get_dynamic_state(const raster_state &st)
{
    rdraw_dyn_state ds{};
    set_flag_from_bool(ds.dflags, RTECHNIQUE_DYN_STATE_FLAG_CULL_FRONT, test_flags(st.rmask, RASTER_FLAG_CULL_FRONT));
    set_flag_from_bool(ds.dflags, RTECHNIQUE_DYN_STATE_FLAG_CULL_BACK, test_flags(st.rmask, RASTER_FLAG_CULL_BACK));
    set_flag_from_bool(ds.dflags, RTECHNIQUE_DYN_STATE_FLAG_STENCIL_TEST, test_flags(st.rmask, RASTER_FLAG_STENCIL_TEST));
    ds.stencil_front = get_stencil_op_state(st.sm);
    ds.stencil_back = get_stencil_op_state(st.sm);
    ds.blend_consts = st.blend_constants;
    ds.depth_b.const_factor = st.depth_bias.x;
    ds.depth_b.slope_factor = st.depth_bias.y;
    ds.depth_b.clamp = st.depth_bias.z;
    return ds;
}

intern rdraw_dyn_state get_dynamic_state(const material &mat)
{
    return get_dynamic_state(mat.overrides);
}

intern rdraw_dyn_state get_dynamic_state(const technique_pass &tpass)
{
    return get_dynamic_state(tpass.dflt_st);
}

intern rformat get_rformat_for_usage(texture_usage usage)
{
    switch (usage) {
    case (texture_usage::ALBEDO):
        return RFMT_RGBA8_SRGB;
    case (texture_usage::NORMAL):
        return RFMT_RG8_UNORM;
    case (texture_usage::GRAYSCALE):
        return RFMT_R8_UNORM;
    case (texture_usage::HDR):
        return RFMT_RGBA16_SFLOAT;
    default:
        asrt_break("Failed to handle texture usage case");
        return RFMT_INVALID;
    }
}

template<typename T>
intern bool get_and_log_upload_result(T *asset)
{
    bool result = is_valid(asset->rhndl);
    if (result) {
        ilog("Uploaded %s %s to renderer", asset->type_str, ls(asset->name));
    }
    else {
        wlog("Failed to upload %s %s to renderer", asset->type_str, ls(asset->name));
    }
    return result;
}

// All we need to do currently is cast it!
intern rshader_stage_type get_renderer_shader_stage_type(shader_stage_type st)
{
    return (rshader_stage_type)st;
}

template<typename PoolT, typename UploadFunc>
intern u32 upload_assets_helper(PoolT *pool, UploadFunc func)
{
    u32 success_count{0};
    for (auto aiter = asset_pool_begin(pool); is_valid(aiter); aiter = asset_pool_next(pool, aiter)) {
        success_count += (u32)func(aiter.item);
    }
    return success_count;
}

void update_materials(rmanifest *m, asset_cache *cg)
{
    auto mpool = get_asset_pool<material>(cg);
    auto tex_pool = get_asset_pool<texture>(cg);

    // Update materials that need updating
    for (auto mat_iter = asset_pool_begin(mpool); is_valid(mat_iter); mat_iter = asset_pool_next(mpool, mat_iter)) {
        auto minfo = get_slot_item(&m->rndr->materials, mat_iter.item->rhndl);
        if (test_flags(mat_iter.item->flags, ASSET_FLAG_DIRTY)) {
            minfo->fif_dirty = MAX_FRAMES_IN_FLIGHT;
        }
        if (minfo->fif_dirty > 0) {
            minfo->dstate = get_dynamic_state(*mat_iter.item);
            minfo->override_mask = get_dynamic_state_override_flags(mat_iter.item->override_mask);

            material_ssbo_data md{};
            md.col = mat_iter.item->col;
            md.use_col = (u32)test_flags(mat_iter.item->flags, MATERIAL_FLAG_USE_COLOR);
            md.sampler_idx = get_rsampler(mat_iter.item->textures[MAT_SAMPLER_SLOT_ALBEDO].sampler);
            auto diffuse = find_asset(tex_pool, mat_iter.item->textures[MAT_SAMPLER_SLOT_ALBEDO].id);
            md.tex_pool_idx = is_valid(diffuse) ? diffuse.item->rhndl.pool_idx : INVALID_IDX;
            md.tex_layer = is_valid(diffuse) ? diffuse.item->rhndl.hndl.si : 0;
            update_material_data(m, mat_iter.item->rhndl, &md);
        }
    }
}

void update_transforms(rmanifest *m, sim_region *sr)
{
    transform_tbl *tf_tbl = get_comp_tbl<transform>(&sr->cdb);
    for (u32 i = 0; i < tf_tbl->entries.size; ++i) {
        transform *tf = &tf_tbl->entries[i];
        idx_t tfind = get_comp_ind(tf, tf_tbl);

        if (is_comp_dirty(*tf)) {
            tf->rfif_dirty = MAX_FRAMES_IN_FLIGHT;
            tf->flags &= ~COMP_FLAG_DIRTY;
        }

        if (tf->rfif_dirty > 0) {
            tf->cached_prev = tf->cached;
            tf->cached = math::model_tform(tf->world_pos, tf->orientation, tf->scale);
            instance_ssbo_data update_d{};
            update_d.model = tf->cached;
            update_d.prev_model = tf->cached_prev;
            update_instance_data(m, tfind, &update_d);
            --tf->rfif_dirty;
        }
    }
}

void update_cameras(rmanifest *m, sim_region *sr)
{
    camera_tbl *cam_tbl = get_comp_tbl<camera>(&sr->cdb);
    transform_tbl *tf_tbl = get_comp_tbl<transform>(&sr->cdb);

    for (u32 i = 0; i < cam_tbl->entries.size; ++i) {
        camera *cam = &cam_tbl->entries[i];
        auto tf = get_comp(cam->ent_id, tf_tbl);

        if (is_comp_dirty(*cam)) {
            if (cam->ptype == CAMERA_PROJ_TYPE_PERSPECTIVE) {
                cam->proj = cam->fov_type == CAMERA_FOV_TYPE_VERTICAL
                                ? math::perspective_vfov(cam->fov, cam->aspect, cam->near_far.x, cam->near_far.y)
                                : math::perspective_hfov(cam->fov, cam->aspect, cam->near_far.x, cam->near_far.y);
            }
            else {
                f32 half_h, half_w;
                if (cam->fov_type == CAMERA_FOV_TYPE_VERTICAL) {
                    half_h = cam->fov * 0.5f;
                    half_w = half_h * cam->aspect;
                }
                else {
                    half_w = cam->fov * 0.5f;
                    half_h = half_w / cam->aspect;
                }
                cam->proj = math::ortho(-half_w, half_w, half_h, -half_h, cam->near_far.x, cam->near_far.y);
            }
        }

        // We compute this on every frame - likely cheaper than looking up to see if each transform has a cam comp and
        // marking it dirty if it does
        cam->view = math::inverse(tf->cached);
        cam->proj_view = cam->proj * cam->view;
    }
}

void enqueue_draws(rmanifest *m, sim_region *sr, asset_cache *cg, material *def_material)
{
    auto tf_tbl = get_comp_tbl<transform>(&sr->cdb);
    auto sm_tbl = get_comp_tbl<static_mesh>(&sr->cdb);
    auto techpool = get_asset_pool<technique>(cg);
    auto geompool = get_asset_pool<geometry>(cg);
    auto mpool = get_asset_pool<material>(cg);

    for (u32 i = 0; i < sm_tbl->entries.size; ++i) {
        static_mesh *sm = &sm_tbl->entries[i];
        transform *tf = get_comp(sm->ent_id, tf_tbl);
        idx_t tfind = get_comp_ind(tf, tf_tbl);

        geometry_item_ref gref = find_asset<geometry>(geompool, sm->geom_id);
        if (!is_valid(gref)) {
            // TODO: Probably have a default mesh?
            wlog("No geometry found for %s", ls(sr->ents[i].name));
            continue;
        }

        for (u32 mi = 0; mi < sm->mat_mapping.size; ++mi) {
            material_item_ref mref = find_asset<material>(mpool, sm->mat_mapping[mi].mat_id);
            material *mat = is_valid(mref) ? mref.item : def_material;

            for (u32 bpi = 0; bpi < mat->bp_techniques.size; ++bpi) {
                if (mat->bp_techniques[bpi].bpid == m->rbp.item->id) {
                    technique_item_ref tref = find_asset<technique>(techpool, mat->bp_techniques[bpi].tech_id);
                    asrt(is_valid(tref));

                    mdraw_params dp{};
                    dp.geom = gref.item->rhndl;
                    dp.subgeom = find_subgeom_by_mat_slot(gref.item, sm->mat_mapping[mi].sm_mat_slot);
                    dp.mat = mat->rhndl;
                    dp.tech = tref.item->rhndl;
                    dp.inst = tfind;
                    asrt(tref.item->passes.size > 0);
                    push_draw(m, dp);
                }
            }
        }
    }
}

void update_and_draw_region(rmanifest *m, sim_region *sr, asset_cache *cg, material *def_material)
{
    // auto smtbl = get_comp_tbl<static_mesh>(&sr->cdb);
    // auto tftbs = get_comp_tbl<transform>(&sr->cdb);
    // auto cam_tbl = get_comp_tbl<camera>(&sr->cdb);
    // auto mpool = get_asset_pool<material>(cg);
    // auto tex_pool = get_asset_pool<texture>(cg);
    // auto techpool = get_asset_pool<technique>(cg);
    // auto geompool = get_asset_pool<geometry>(cg);

    update_materials(m, cg);
    update_transforms(m, sr);
    update_cameras(m, sr);
    enqueue_draws(m, sr, cg, def_material);
}

u32 setup_geometry_stream_group(renderer *rndr)
{
    geometry_stream_group_desc desc{};
    desc.name = MAIN_GEOM_STREAM_GP;
    desc.max_ind_count = MAX_TOTAL_GEOM_IND_COUNT;

    geometry_vert_layout_desc *static_geom_layout = push_geometry_layout(&desc, MAX_STATIC_GEOM_VERT_COUNT);
    vert_stream_desc *pos_col = push_geometry_stream(static_geom_layout, "static-pos-col");
    u32 shader_location = 0;
    push_geometry_attribute<vec3>(pos_col, shader_location++);
    push_geometry_attribute<u8vec4>(pos_col, shader_location++, true);

    vert_stream_desc *norm_tan_uv = push_geometry_stream(static_geom_layout, "static-tan-norm-uv");
    push_geometry_attribute<vec3>(norm_tan_uv, shader_location++);
    push_geometry_attribute<vec3>(norm_tan_uv, shader_location++);
    push_geometry_attribute<vec2>(norm_tan_uv, shader_location++);

    geometry_vert_layout_desc *skinned_geom_layout = push_geometry_layout(&desc, MAX_SKINNED_GEOM_VERT_COUNT);
    pos_col = push_geometry_stream(skinned_geom_layout, "skinned-pos-col");
    push_geometry_attribute<vec3>(pos_col, shader_location++);
    push_geometry_attribute<u8vec4>(pos_col, shader_location++, true);

    norm_tan_uv = push_geometry_stream(skinned_geom_layout, "skinned-tan-norm-uv");
    push_geometry_attribute<vec3>(norm_tan_uv, shader_location++);
    push_geometry_attribute<vec3>(norm_tan_uv, shader_location++);
    push_geometry_attribute<vec2>(norm_tan_uv, shader_location++);

    vert_stream_desc *bone_weight_ids = push_geometry_stream(skinned_geom_layout, "skinned-bone-weight-ids");
    push_geometry_attribute<vec3>(bone_weight_ids, shader_location++);
    push_geometry_attribute<vec3>(bone_weight_ids, shader_location++);
    push_geometry_attribute<vec2>(bone_weight_ids, shader_location++);

    return push_geometry_stream_group(rndr, desc);
}

bool upload_geometry(renderer *rndr, u32 stream_gp, geometry *geom, mem_arena *scratch)
{
    ilog("Registering geom id: %s  name: %s", ls(geom->name), str_cstr(geom->name));
    asrt(geom->verts.size > 0);
    rgeom_desc cinf{};

    // Vert/Ind counts
    cinf.vert_count = geom->verts.size;
    cinf.ind_count = geom->inds.size;

    // Subgeom ranges
    cinf.subgeom_cnt = geom->sm_info.size;

    // bone weight ids will be null if size is 0 - size will either be 0 or same size as verts (we assert that now)
    asrt(geom->skinned_verts_info.size == cinf.vert_count || geom->skinned_verts_info.size == 0);

    // Allocate temporary buffers for everything
    rsubgeom_range *tmp_sgeoms = mem_alloc<rsubgeom_range>(scratch, cinf.subgeom_cnt);
    rgeom_vert_pos_col *tmp_pos_cols = mem_alloc<rgeom_vert_pos_col>(scratch, cinf.vert_count);
    rgeom_vert_norm_tan_uv *tmp_norm_tan_uvs = mem_alloc<rgeom_vert_norm_tan_uv>(scratch, cinf.vert_count);
    rgeom_vert_bone_weights_ids *tmp_bone_weight_ids = mem_alloc<rgeom_vert_bone_weights_ids>(scratch, geom->skinned_verts_info.size);
    ind_t *tmp_inds = mem_alloc<ind_t>(scratch, cinf.ind_count);

    // Copy subgeoms
    for (u32 i = 0; i < cinf.subgeom_cnt; ++i) {
        tmp_sgeoms[i].count = geom->sm_info[i].count;
        tmp_sgeoms[i].offset = geom->sm_info[i].offset;
    }

    // Copy vert data
    for (u32 i = 0; i < cinf.vert_count; ++i) {
        tmp_pos_cols[i].pos = geom->verts[i].pos;
        tmp_pos_cols[i].col = geom->verts[i].col;
        tmp_norm_tan_uvs[i].norm = geom->verts[i].norm;
        tmp_norm_tan_uvs[i].tangent = geom->verts[i].tan;
        tmp_norm_tan_uvs[i].uv = geom->verts[i].uv;
        if (tmp_bone_weight_ids) {
            tmp_bone_weight_ids[i].bone_weights = geom->skinned_verts_info[i].bone_weights;
            tmp_bone_weight_ids[i].bone_ids = geom->skinned_verts_info[i].bone_ids;
        }
    }

    // Copy indices
    for (u32 i = 0; i < cinf.ind_count; ++i) {
        tmp_inds[i] = geom->inds[i];
    }

    void *vdata[] = {tmp_pos_cols, tmp_norm_tan_uvs, tmp_bone_weight_ids};
    cinf.name = str_cstr(geom->name);
    cinf.group = stream_gp;
    cinf.layout = tmp_bone_weight_ids ? RVERT_LAYOUT_SKINNED_GEOM : RVERT_LAYOUT_STATIC_GEOM;
    cinf.vert_data = vdata;
    cinf.ind_data = tmp_inds;
    cinf.subgeoms = tmp_sgeoms;
    cinf.topology = (rgeom_topology)geom->topology;

    geom->rhndl = create_rgeometry(rndr, cinf);
    bool result = get_and_log_upload_result(geom);

    mem_free(tmp_inds, scratch);
    mem_free(tmp_bone_weight_ids, scratch);
    mem_free(tmp_norm_tan_uvs, scratch);
    mem_free(tmp_pos_cols, scratch);
    mem_free(tmp_sgeoms, scratch);
    return result;
}

// Great use for a stack arena - will work
u32 upload_geometries(renderer *rndr, u32 stream_gp, asset_pool<geometry> *geom_pool, mem_arena *scratch)
{
    auto upload_func = [rndr, stream_gp, scratch](geometry *geom) -> bool { return upload_geometry(rndr, stream_gp, geom, scratch); };
    return upload_assets_helper(geom_pool, upload_func);
}

intern rtexture_flags get_rtexture_flags(asset_flags flags)
{
    return test_flags(flags, TEXTURE_FLAG_CUBEMAP) ? RTEXTURE_FLAG_CUBEMAP : RTEXTURE_FLAG_NONE;
}

intern void set_technique_pass_desc(rtechnique_pass_desc *dst, const technique_pass &src, shader_pool *sp, render_blueprint_ref bp)
{
    idx_t pass_idx = find_rbp_pass(bp.item, src.bp_pass);
    asrt(is_valid(pass_idx));

    shader_item_ref shdr = find_asset(sp, src.shader);
    dst->shader = is_valid(shdr) ? shdr.item->rhndl : rshader_handle{};
    dst->bp_info.bp = bp.hndl;
    dst->bp_info.pid = pass_idx;
    dst->bp_info.spi = src.bp_subpass;
    dst->geom_buffer_layout = src.gsg_layout;

    // Sensible defaults for fields not driven by technique_pass
    dst->topology = (rgeom_topology)src.topology;
    dst->poly_mode = (rpolygon_mode)src.poly_mode;
    dst->tess_patch_control_points = 0;
    dst->logic_op = RLOGIC_OP_COPY;
    dst->depth_bounds = {0.0f, 1.0f};
    dst->tmask = 0;

    dst->dstate = get_dynamic_state(src);
    dst->dstate_can_override = get_dynamic_state_override_flags(src.can_override);

    // DEPTH_MODE_OFF disables both test and write entirely
    bool depth_active = (src.dm != DEPTH_MODE_OFF);
    set_flag_from_bool(dst->tmask, RTECHNIQUE_DESC_FLAG_DEPTH_TEST, depth_active && test_flags(src.dflt_st.rmask, RASTER_FLAG_DEPTH_TEST));
    set_flag_from_bool(dst->tmask, RTECHNIQUE_DESC_FLAG_DEPTH_WRITE, depth_active && test_flags(src.dflt_st.rmask, RASTER_FLAG_DEPTH_WRITE));

    // depth_mode → compare op
    switch (src.dm) {
    case DEPTH_MODE_NORMAL:
        dst->depth_compare_op = RCOMPARE_OP_LESS;
        break;
    case DEPTH_MODE_ALWAYS:
        dst->depth_compare_op = RCOMPARE_OP_ALWAYS;
        break;
    case DEPTH_MODE_BEHIND:
        dst->depth_compare_op = RCOMPARE_OP_GREATER;
        break;
    case DEPTH_MODE_MATCH:
        dst->depth_compare_op = RCOMPARE_OP_EQUAL;
        break;
    case DEPTH_MODE_OFF:
        dst->depth_compare_op = RCOMPARE_OP_ALWAYS;
        break; // irrelevant, test disabled
    }

    // blend_mode → per-attachment blend state (set all slots uniformly;
    // create_rtechnique only reads as many as the blueprint pass has)
    rcolor_component_flags write_all = RCOLOR_COMPONENT_R | RCOLOR_COMPONENT_G | RCOLOR_COMPONENT_B | RCOLOR_COMPONENT_A;
    u32 att_cnt = get_rbp_slot_count(bp.item->passes[pass_idx]);
    for (u32 i = 0; i < att_cnt; ++i) {
        auto *att = &dst->atts_blending[i];
        att->write_mask = write_all;
        switch (src.bm) {
        case BLEND_MODE_OPAQUE:
        case BLEND_MODE_MASKED: // alpha cutoff handled in shader, no HW blending
            att->blend_enable = false;
            att->color = {RBLEND_FACTOR_ONE, RBLEND_FACTOR_ZERO, RBLEND_OP_ADD};
            att->alpha = {RBLEND_FACTOR_ONE, RBLEND_FACTOR_ZERO, RBLEND_OP_ADD};
            break;
        case BLEND_MODE_TRANSPARENT:
            att->blend_enable = true;
            att->color = {RBLEND_FACTOR_SRC_ALPHA, RBLEND_FACTOR_ONE_MINUS_SRC_ALPHA, RBLEND_OP_ADD};
            att->alpha = {RBLEND_FACTOR_ONE, RBLEND_FACTOR_ONE_MINUS_SRC_ALPHA, RBLEND_OP_ADD};
            break;
        case BLEND_MODE_ADDITIVE:
            att->blend_enable = true;
            att->color = {RBLEND_FACTOR_SRC_ALPHA, RBLEND_FACTOR_ONE, RBLEND_OP_ADD};
            att->alpha = {RBLEND_FACTOR_ONE, RBLEND_FACTOR_ONE, RBLEND_OP_ADD};
            break;
        case BLEND_MODE_CONSTANT:
            att->blend_enable = true;
            att->color = {RBLEND_FACTOR_CONSTANT_ALPHA, RBLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA, RBLEND_OP_ADD};
            att->alpha = {RBLEND_FACTOR_CONSTANT_ALPHA, RBLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA, RBLEND_OP_ADD};
            break;
        }
    }
}

bool upload_texture(renderer *rndr, texture *tex, mem_arena *scratch)
{
    rtexture_desc ctinfo{};
    ctinfo.name = ls(tex->name);
    ctinfo.meta.dims = tex->dims;
    ctinfo.meta.flags = get_rtexture_flags(tex->flags);
    ctinfo.meta.fmt = get_rformat_for_usage(tex->usage);
    ctinfo.meta.mip_levels = tex->mip_levels;
    ctinfo.data = tex->pixels;
    ctinfo.data_size = get_texture_memsize(tex);
    tex->rhndl = create_rtexture(rndr, ctinfo);
    return get_and_log_upload_result(tex);
}

u32 upload_textures(renderer *rndr, texture_pool *tex_pool, mem_arena *scratch)
{
    auto upload_func = [rndr, scratch](texture *tex) -> bool { return upload_texture(rndr, tex, scratch); };
    return upload_assets_helper(tex_pool, upload_func);
}

bool upload_technique(renderer *rndr, technique *tech, shader_pool *sp, mem_arena *scratch)
{
    rtechnique_desc tdesc{};
    tdesc.name = ls(tech->name);
    tdesc.pass_count = tech->passes.size;
    auto bp = find_render_blueprint(rndr, tech->bpid);

    rtechnique_pass_desc *tmp_passes = mem_alloc<rtechnique_pass_desc>(scratch, tdesc.pass_count);
    for (u32 i = 0; i < tech->passes.size; ++i) {
        set_technique_pass_desc(&tmp_passes[i], tech->passes[i], sp, bp);
        ++i;
    }
    tdesc.passes = tmp_passes;
    tech->rhndl = create_rtechnique(rndr, tdesc);
    mem_free(tmp_passes, scratch);
    return get_and_log_upload_result(tech);
}

u32 upload_techniques(renderer *rndr, technique_pool *tech_pool, shader_pool *sp, mem_arena *scratch)
{
    auto upload_func = [rndr, sp, scratch](technique *tech) -> bool { return upload_technique(rndr, tech, sp, scratch); };
    return upload_assets_helper(tech_pool, upload_func);
}

bool upload_material(renderer *rndr, material *mat, texture_pool *tex_pool, mem_arena *scratch)
{
    rmaterial_desc md{};
    md.name = ls(mat->name);
    md.dstate = get_dynamic_state(*mat);
    md.dstate_override_mask = get_dynamic_state_override_flags(mat->override_mask);
    static_assert((u8)MAT_SAMPLER_SLOT_COUNT <= (u8)RMATERIAL_TEXTURE_COUNT);
    for (u32 i = 0; i < MAT_SAMPLER_SLOT_COUNT; ++i) {
        auto tex = find_asset(tex_pool, mat->textures[i].id);
        md.slots[i] = tex.item ? tex.item->rhndl : rtexture_handle{};
        if (!is_valid(tex) && is_valid(mat->textures[i].id)) {
            wlog("Failed to load texture id %lu from texture pool", mat->textures[i].id);
        }
    }
    mat->rhndl = create_rmaterial(rndr, md);
    return get_and_log_upload_result(mat);
}

u32 upload_materials(renderer *rndr, material_pool *mat_pool, texture_pool *tex_pool, mem_arena *scratch)
{
    auto upload_func = [rndr, tex_pool, scratch](material *mat) -> bool { return upload_material(rndr, mat, tex_pool, scratch); };
    return upload_assets_helper(mat_pool, upload_func);
}

bool upload_shader(renderer *rndr, shader *shdr, mem_arena *scratch)
{
    rshader_desc rd{};
    rd.name = ls(shdr->name);

    array<rshader_stage_desc> stages(shdr->stages.size, scratch);
    arr_resize(&stages, shdr->stages.size);
    for (sizet i = 0; i < stages.size; ++i) {
        auto cur_s = &stages[i];
        auto src_s = &shdr->stages[i];
        cur_s->src_byte_size = src_s->src.size;
        cur_s->src = src_s->src.data;
        cur_s->entry_point = src_s->entry_point;
        cur_s->stype = get_renderer_shader_stage_type(src_s->stype);
    }
    rd.stage_cnt = stages.size;
    rd.stages = stages.data;

    shdr->rhndl = create_rshader(rndr, rd);
    return get_and_log_upload_result(shdr);
}

u32 upload_shaders(renderer *rndr, shader_pool *shdr_pool, mem_arena *scratch)
{
    auto upload_func = [rndr, scratch](shader *shdr) -> bool { return upload_shader(rndr, shdr, scratch); };
    return upload_assets_helper(shdr_pool, upload_func);
}

} // namespace nslib
