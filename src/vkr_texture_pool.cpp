#include "vkr_texture_pool.h"
#include "vkr_utils.h"
#include <string.h>

namespace nslib
{

struct pool_slot_range
{
    u32 base_ind{};
    u32 count{};
};

intern const u32 get_layers_per_slot(const vkr_texture_pool &pool)
{
    return pool.type == VKR_TEXTURE_POOL_TYPE_CUBE_ARRAY ? 6u : 1u;
}

intern const sizet get_layer_byte_size(const vkr_texture_pool &pool) {
    float bpp = get_bytes_per_pixel(pool.format);
    return sizet(pool.dims.w * pool.dims.h * bpp);
}

intern const sizet get_slot_byte_size(const vkr_texture_pool &pool)
{
    return get_layer_byte_size(pool) * get_layers_per_slot(pool);
}

intern const u32 get_layer_count(const vkr_texture_pool &pool)
{
    return pool.tpool.slots.size * get_layers_per_slot(pool);
}

intern const u32 get_layer_from_slot(const vkr_texture_pool &pool, u32 slot)
{
    return slot * get_layers_per_slot(pool);
}

intern VkImageLayout get_layout_from_intent(vkr_texture_pool_layout intent)
{
    switch (intent) {
    case VKR_TEXTURE_POOL_LAYOUT_TRANSFER_DST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case VKR_TEXTURE_POOL_LAYOUT_SHADER_READ:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default:
        asrt_break("Unhandled layout intent");
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

intern b32 get_layout_transition_masks(VkImageLayout old_layout,
                                       VkImageLayout new_layout,
                                       VkAccessFlags *src_access,
                                       VkAccessFlags *dst_access,
                                       VkPipelineStageFlags *src_stage,
                                       VkPipelineStageFlags *dst_stage)
{
    if (old_layout == new_layout) {
        *src_access = VK_ACCESS_NONE;
        *dst_access = VK_ACCESS_NONE;
        *src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        *dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        *src_access = VK_ACCESS_NONE;
        *dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        *dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        *src_access = VK_ACCESS_SHADER_READ_BIT;
        *dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        *src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *dst_access = VK_ACCESS_SHADER_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        *dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        *src_access = VK_ACCESS_NONE;
        *dst_access = VK_ACCESS_SHADER_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        *dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else {
        wlog("Invvalid layout transition");
        return false;
    }
    return true;
}

intern b32 transition_ranges_to_intent(vkr_texture_pool *pool,
                                       VkCommandBuffer cmd_buf,
                                       const pool_slot_range *ranges,
                                       u32 range_count,
                                       vkr_texture_pool_layout intent)
{
    asrt(pool);
    asrt(pool->vk);
    asrt(range_count > 0);
    auto new_layout = get_layout_from_intent(intent);

    array<VkImageMemoryBarrier> barriers{};
    arr_init(&barriers, pool->frame_scratch, range_count);

    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;

    for (u32 range_i = 0; range_i < range_count; ++range_i) {
        auto cur_slot_i = ranges[range_i].base_ind;
        VkImageMemoryBarrier *cur_barrier{};
        
        for (u32 soffset = 0; soffset < ranges[range_i].count; ++soffset) {
            VkImageLayout old_layout = pool->tpool.slots[cur_slot_i+soffset].item.layout;
            if (!cur_barrier || old_layout != cur_barrier->oldLayout) {
                cur_barrier = arr_push_back(&barriers, {});
                
                VkAccessFlags src_access{};
                VkAccessFlags dst_access{};
                VkPipelineStageFlags src_stage{};
                VkPipelineStageFlags dst_stage{};
                if (!get_layout_transition_masks(pool->tpool.slots[cur_slot_i+soffset].item.layout, new_layout, &src_access, &dst_access, &src_stage, &dst_stage)) return false;

                cur_barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                cur_barrier->srcAccessMask = src_access;
                cur_barrier->dstAccessMask = dst_access;
                cur_barrier->oldLayout = old_layout;
                cur_barrier->newLayout = new_layout;
                cur_barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                cur_barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                cur_barrier->image = pool->image.hndl;
                cur_barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                cur_barrier->subresourceRange.baseMipLevel = 0;
                cur_barrier->subresourceRange.levelCount = pool->;
                cur_barrier->subresourceRange.baseArrayLayer = get_layer_from_slot(*pool, cur_slot_i + soffset);

                src_stage_mask |= src_stage;
                dst_stage_mask |= dst_stage;
            }
            else {
                cur_barrier->subresourceRange.layerCount += get_layers_per_slot(*pool);
            }
        }
    }

    if (barriers.size > 0) {
        vkCmdPipelineBarrier(cmd_buf, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, (u32)barriers.size, barriers.data);
    }

    arr_terminate(&barriers);
    return true;
}

intern void build_contiguous_slot_ranges(const texture_slot_item_ref *tslots,
                                          u32 tslot_count,
                                          array<pool_slot_range> *out_ranges)
{
    asrt(tslots);
    asrt(tslot_count > 0);

    u32 first_slot = tslots[0].hndl.index;
    u32 prev_slot = first_slot;
    u32 run_count = 1;
    for (u32 i = 1; i < tslot_count; ++i) {
        u32 slot = tslots[i].hndl.index;
        if (slot == prev_slot + 1) {
            ++run_count;
            prev_slot = slot;
            continue;
        }
        arr_push_back(out_ranges,
                      {.base_ind = first_slot, .count = run_count});
        first_slot = slot;
        prev_slot = slot;
        run_count = 1;
    }
    arr_push_back(out_ranges, {.base_ind = first_slot, .count = run_count});
}

b32 vkr_init_texture_pool(vkr_texture_pool *pool, const vkr_texture_pool_cfg &cfg, const vkr_context *vk)
{
    asrt(pool);
    asrt(vk);
    asrt(cfg.arena);
    asrt(cfg.slot_count != 0);
    asrt(cfg.dims != uvec2{0u});
    asrt(cfg.format != VK_FORMAT_UNDEFINED);
    asrt(cfg.frame_scratch);

    pool->vk = vk;
    pool->frame_scratch = cfg.frame_scratch;
    pool->dims = cfg.dims;
    pool->mip_levels = cfg.mip_levels;
    pool->format = cfg.format;
    pool->type = cfg.type;

    init_slot_pool(&pool->tpool, cfg.slot_count, cfg.arena);
    arr_init(&pool->pending_staging_buffers, cfg.arena);

    vkr_image_cfg img_cfg{};
    img_cfg.dims = {cfg.dims.x, cfg.dims.y, 1};
    img_cfg.type = VK_IMAGE_TYPE_2D;
    img_cfg.format = cfg.format;
    img_cfg.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_cfg.usage = cfg.image_usage;
    img_cfg.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_cfg.im_create_flags = (cfg.type == VKR_TEXTURE_POOL_TYPE_CUBE_ARRAY) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    img_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    img_cfg.alloc_flags = 0;
    img_cfg.samples = VK_SAMPLE_COUNT_1_BIT;
    img_cfg.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    img_cfg.mip_levels = cfg.mip_levels;
    img_cfg.array_layers = (int)get_layers_per_slot(*pool) * cfg.slot_count;
    img_cfg.vma_alloc = &vk->inst.device.vma_alloc;
    img_cfg.vma_alloc_name = cfg.image_name;

    int err = vkr_init_image(&pool->image, img_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_texture_pool_cleanup_staging_buffers(pool);
        terminate_slot_pool(&pool->tpool);
        return false;
    }

    vkr_image_view_cfg view_cfg{};
    view_cfg.image = &pool->image;
    view_cfg.view_type = (cfg.type == VKR_TEXTURE_POOL_TYPE_CUBE_ARRAY) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view_cfg.srange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_cfg.srange.baseMipLevel = 0;
    view_cfg.srange.levelCount = 1;
    view_cfg.srange.baseArrayLayer = 0;
    view_cfg.srange.layerCount = img_cfg.array_layers;
    err = vkr_init_image_view(&pool->view, view_cfg, vk);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_terminate_image(&pool->image, vk);
        vkr_texture_pool_cleanup_staging_buffers(pool);
        terminate_slot_pool(&pool->tpool);
        return false;
    }

    return true;
}

void vkr_texture_pool_cleanup_staging_buffers(vkr_texture_pool *pool)
{
    asrt(pool && pool->vk);
    for (sizet i = 0; i < pool->pending_staging_buffers.size; ++i) {
        vkr_terminate_buffer(&pool->pending_staging_buffers[i], pool->vk);
    }
    arr_clear(&pool->pending_staging_buffers);
}

void vkr_terminate_texture_pool(vkr_texture_pool *pool)
{
    asrt(pool && pool->vk);
    vkr_texture_pool_cleanup_staging_buffers(pool);
    vkr_terminate_image_view(pool->view, pool->vk);
    vkr_terminate_image(&pool->image, pool->vk);
    terminate_slot_pool(&pool->tpool);
    arr_terminate(&pool->pending_staging_buffers);
    (*pool) = {};
}

b32 vkr_transition_texture_layouts(vkr_texture_pool *pool,
                                   VkCommandBuffer cmd_buf,
                                   vkr_texture_pool_layout intent,
                                   const texture_slot_item_ref *tslots,
                                   u32 tslot_count)
{
    asrt(tslots && tslot_count > 0);
    array<pool_slot_range> ranges{};
    arr_init(&ranges, pool->frame_scratch);
    build_contiguous_slot_ranges(tslots, tslot_count, &ranges);
    return transition_ranges_to_intent(pool, cmd_buf, ranges.data, ranges.size, intent);
}

b32 vkr_transition_pool_layout(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent)
{
    pool_slot_range range{.base_ind = 0, .count = (u32)pool->tpool.slots.size};
    return transition_ranges_to_intent(pool, cmd_buf, &range, 1, intent);
}

b32 vkr_upload_to_texture_slots(vkr_texture_pool *pool,
                                VkCommandBuffer cmd_buf,
                                const void *src_image_data,
                                const texture_slot_item_ref *tslots,
                                u32 count)
{
    asrt(pool);
    asrt(count > 0);
    asrt(src_image_data);
    asrt(tslots);

    vkr_buffer staging{};
    vkr_buffer_cfg staging_cfg{};
    staging_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    staging_cfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    staging_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    staging_cfg.vma_alloc = &pool->vk->inst.device.vma_alloc;
    staging_cfg.vma_alloc_name = "staging-buffer";
    staging_cfg.buffer_size = get_slot_byte_size(*pool) * count;

    s32 result = vkr_init_buffer(&staging, staging_cfg);
    if (result != err_code::VKR_NO_ERROR) {
        return false;
    }
    arr_push_back(&pool->pending_staging_buffers, staging);

    array<pool_slot_range> ranges;
    arr_init(&ranges, pool->frame_scratch);
    build_contiguous_slot_ranges(tslots, count, &ranges);

    transition_ranges_to_intent(pool, cmd_buf, ranges.data, ranges.size, VKR_TEXTURE_POOL_LAYOUT_TRANSFER_DST);

    sizet buff_offset{0};
    for (u32 rangei = 0; rangei < count; ++rangei) {
        VkBufferImageCopy region{};
        region.bufferOffset = buff_offset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = get_layer_from_slot(*pool, tslots[hi].index);
        region.imageSubresource.layerCount = get_layers_per_slot(*pool);
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {pool->dims.x, pool->dims.y, 1};
    }

    memcpy(staging.mem_info.pMappedData, src[hi].data, src[hi].data_size);
    vkCmdCopyBufferToImage(cmd_buf, staging.hndl, pool->image.hndl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    array<pool_slot_range> slot_ranges{};
    build_contiguous_slot_ranges(acquired_slots.data, (u32)acquired_slots.size, &slot_ranges, pool->vk->cfg.arenas.command_arena);

    int err = transition_slot_ranges(pool, cmd_buf, slot_ranges.data, (u32)slot_ranges.size, VKR_TEXTURE_POOL_LAYOUT_INTENT_TRANSFER_DST);
    if (err != err_code::VKR_TEXTURE_POOL_NO_ERROR) {
        for (u32 i = 0; i < acquired_slots.size; ++i) {
            pool->slot_used[acquired_slots[i]] = 0;
        }
        arr_terminate(&slot_ranges);
        arr_terminate(&acquired_slots);
        return err;
    }

    for (u32 i = 0; i < slot_count; ++i) {
    }

    err = transition_slot_ranges(pool, cmd_buf, slot_ranges.data, (u32)slot_ranges.size, VKR_TEXTURE_POOL_LAYOUT_INTENT_SHADER_READ);

    arr_terminate(&slot_ranges);
    arr_terminate(&acquired_slots);
    return err;
}
}

b32 vkr_acquire_texture_slots(vkr_texture_pool *pool,
                              VkCommandBuffer cmd_buf,
                              const vkr_texture_source_image *src_images,
                              u32 src_image_count,
                              texture_slot_item_ref *slots_out)
{
    asrt(pool);
    asrt(slots_out);
    asrt(src_images);
    asrt(src_image_count > 0);

    u32 remaining_slots = get_slots_available_count(pool->tpool);
    if (remaining_slots < src_image_count) {
        wlog("Not enough slots (requested:%u available:%u)", src_image_count, remaining_slots);
        return false;
    }

    for (u32 i = 0; i < src_image_count; ++i) {
        slots_out[i] = acquire_slot(&pool->tpool);
        asrt(is_valid(slots_out[i]));
    }
    return true;
}

int vkr_texture_pool_acquire_slot(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, const vkr_texture_source_image *src_image, u32 *out_slot)
{
    if (!src_image) {
        return err_code::VKR_TEXTURE_POOL_INVALID_SOURCE_IMAGE;
    }

    return vkr_acquire_texture_slots(pool, cmd_buf, src_image, 1, 1, out_slot);
}

void vkr_texture_pool_release_slot(vkr_texture_pool *pool, u32 slot_index)
{
    asrt(pool);
    if (slot_index >= pool->slot_count) {
        return;
    }

    pool->slot_used[slot_index] = 0;
}

void vkr_texture_pool_release_slots(vkr_texture_pool *pool, const u32 *slot_indices, u32 slot_count)
{
    asrt(pool);
    if (!slot_indices) {
        return;
    }
    for (u32 i = 0; i < slot_count; ++i) {
        vkr_texture_pool_release_slot(pool, slot_indices[i]);
    }
}

} // namespace nslib
