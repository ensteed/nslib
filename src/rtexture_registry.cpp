#include "vkr_utils.h"
#include "vkr_texture_pool.h"
#include "rtexture_registry.h"

namespace nslib
{

b32 is_valid(const rtexture_handle &h) {
    return h.pool_idx != INVALID_IDX && is_valid(h.hndl);
}

u32 get_slot_used_count(const rtexture_registry &reg)
{
    u32 slot_used_cnt{};
    for (u32 i = 0; i < reg.pools.size; ++i) {
        slot_used_cnt += get_slot_used_count(reg.pools[i].tpool);
    }
    return slot_used_cnt;
}

b32 init_rtexture_registry(rtexture_registry *reg, const rtexture_regisitry_cfg &cfg)
{
    ilog("Initializing texture registry with %u pools", cfg.pool_count);
    hmap_init(&reg->pmap, hash_type, cfg.arena, cfg.pool_count*2);
    arr_init(&reg->pools, cfg.arena, cfg.pool_count);
    arr_resize(&reg->pools, cfg.pool_count, vkr_texture_pool{});
    for (u32 i = 0; i < reg->pools.size; ++i) {
        vkr_texture_pool_cfg dst{};
        dst.arena = cfg.arena;
        dst.scratch = cfg.scratch;
        dst.dims = cfg.cfgs[i].tmeta.dims;
        dst.format = get_vk_format(cfg.cfgs[i].tmeta.fmt);
        dst.image_usage = {VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
        dst.pool_name = cfg.cfgs[i].pool_name;
        dst.type = test_flags(cfg.cfgs[i].tmeta.flags, RTEXTURE_FLAG_CUBEMAP) ? VKR_TEXTURE_POOL_TYPE_CUBE_ARRAY : VKR_TEXTURE_POOL_TYPE_2D_ARRAY;
        dst.mip_levels = cfg.cfgs[i].tmeta.mip_levels;
        dst.slot_count = cfg.cfgs[i].slot_count;
        dst.vk = cfg.vk;
        if (!vkr_init_texture_pool(&reg->pools[i], dst)) {
            terminate_rtexture_registry(reg);
            return false;
        }
    }
    return true;
}

void terminate_rtexture_registry(rtexture_registry *reg)
{
    ilog("Terminating texture registry");
    for (u32 i = 0; i < reg->pools.size; ++i) {
        vkr_terminate_texture_pool(&reg->pools[i]);
    }
    arr_terminate(&reg->pools);
    hmap_terminate(&reg->pmap);
}

rtexture_handle create_rtexture(rtexture_registry *reg, const rtexture_desc &tdesc)
{
    asrt(reg);
    asrt(tdesc.data);
    asrt(tdesc.meta.dims > uvec2{});
    asrt(tdesc.data_size > 0);
    asrt(tdesc.name);
    return {};
}


}
