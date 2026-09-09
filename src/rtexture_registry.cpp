#include "vkr_utils.h"
#include "vkr_texture_pool.h"
#include "rtexture_registry.h"

namespace nslib
{

b8 is_valid(const rtexture_handle &h)
{
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

b8 init_rtexture_registry(rtexture_registry *reg, const rtexture_regisitry_cfg &cfg)
{
    ilog("Initializing texture registry with %u pools", cfg.pool_count);
    hmap_init(&reg->pmap, cfg.persist_fl, cfg.pool_count * 2);
    arr_init(&reg->pools, cfg.persist_fl, cfg.pool_count);
    arr_resize(&reg->pools, cfg.pool_count, vkr_texture_pool{});
    for (u32 i = 0; i < reg->pools.size; ++i) {
        vkr_texture_pool_cfg dst{};
        dst.tmeta = cfg.cfgs[i].tmeta;
        dst.persist_fl = cfg.persist_fl;
        dst.scratch_stack = cfg.scratch_stack;
        dst.image_usage = {VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
        dst.pool_name = cfg.cfgs[i].pool_name;
        dst.slot_count = cfg.cfgs[i].slot_count;
        dst.vk = cfg.vk;
        if (!vkr_init_texture_pool(&reg->pools[i], dst)) {
            terminate_rtexture_registry(reg);
            return false;
        }
        u64 key = hash_type(&cfg.cfgs[i].tmeta, sizeof(rtexture_meta));
        hmap_insert(&reg->pmap, key, i);
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

} // namespace nslib
