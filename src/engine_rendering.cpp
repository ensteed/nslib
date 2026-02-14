#include "engine_rendering.h"
#include "renderer.h"

namespace nslib
{

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
    bool result = is_valid(geom->rhndl);
    if (result) {
        ilog("Uploaded %s to renderer for %s", geom->type_str, ls(geom->name));
    }
    else {
        wlog("Could not upload %s to renderer for %s", geom->type_str, ls(geom->name));
    }
    mem_free(tmp_inds, scratch);
    mem_free(tmp_bone_weight_ids, scratch);
    mem_free(tmp_norm_tan_uvs, scratch);
    mem_free(tmp_pos_cols, scratch);
    mem_free(tmp_sgeoms, scratch);
    return result;
}

// Great use for a stack arena - will work
u32 upload_geometry(renderer *rndr, u32 stream_gp, asset_pool<geometry> *geometry, mem_arena *scratch)
{
    u32 success_count{0};
    for (auto rm = asset_pool_begin(geometry); is_valid(rm); rm = asset_pool_next(geometry, rm)) {
        success_count += (u32)upload_geometry(rndr, stream_gp, rm.item, scratch);
    }
    return success_count;
}

void upload_textures(renderer *rndr, texture_pool *tex_pool, mem_arena *scratch)
{
    for (auto iter = asset_pool_begin(tex_pool); is_valid(iter); iter = asset_pool_next(tex_pool, iter)) {
        rtexture_desc ctinfo{};
        ctinfo.name = ls(iter.item->name);
        ctinfo.dims = iter.item->dims;
        ctinfo.data = iter.item->pixels;
        ctinfo.data_size = get_texture_memsize(iter.item);
        ctinfo.format = get_rformat_for_usage(iter.item->usage);
        iter.item->rndr_hndl = create_rtexture(rndr, ctinfo);
        if (is_valid(iter.item->rndr_hndl)) {
            ilog("Uploaded %s %s to renderer", iter.item->type_str, ls(iter.item->name));
        }
        else {
            wlog("Failed to upload %s %s to renderer", iter.item->type_str, ls(iter.item->name));
        }
    }
}

// All we need to do currently is cast it!
intern rshader_stage_type get_renderer_shader_stage_type(shader_stage_type st)
{
    return (rshader_stage_type)st;
}

void upload_shaders(renderer *rndr, shader_pool *shdr_pool, mem_arena *scratch)
{
    for (auto iter = asset_pool_begin(shdr_pool); is_valid(iter); iter = asset_pool_next(shdr_pool, iter)) {
        rshader_desc rd{};
        rd.name = ls(iter.item->name);

        array<rshader_stage_desc> stages(iter.item->stages.size, scratch);
        arr_resize(&stages, iter.item->stages.size);
        for (sizet i = 0; i < stages.size; ++i) {
            auto cur_s = &stages[i];
            auto src_s = &iter.item->stages[i];
            cur_s->src_byte_size = src_s->src.size;
            cur_s->src = src_s->src.data;
            cur_s->entry_point = src_s->entry_point;
            cur_s->stype = get_renderer_shader_stage_type(src_s->stype);
        }
        rd.stage_cnt = stages.size;
        rd.stages = stages.data;

        iter.item->gpu_hndl = create_rshader(rndr, rd);
        if (is_valid(iter.item->gpu_hndl)) {
            ilog("Uploaded %s %s to renderer", iter.item->type_str, ls(iter.item->name));
        }
        else {
            wlog("Failed to upload %s %s to renderer", iter.item->type_str, ls(iter.item->name));
        }
    }
}

} // namespace nslib
