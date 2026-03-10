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
    hmap_init(&reg->pmap, hash_type, cfg.persist_fl, cfg.pool_count*2);
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

rtexture_handle create_rtexture(rtexture_registry *reg, const rtexture_desc &tdesc, gpu_handle transient_pool)
{
    asrt(reg);
    asrt(tdesc.data);
    asrt(tdesc.meta.dims > uvec2{});
    asrt(tdesc.data_size > 0);
    asrt(tdesc.name);
    rtexture_handle ret{};
    u64 key = hash_type(&tdesc.meta,sizeof(rtexture_meta));
    auto pool_fiter = hmap_find(&reg->pmap, key);
    if (!pool_fiter) return ret;

    vkr_texture_pool *pool = &reg->pools[pool_fiter->val];
    rtexture_pool_item_ref slot;
    b32 success = vkr_acquire_texture_slots(pool, 1, &slot);
    if (!success) return {};

    asrt(is_valid(slot));
            
    VkCommandBuffer tmp_cmd_buf;
    s32 result = vkr_alloc_cmd_bufs(&tmp_cmd_buf, {.pool = (VkCommandPool)transient_pool}, pool->vk);
    asrt(result == err_code::VKR_NO_ERROR);
    auto tmp_q = pool->vk->inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE];

    result = vkr_begin_cmd_buf(tmp_cmd_buf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    asrt(result == err_code::VKR_NO_ERROR);
            
    vkr_source_image_data src_data{};
    src_data.data = tdesc.data;
    src_data.name = tdesc.name;
    success = vkr_upload_to_texture_slots(pool, tmp_cmd_buf, &src_data, &slot, 1);
    if (!success) {
        vkr_release_texture_slots(pool, &slot.hndl, 1);
        return ret;
    }

    result = vkr_end_cmd_buf(tmp_cmd_buf);
    asrt(result == err_code::VKR_NO_ERROR);
                
    result = vkr_blocking_queue_submit(tmp_q, &tmp_cmd_buf, 1, pool->vk);
    asrt(result == err_code::VKR_NO_ERROR);
    
    ret.pool_idx = pool_fiter->val;
    ret.hndl = slot.hndl;
    return ret;
}


}
