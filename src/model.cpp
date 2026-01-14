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

void init_texture(texture *tex, const string &name, mem_arena *arena)
{
    init_robj(tex, name, arena);
    tex->name = name;
}

void release_texture_ram_data(texture *tex)
{
    
}

void terminate_texture(texture *tex)
{
    terminate_robj(tex);
    release_texture_ram_data(tex);
}

u32 get_texture_layer_pixel_count(const texture *tex)
{
    return tex->dims.w * tex->dims.h;
}

u32 get_texture_total_pixel_count(const texture *tex)
{
    return get_texture_layer_pixel_count(tex) * tex->dims.layers;
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
    }
}

sizet get_texture_memsize(const texture *tex)
{
    return get_texture_total_pixel_count(tex) * get_pixel_byte_size(tex->usage);
}

bool load_texture(texture *tex, const char *path, cstr *err)
{
    s32 channels{};
    auto stb_pixels = stbi_load(path, (s32 *)&tex->dims.w, (s32 *)&tex->dims.h, (s32 *)&channels, STBI_rgb_alpha);
    if (stb_pixels) {
        if (channels != STBI_rgb_alpha) {
            ilog("Converted %s from %d to %d channels", path, channels, STBI_rgb_alpha);
            channels = STBI_rgb_alpha;
        }
        auto sz = get_texture_memsize(tex);
        tex->pixels = mem_alloc(sz, tex->arena);
        memcpy(tex->pixels, stb_pixels, sz);
        stbi_image_free(stb_pixels);
        return true;
    }
    else if (err) {
        *err = stbi_failure_reason();
    }
    return false;
}

void init_material(material *mat, const string &name, mem_arena *arena)
{
    init_robj(mat, name, arena);
}

void terminate_material(material *mat)
{
    terminate_robj(mat);
}

void make_rect(mesh *msh, const string &name, mem_arena *arena)
{
    init_mesh(msh, name, arena);
    arr_copy(&msh->verts, RECT_VERTS, sizeof(RECT_VERTS) / sizeof(mvert));
    arr_copy(&msh->inds, RECT_INDS_TRI_LIST, sizeof(RECT_INDS_TRI_LIST) / sizeof(u32));
    arr_resize(&msh->sm_info, 1);
    msh->sm_info[0].count = msh->inds.size;
    strncpy(msh->sm_info[0].mat_slot_name, "default", SMALL_STR_LEN);
}

void make_cube(mesh *msh, const string &name, mem_arena *arena)
{
    init_mesh(msh, name, arena);
    arr_copy(&msh->verts, CUBE_VERTS, sizeof(CUBE_VERTS) / sizeof(mvert));
    arr_copy(&msh->inds, CUBE_INDS_TRI_LIST, sizeof(CUBE_INDS_TRI_LIST) / sizeof(u32));
    arr_resize(&msh->sm_info, 1);
    msh->sm_info[0].count = msh->inds.size;
    strncpy(msh->sm_info[0].mat_slot_name, "default", SMALL_STR_LEN);
}

void init_mesh(mesh *msh, const string &name, mem_arena *arena)
{
    init_robj(msh, name, arena);
    asrt(msh->sm_info.size == 0);
    asrt(msh->inds.size == 0);
    asrt(msh->verts.size == 0);
    asrt(msh->skinned_verts_info.size == 0);
    arr_init(&msh->verts, arena);
    arr_init(&msh->skinned_verts_info, arena);
    arr_init(&msh->inds, arena);
    arr_init(&msh->sm_info, arena);
}

void release_mesh_ram_data(mesh *msh)
{
    arr_terminate(&msh->verts);
    arr_terminate(&msh->skinned_verts_info);
    arr_terminate(&msh->inds);
    arr_terminate(&msh->sm_info);
}

void terminate_mesh(mesh *msh)
{
    terminate_robj(msh);
    release_mesh_ram_data(msh);
}

} // namespace nslib
