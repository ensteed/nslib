#include "platform.h"
#include "stb_image.h"
#include "model.h"

namespace nslib
{

// Colors are ARGB - msb gets alpha
intern const mvert RECT_VERTS[] = {
    {
        {-0.5f, -0.5f, 0.0f},
        {},
        {},
        {0.0f, 0.0f},
        0xffff0000,
    },
    {
        {0.5f, -0.5f, 0.0f},
        {},
        {},
        {1.0f, 0.0f},
        0xff00ff00,
    },
    {
        {0.5f, 0.5f, 0.0f},
        {},
        {},
        {1.0f, 1.0f},
        0xff0000ff,
    },
    {
        {-0.5f, 0.5f, 0.0f},
        {},
        {},
        {0.0f, 1.0f},
        0xff00ffff,
    },
};

intern const u32 RECT_INDS_TRI_LIST[] = {
    0,
    1,
    2,
    2,
    3,
    0,
};

// Colors are ARGB - msb gets alpha
intern mvert CUBE_VERTS[] = {
    {
        {-0.5f, 0.5f, 0.5f},
        {},
        {},
        {0.0f, 1.0f},
        0xff000000,
    },
    {
        {0.5f, 0.5f, 0.5f},
        {},
        {},
        {1.0f, 1.0f},
        0xff0000ff,
    },
    {
        {-0.5f, -0.5f, 0.5f},
        {},
        {},
        {0.0f, 0.0f},
        0xff00ff00,
    },
    {
        {0.5f, -0.5f, 0.5f},
        {},
        {},
        {1.0f, 0.0f},
        0xff00ffff,
    },
    {
        {-0.5f, 0.5f, -0.5f},
        {},
        {},
        {0.0f, 1.0f},
        0xffff0000,
    },
    {
        {0.5f, 0.5f, -0.5f},
        {},
        {},
        {1.0f, 1.0f},
        0xffff00ff,
    },
    {
        {-0.5f, -0.5f, -0.5f},
        {},
        {},
        {0.0f, 0.0f},
        0xffffff00,
    },
    {
        {0.5f, -0.5f, -0.5f},
        {},
        {},
        {1.0f, 0.0f},
        0xffffffff,
    },
};

intern const u32 CUBE_INDS_TRI_LIST[] = {
    0, 1, 2, // 0
    1, 3, 2, // 1
    4, 6, 5, // 2
    5, 6, 7, // 3
    0, 2, 4, // 4
    4, 2, 6, // 5
    1, 5, 3, // 6
    5, 7, 3, // 7
    0, 4, 1, // 8
    4, 5, 1, // 9
    2, 3, 6, // 10
    6, 3, 7, // 11
};

void init_asset(texture *tex)
{}

void release_ram_data(texture *tex)
{
    mem_free(tex->pixels, tex->fl);
}

void terminate_asset(texture *tex)
{
    release_ram_data(tex);
}

u32 get_texture_pixel_count(const texture *tex)
{
    return tex->dims.w * tex->dims.h * (test_flags(tex->flags, TEXTURE_FLAG_CUBEMAP) ? 6 : 1);
}

u8 get_pixel_byte_size(texture_usage usage)
{
    switch (usage) {
    case (texture_usage::ALBEDO):
        return 4;
    case (texture_usage::NORMAL):
        return 2;
    case (texture_usage::GRAYSCALE):
        return 1;
    case (texture_usage::HDR):
        return 8;
    default:
        asrt_break("Unhandled texture usage type");
        return 0;
    }
}

sizet get_texture_memsize(const texture *tex)
{
    return get_texture_pixel_count(tex) * get_pixel_byte_size(tex->usage);
}

const char *load_texture(texture *tex, const char *path)
{
    s32 channels{};
    auto stb_pixels = stbi_load(path, (s32 *)&tex->dims.w, (s32 *)&tex->dims.h, (s32 *)&channels, STBI_rgb_alpha);
    if (!stb_pixels) {
        return stbi_failure_reason();
    }

    if (channels != STBI_rgb_alpha) {
        ilog("Converted %s from %d to %d channels", path, channels, STBI_rgb_alpha);
        channels = STBI_rgb_alpha;
    }
    tex->usage = texture_usage::ALBEDO;
    auto sz = get_texture_memsize(tex);
    ilog("Allocating %lu bytes for texture (%lu bytes left in arena)", sz, tex->fl->total_size - tex->fl->used);
    tex->pixels = mem_alloc(sz, tex->fl);
    tex->mip_levels = 1;
    memcpy(tex->pixels, stb_pixels, sz);
    stbi_image_free(stb_pixels);
    return nullptr;
}

void init_asset(shader *shdr)
{
    arr_init(&shdr->stages, shdr->fl, 8);
}

void release_ram_data(shader *shdr)
{
    for (u32 i = 0; i < shdr->stages.size; ++i) {
        arr_terminate(&shdr->stages[i].src);
    }
}

void terminate_asset(shader *shdr)
{
    release_ram_data(shdr);
    arr_terminate(&shdr->stages);
}

const char *load_shader(shader *shdr, const char *path)
{
    arr_resize(&shdr->stages, 2);
    shdr->stages[0].stype = SHADER_STAGE_TYPE_VERTEX;
    shdr->stages[1].stype = SHADER_STAGE_TYPE_FRAGMENT;
    for (u32 i = 0; i < shdr->stages.size; ++i) {
        auto cur_stage = &shdr->stages[i];
        strncpy(cur_stage->entry_point, "main", SMALL_STR_LEN - 1);
        string str(path, shdr->frame_lin);
        str_append(&str, cur_stage->stype == SHADER_STAGE_TYPE_VERTEX ? ".vert.spv" : ".frag.spv");
        arr_init(&cur_stage->src, shdr->fl);
        platform_file_err_desc err{};
        read_file(str_cstr(str), &cur_stage->src, 0, &err);
        if (err.code != err_code::file::FILE_NO_ERROR) {
            release_ram_data(shdr);
            return err.str;
        }
        else {
            ilog("%s: loaded %s from %s", ls(shdr->name), get_shader_stage_str(cur_stage->stype), ls(str));
        }
    }
    return nullptr;
}

const char *get_shader_stage_str(shader_stage_type stype)
{
    switch (stype) {
    case (SHADER_STAGE_TYPE_VERTEX):
        return "vertex";
    case (SHADER_STAGE_TYPE_FRAGMENT):
        return "fragment";
    case (SHADER_STAGE_TYPE_COMPUTE):
        return "compute";
    default:
        return "undefined";
    }
}

void init_asset(technique *tech)
{
    arr_init(&tech->passes, tech->fl);
}

void terminate_asset(technique *tech)
{
    arr_terminate(&tech->passes);
}

void init_asset(material *mat)
{}

void terminate_asset(material *mat)
{}

void make_rect(geometry *geom)
{
    arr_copy(&geom->verts, RECT_VERTS, sizeof(RECT_VERTS) / sizeof(mvert));
    arr_copy(&geom->inds, RECT_INDS_TRI_LIST, sizeof(RECT_INDS_TRI_LIST) / sizeof(u32));
    arr_resize(&geom->sm_info, 1);
    geom->sm_info[0].count = geom->inds.size;
    strncpy(geom->sm_info[0].mat_slot_name, "default", SMALL_STR_LEN);
}

void make_cube(geometry *geom)
{
    arr_copy(&geom->verts, CUBE_VERTS, sizeof(CUBE_VERTS) / sizeof(mvert));
    arr_copy(&geom->inds, CUBE_INDS_TRI_LIST, sizeof(CUBE_INDS_TRI_LIST) / sizeof(u32));
    arr_resize(&geom->sm_info, 1);
    geom->sm_info[0].count = geom->inds.size;
    strncpy(geom->sm_info[0].mat_slot_name, "default", SMALL_STR_LEN);
}

idx_t find_subgeom_by_mat_slot(const geometry *geom, u32 mat_slot_ind)
{
    for (u32 i = 0; i < geom->sm_info.size; ++i) {
        if (geom->sm_info[i].mat_slot_ind == mat_slot_ind) {
            return i;
        }
    }
    return INVALID_IDX;
}

void init_asset(geometry *geom)
{
    asrt(geom->sm_info.size == 0);
    asrt(geom->inds.size == 0);
    asrt(geom->verts.size == 0);
    asrt(geom->skinned_verts_info.size == 0);
    arr_init(&geom->verts, geom->fl);
    arr_init(&geom->skinned_verts_info, geom->fl);
    arr_init(&geom->inds, geom->fl);
    arr_init(&geom->sm_info, geom->fl);
}

void release_ram_data(geometry *geom)
{
    arr_terminate(&geom->verts);
    arr_terminate(&geom->skinned_verts_info);
    arr_terminate(&geom->inds);
    arr_terminate(&geom->sm_info);
}

void terminate_asset(geometry *geom)
{
    release_ram_data(geom);
}

} // namespace nslib
