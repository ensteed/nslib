#include "engine_rendering.h"
#include "model.h"
#include "renderer.h"

namespace nslib
{

intern rformat get_rformat_for_usage(texture_usage usage)
{
    switch (usage) {
    case (texture_usage::ALBEDO):
        return rformat::RGBA8_SRGB;
    case (texture_usage::NORMAL):
        return rformat::RG8_UNORM;
    case (texture_usage::GRAYSCALE):
        return rformat::R8_UNORM;
    case (texture_usage::HDR):
        return rformat::RGBA16_SFLOAT;
    default:
        asrt_break("Failed to handle texture usage case");
        return rformat::INVALID;
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

u32 setup_geometry_stream_group(renderer *rndr)
{
    geometry_stream_group_desc desc{};
    desc.name = "nslib";
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

intern rtexture_flags get_rtexture_flags(asset_flags flags) {
    return test_flags(flags, TEXTURE_FLAG_CUBEMAP) ? RTEXTURE_FLAG_CUBEMAP : RTEXTURE_FLAG_NONE;
}

intern void set_technique_pass_desc(rtechnique_pass_desc *dst, const technique_pass &src, shader_pool *sp) {
    shader_item_ref shdr = find_asset(sp, src.shader);
    dst->shader = is_valid(shdr) ? shdr.item->rhndl : rshader_handle{};
    
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
    tdesc.pass_count = tech->passes.count;

    u32 i{};
    rtechnique_pass_desc *tmp_passes = mem_alloc<rtechnique_pass_desc>(scratch, tdesc.pass_count);
    for (auto pass_iter = hmap_begin(&tech->passes); pass_iter; pass_iter = hmap_next(&tech->passes, pass_iter)) {
        set_technique_pass_desc(&tmp_passes[i], pass_iter->val, sp);
        ++i;
    }
    create_rtechnique(rndr, tdesc);
    mem_free(tmp_passes, scratch);
    return get_and_log_upload_result(tech);
}

u32 upload_techniques(renderer *rndr, technique_pool *tech_pool, shader_pool *sp, mem_arena *scratch)
{
    auto upload_func = [rndr, sp, scratch](technique *tech) -> bool { return upload_technique(rndr, tech, sp, scratch); };
    return upload_assets_helper(tech_pool, upload_func);
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
