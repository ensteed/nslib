#include "vkr_texture_pool.h"
#include <string.h>

namespace nslib
{

struct pool_layer_range
{
    u32 base_layer{};
    u32 layer_count{};
};

intern const u32 get_layers_per_slot(const vkr_texture_pool &pool)
{
    return pool.type == VKR_TEXTURE_POOL_TYPE_CUBE_ARRAY ? 6u : 1u;
}

intern const u32 get_slot_count(const vkr_texture_pool &pool)
{
    return pool.tpool.slots.size / get_layers_per_slot(pool);
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

intern int get_layout_transition_masks(VkImageLayout old_layout,
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
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        *src_access = VK_ACCESS_NONE;
        *dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        *dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }

    if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        *src_access = VK_ACCESS_SHADER_READ_BIT;
        *dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }

    if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        *src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *dst_access = VK_ACCESS_SHADER_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        *dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        *src_access = VK_ACCESS_NONE;
        *dst_access = VK_ACCESS_SHADER_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        *dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }

    return err_code::VKR_TEXTURE_POOL_UNSUPPORTED_LAYOUT;
}

intern int transition_ranges_to_intent(vkr_texture_pool *pool,
                                       VkCommandBuffer cmd_buf,
                                       const pool_layer_range *ranges,
                                       u32 range_count,
                                       vkr_texture_pool_layout intent)
{
    asrt(pool);
    asrt(pool->vk);
    asrt(range_count > 0);
    auto new_layout = get_layout_from_intent(intent);

    array<VkImageMemoryBarrier> barriers{};
    arr_init(&barriers, pool->vk->cfg.arenas.command_arena, range_count);

    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;

    for (u32 range_i = 0; range_i < range_count; ++range_i) {
        u32 range_start = ranges[range_i].base_layer;
        u32 range_end = range_start + ranges[range_i].layer_count;
        asrt(range_end < pool->tpool.slots.size);

        u32 layer_i = range_start;
        while (layer_i < range_end) {
            auto old_layout = pool->tpool.slots[layer_i].item.layout;
            if (old_layout == new_layout) {
                ++layer_i;
                continue;
            }

            u32 chunk_start = layer_i;
            ++layer_i;
            while (layer_i < range_end && pool->tpool.slots[layer_i].item.layout == old_layout) {
                ++layer_i;
            }
            u32 chunk_count = layer_i - chunk_start;

            VkAccessFlags src_access{};
            VkAccessFlags dst_access{};
            VkPipelineStageFlags src_stage{};
            VkPipelineStageFlags dst_stage{};
            int err = get_layout_transition_masks(old_layout, new_layout, &src_access, &dst_access, &src_stage, &dst_stage);
            if (err != err_code::VKR_TEXTURE_POOL_NO_ERROR) {
                arr_terminate(&barriers);
                return err;
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = src_access;
            barrier.dstAccessMask = dst_access;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = pool->image.hndl;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = chunk_start;
            barrier.subresourceRange.layerCount = chunk_count;
            arr_push_back(&barriers, barrier);

            src_stage_mask |= src_stage;
            dst_stage_mask |= dst_stage;

            for (u32 update_i = chunk_start; update_i < chunk_start + chunk_count; ++update_i) {
                pool->tpool.slots[update_i].item.layout = new_layout;
            }
        }
    }

    if (barriers.size > 0) {
        vkCmdPipelineBarrier(cmd_buf, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, (u32)barriers.size, barriers.data);
    }

    arr_terminate(&barriers);
    return err_code::VKR_TEXTURE_POOL_NO_ERROR;
}

intern void build_contiguous_layer_ranges(const texture_pool_handle *handles, u32 handle_count, array<pool_layer_range> *out_ranges)
{
    asrt(handles);
    asrt(handle_count > 0);

    u32 first_slot = handles[0].index;
    u32 prev_slot = first_slot;
    u32 run_count = 1;
    for (u32 i = 1; i < handle_count; ++i) {
        u32 slot = handles[i].index;
        if (slot == prev_slot + 1) {
            ++run_count;
            prev_slot = slot;
            continue;
        }
        arr_push_back(out_ranges, {.base_layer = first_slot, .layer_count = run_count});
        first_slot = slot;
        prev_slot = slot;
        run_count = 1;
    }
    arr_push_back(out_ranges, {.base_layer = first_slot, .layer_count = run_count});
}

b32 vkr_init_texture_pool(vkr_texture_pool *pool, const vkr_texture_pool_cfg &cfg, const vkr_context *vk)
{
    asrt(pool);
    asrt(vk);
    asrt(cfg.arena);
    asrt(cfg.slot_count != 0);
    asrt(cfg.dims != uvec2{0u});
    asrt(cfg.format != VK_FORMAT_UNDEFINED);

    (*pool) = {};
    pool->vk = vk;
    pool->dims = cfg.dims;
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

int vkr_transition_pool_layout(vkr_texture_pool *pool,
                               VkCommandBuffer cmd_buf,
                               vkr_texture_pool_layout intent,
                               const texture_pool_handle *handles,
                               u32 handle_count)
{
    array<pool_slot_range> build_contiguous_layer_ranges(
        handles, handle_count, array<vkr_texture_pool_slot_range> * out_ranges, mem_arena * arena) for (u32 i = 0; i < handle_count; ++i)
    {

        range.base_layer = base_layer;
        range.layer_count = layer_count;
        return transition_ranges_to_intent(pool, cmd_buf, &range, 1, intent);
    }
}

int vkr_transition_pool_layout(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent, u32 slot_index, u32 slot_count)
{
    asrt(pool && pool->vk);

    if (layer_count == 0) {
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }
    if (base_layer + layer_count > pool->layer_count) {
        return err_code::VKR_TEXTURE_POOL_INVALID_RANGE;
    }

    pool_layer_range range{};
    range.base_layer = base_layer;
    range.layer_count = layer_count;
    return transition_ranges_to_intent(pool, cmd_buf, &range, 1, intent);
}

int vkr_transition_pool_layout(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent)
{
    pool_layer_range range{};
    range.base_layer = 0;
    range.layer_count = pool->layer_count;
    return transition_ranges_to_intent(pool, cmd_buf, &range, 1, intent);
}

int vkr_transition_pool_layout_layers(vkr_texture_pool *pool,
                                      VkCommandBuffer cmd_buf,
                                      vkr_texture_pool_layout intent,
                                      u32 base_layer,
                                      u32 layer_count)
{
    if (layer_count == 0) {
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }
    if (base_layer + layer_count > pool->layer_count) {
        return err_code::VKR_TEXTURE_POOL_INVALID_RANGE;
    }

    pool_layer_range range{};
    range.base_layer = base_layer;
    range.layer_count = layer_count;
    return transition_ranges_to_intent(pool, cmd_buf, &range, 1, intent);
}

int vkr_transition_pool_layout_slot(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent, u32 slot_index)
{
    asrt(slot_index <= get_slot_count(pool));
    return vkr_transition_pool_layout_layers(pool, cmd_buf, intent, slot_index * pool->layers_per_slot, pool->layers_per_slot);
}

bool vkr_pool_has_available_slots(const vkr_texture_pool *pool, u32 slot_count)
{
    asrt(pool);
    if (slot_count == 0) {
        return true;
    }

    u32 available{};
    for (u32 i = 0; i < pool->slot_count; ++i) {
        if (!pool->slot_used[i]) {
            ++available;
            if (available >= slot_count) {
                return true;
            }
        }
    }
    return false;
}

int vkr_acquire_slots(vkr_texture_pool *pool,
                      VkCommandBuffer cmd_buf,
                      const vkr_texture_source_image *src_images,
                      u32 src_image_count,
                      u32 slot_count,
                      u32 *out_slots)
{
    asrt(pool);
    if (slot_count == 0) {
        return err_code::VKR_TEXTURE_POOL_NO_ERROR;
    }

    if (!src_images || src_image_count != slot_count) {
        return err_code::VKR_TEXTURE_POOL_INVALID_SOURCE_IMAGE;
    }

    if (!vkr_pool_has_available_slots(pool, slot_count)) {
        return err_code::VKR_TEXTURE_POOL_OUT_OF_SLOTS;
    }

    for (u32 i = 0; i < slot_count; ++i) {
        if (!src_images[i].data || src_images[i].data_size == 0) {
            return err_code::VKR_TEXTURE_POOL_INVALID_SOURCE_IMAGE;
        }
    }

    array<u32> acquired_slots{};
    arr_init(&acquired_slots, pool->vk->cfg.arenas.command_arena, slot_count);

    for (u32 i = 0; i < pool->slot_count && acquired_slots.size < slot_count; ++i) {
        if (!pool->slot_used[i]) {
            pool->slot_used[i] = 1;
            arr_push_back(&acquired_slots, i);
        }
    }

    if (acquired_slots.size != slot_count) {
        arr_terminate(&acquired_slots);
        return err_code::VKR_TEXTURE_POOL_OUT_OF_SLOTS;
    }

    if (out_slots) {
        for (u32 i = 0; i < slot_count; ++i) {
            out_slots[i] = acquired_slots[i];
        }
    }

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
        vkr_buffer staging{};
        vkr_buffer_cfg staging_cfg{};
        staging_cfg.buffer_size = src_images[i].data_size;
        staging_cfg.alloc_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        staging_cfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        staging_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        staging_cfg.vma_alloc = &pool->vk->inst.device.vma_alloc;
        staging_cfg.vma_alloc_name = pool->staging_buffer_name;
        err = vkr_init_buffer(&staging, staging_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            transition_slot_ranges(pool, cmd_buf, slot_ranges.data, (u32)slot_ranges.size, VKR_TEXTURE_POOL_LAYOUT_INTENT_SHADER_READ);
            for (u32 slot_i = 0; slot_i < acquired_slots.size; ++slot_i) {
                pool->slot_used[acquired_slots[slot_i]] = 0;
            }
            arr_terminate(&slot_ranges);
            arr_terminate(&acquired_slots);
            return err_code::VKR_TEXTURE_POOL_STAGE_BUFFER_FAIL;
        }

        void *mapped = vkr_map_buffer(&staging, &pool->vk->inst.device.vma_alloc);
        memcpy(mapped, src_images[i].data, src_images[i].data_size);
        vkr_unmap_buffer(&staging, &pool->vk->inst.device.vma_alloc);

        arr_push_back(&pool->pending_staging_buffers, staging);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = acquired_slots[i] * pool->layers_per_slot;
        region.imageSubresource.layerCount = pool->layers_per_slot;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {pool->dims.x, pool->dims.y, 1};

        vkCmdCopyBufferToImage(cmd_buf, staging.hndl, pool->image.hndl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    err = transition_slot_ranges(pool, cmd_buf, slot_ranges.data, (u32)slot_ranges.size, VKR_TEXTURE_POOL_LAYOUT_INTENT_SHADER_READ);

    arr_terminate(&slot_ranges);
    arr_terminate(&acquired_slots);
    return err;
}

int vkr_texture_pool_acquire_slot(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, const vkr_texture_source_image *src_image, u32 *out_slot)
{
    if (!src_image) {
        return err_code::VKR_TEXTURE_POOL_INVALID_SOURCE_IMAGE;
    }

    return vkr_acquire_slots(pool, cmd_buf, src_image, 1, 1, out_slot);
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
