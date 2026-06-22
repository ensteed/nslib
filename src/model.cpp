#include "platform.h"
#include "stb_image.h"
#include "model.h"
#include "math/algorithm.h"

namespace nslib
{

// Colors are ARGB - msb gets alpha
intern const gvert RECT_VERTS[] = {
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

// 24 verts (4 per face). UVs are (0,0) bottom-left, (1,1) top-right per face.
intern gvert CUBE_VERTS[] =
{
    // +Z (front)
    {{-0.5f, -0.5f,  0.5f}, {0,0,1}, {1,0,0}, {0,0}, 0xffffffff},
    {{ 0.5f, -0.5f,  0.5f}, {0,0,1}, {1,0,0}, {1,0}, 0xffffffff},
    {{-0.5f,  0.5f,  0.5f}, {0,0,1}, {1,0,0}, {0,1}, 0xffffffff},
    {{ 0.5f,  0.5f,  0.5f}, {0,0,1}, {1,0,0}, {1,1}, 0xffffffff},

    // -Z (back)
    {{ 0.5f, -0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {0,0}, 0xffffffff},
    {{-0.5f, -0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {1,0}, 0xffffffff},
    {{ 0.5f,  0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {0,1}, 0xffffffff},
    {{-0.5f,  0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {1,1}, 0xffffffff},

    // +X (right)
    {{ 0.5f, -0.5f,  0.5f}, {1,0,0}, {0,0,-1}, {0,0}, 0xffffffff},
    {{ 0.5f, -0.5f, -0.5f}, {1,0,0}, {0,0,-1}, {1,0}, 0xffffffff},
    {{ 0.5f,  0.5f,  0.5f}, {1,0,0}, {0,0,-1}, {0,1}, 0xffffffff},
    {{ 0.5f,  0.5f, -0.5f}, {1,0,0}, {0,0,-1}, {1,1}, 0xffffffff},

    // -X (left)
    {{-0.5f, -0.5f, -0.5f}, {-1,0,0}, {0,0,1}, {0,0}, 0xffffffff},
    {{-0.5f, -0.5f,  0.5f}, {-1,0,0}, {0,0,1}, {1,0}, 0xffffffff},
    {{-0.5f,  0.5f, -0.5f}, {-1,0,0}, {0,0,1}, {0,1}, 0xffffffff},
    {{-0.5f,  0.5f,  0.5f}, {-1,0,0}, {0,0,1}, {1,1}, 0xffffffff},

    // +Y (top)
    {{-0.5f,  0.5f,  0.5f}, {0,1,0}, {1,0,0}, {0,0}, 0xffffffff},
    {{ 0.5f,  0.5f,  0.5f}, {0,1,0}, {1,0,0}, {1,0}, 0xffffffff},
    {{-0.5f,  0.5f, -0.5f}, {0,1,0}, {1,0,0}, {0,1}, 0xffffffff},
    {{ 0.5f,  0.5f, -0.5f}, {0,1,0}, {1,0,0}, {1,1}, 0xffffffff},

    // -Y (bottom)
    {{-0.5f, -0.5f, -0.5f}, {0,-1,0}, {1,0,0}, {0,0}, 0xffffffff},
    {{ 0.5f, -0.5f, -0.5f}, {0,-1,0}, {1,0,0}, {1,0}, 0xffffffff},
    {{-0.5f, -0.5f,  0.5f}, {0,-1,0}, {1,0,0}, {0,1}, 0xffffffff},
    {{ 0.5f, -0.5f,  0.5f}, {0,-1,0}, {1,0,0}, {1,1}, 0xffffffff},
};

// 36 indices (2 tris per face)
intern u32 CUBE_INDS_TRI_LIST[] =
{
     0,  1,  2,   2,  1,  3,   // +Z
     4,  5,  6,   6,  5,  7,   // -Z
     8,  9, 10,  10,  9, 11,   // +X
    12, 13, 14,  14, 13, 15,   // -X
    16, 17, 18,  18, 17, 19,   // +Y
    20, 21, 22,  22, 21, 23,   // -Y
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
{
    mat->flags |= ASSET_FLAG_DIRTY;
}

void terminate_asset(material *mat)
{}

void make_unit_rect(geometry *geom)
{
    arr_copy(&geom->verts, RECT_VERTS, sizeof(RECT_VERTS) / sizeof(gvert));
    arr_copy(&geom->inds, RECT_INDS_TRI_LIST, sizeof(RECT_INDS_TRI_LIST) / sizeof(u32));
    arr_resize(&geom->sm_info, 1);
    geom->sm_info[0].count = geom->inds.size;
    strncpy(geom->sm_info[0].mat_slot_name, "default", SMALL_STR_LEN);
}

void make_unit_cube(geometry *geom)
{
    arr_copy(&geom->verts, CUBE_VERTS, ARR_SIZE(CUBE_VERTS));
    arr_copy(&geom->inds, CUBE_INDS_TRI_LIST, ARR_SIZE(CUBE_INDS_TRI_LIST));
    arr_resize(&geom->sm_info, 1);
    geom->sm_info[0].count = geom->inds.size;
    strncpy(geom->sm_info[0].mat_slot_name, "default", SMALL_STR_LEN);
}

void make_unit_sphere(geometry *geom, u32 precision)
{
    if (precision < 3) {
        precision = 3;
    }

    u32 stack_count = precision;
    u32 slice_count = precision * 2;
    f32 radius = 0.5f;

    u32 vert_count = (stack_count + 1) * (slice_count + 1);
    u32 ind_count = stack_count * slice_count * 6;

    arr_resize(&geom->verts, vert_count);
    arr_resize(&geom->inds, ind_count);

    u32 vert_ind = 0;
    for (u32 stack = 0; stack <= stack_count; ++stack) {
        f32 stack_frac = (f32)stack / (f32)stack_count;
        f32 phi = stack_frac * math::PI;
        f32 sin_phi = math::sin(phi);
        f32 cos_phi = math::cos(phi);

        for (u32 slice = 0; slice <= slice_count; ++slice) {
            f32 slice_frac = (f32)slice / (f32)slice_count;
            f32 theta = slice_frac * math::PI * 2.0f;
            f32 sin_theta = math::sin(theta);
            f32 cos_theta = math::cos(theta);

            vec3 norm{
                cos_theta * sin_phi,
                sin_theta * sin_phi,
                cos_phi,
            };

            vec3 tan{
                -sin_theta,
                cos_theta,
                0.0f,
            };

            if (stack == 0 || stack == stack_count) {
                tan = {1.0f, 0.0f, 0.0f};
            }

            geom->verts[vert_ind++] = {
                norm * radius,
                norm,
                tan,
                {slice_frac, 1.0f - stack_frac},
                0xffffffff,
            };
        }
    }

    u32 ind_ind = 0;
    for (u32 stack = 0; stack < stack_count; ++stack) {
        u32 cur_row = stack * (slice_count + 1);
        u32 next_row = cur_row + slice_count + 1;

        for (u32 slice = 0; slice < slice_count; ++slice) {
            u32 cur = cur_row + slice;
            u32 next = cur + 1;
            u32 below = next_row + slice;
            u32 below_next = below + 1;

            geom->inds[ind_ind++] = cur;
            geom->inds[ind_ind++] = next;
            geom->inds[ind_ind++] = below;

            geom->inds[ind_ind++] = below;
            geom->inds[ind_ind++] = next;
            geom->inds[ind_ind++] = below_next;
        }
    }

    arr_resize(&geom->sm_info, 1);
    geom->sm_info[0].offset = 0;
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
