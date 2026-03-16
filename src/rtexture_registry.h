#pragma once
#include "rformat.h"
#include "containers/array.h"
#include "containers/hmap.h"
#include "math/vector2.h"
#include "render_defs.h"
// #include "vkr_texture_pool.h"

namespace nslib
{

struct vkr_texture_pool;
struct vkr_context;

struct rtexture_registry
{
    hmap<u64, rtexture_pool_idx> pmap;
    array<vkr_texture_pool> pools;
    const vkr_context *vk;
};

enum rtexture_flag {
    RTEXTURE_FLAG_NONE = 0,
    RTEXTURE_FLAG_CUBEMAP = make_flag(0),
};
using rtexture_flags = u32;

struct rtexture_meta {
    rformat fmt;
    uvec2 dims;
    u32 mip_levels;
    rtexture_flags flags;
};

struct rtexture_pool_cfg
{
    rtexture_meta tmeta;
    const char *pool_name;
    u32 slot_count;    
};

struct rtexture_regisitry_cfg
{
    mem_arena *persist_fl;
    mem_arena *scratch_stack;
    u32 pool_count;
    const rtexture_pool_cfg *cfgs;
    vkr_context *vk;
};

struct rtexture_desc
{
    const char *name;
    // Pixel data
    const void *data;
    // For validation basically
    sizet data_size;
    rtexture_meta meta;
};


b32 is_valid(const rtexture_handle &h);
u32 get_slot_used_count(const rtexture_registry &reg);

b32 init_rtexture_registry(rtexture_registry *reg, const rtexture_regisitry_cfg &cfg);
void terminate_rtexture_registry(rtexture_registry *reg);
rtexture_handle create_rtexture(rtexture_registry *reg, const rtexture_desc &tdesc, gpu_handle transient_pool);



} // namespace nslib
