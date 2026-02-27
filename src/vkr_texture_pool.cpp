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
    return test_flags(pool.tmeta.flags, RTEXTURE_FLAG_CUBEMAP) ? 6u : 1u;
}

intern const u32 get_layer_count(const vkr_texture_pool &pool)
{
    return pool.tpool.slots.size * get_layers_per_slot(pool);
}

intern const u32 get_layer_from_slot(const vkr_texture_pool &pool, u32 slot)
{
    return slot * get_layers_per_slot(pool);
}

intern VkImageLayout get_layout_for_format(VkFormat format)
{
    if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT) {
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    }
    else if (format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
    else {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

intern VkImageLayout get_layout_from_intent(vkr_texture_pool_layout intent, VkFormat format)
{
    switch (intent) {
    case VKR_TEXTURE_POOL_LAYOUT_TRANSFER_DST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case VKR_TEXTURE_POOL_LAYOUT_SHADER_READ:
        return get_layout_for_format(format);
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
    bool new_layout_depth_format = (new_layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL ||
                                    new_layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
                                    new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ||
                                    new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
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
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        *src_access = VK_ACCESS_NONE;
        *dst_access = VK_ACCESS_SHADER_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        *dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout_depth_format) {
        *src_access = VK_ACCESS_NONE;
        *dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        *dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        *src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *dst_access = VK_ACCESS_SHADER_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        *dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout_depth_format) {
        *src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        *dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        *src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        *dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else {
        wlog("Invvalid layout transition");
        return false;
    }
    return true;
}

intern void transition_ranges_to_intent(vkr_texture_pool *pool,
                                        VkCommandBuffer cmd_buf,
                                        const pool_slot_range *ranges,
                                        u32 range_count,
                                        vkr_texture_pool_layout intent)
{
    asrt(pool);
    asrt(pool->vk);
    asrt(range_count > 0);
    auto vkfmt = get_vk_format(pool->tmeta.fmt);
    auto new_layout = get_layout_from_intent(intent, vkfmt);

    array<VkImageMemoryBarrier> barriers{};
    arr_init(&barriers, pool->scratch_stack, range_count);

    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;

    for (u32 range_i = 0; range_i < range_count; ++range_i) {
        auto base_slot_i = ranges[range_i].base_ind;
        VkImageMemoryBarrier *cur_barrier{};

        for (u32 slot_offset = 0; slot_offset < ranges[range_i].count; ++slot_offset) {
            auto cur_slot_item = &pool->tpool.slots[base_slot_i + slot_offset].item;
            VkImageLayout old_layout = cur_slot_item->layout;

            if (!cur_barrier || old_layout != cur_barrier->oldLayout) {
                VkAccessFlags src_access{};
                VkAccessFlags dst_access{};
                VkPipelineStageFlags src_stage{};
                VkPipelineStageFlags dst_stage{};
                asrt(get_layout_transition_masks(cur_slot_item->layout, new_layout, &src_access, &dst_access, &src_stage, &dst_stage));

                cur_barrier = arr_push_back(&barriers, {});
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
                cur_barrier->subresourceRange.levelCount = pool->tmeta.mip_levels;
                cur_barrier->subresourceRange.baseArrayLayer = get_layer_from_slot(*pool, base_slot_i + slot_offset);
                cur_barrier->subresourceRange.layerCount = get_layers_per_slot(*pool);
                src_stage_mask |= src_stage;
                dst_stage_mask |= dst_stage;
            }
            else {
                cur_barrier->subresourceRange.layerCount += get_layers_per_slot(*pool);
            }

            // Update the slot layout
            cur_slot_item->layout = new_layout;
        }
    }

    if (barriers.size > 0) {
        vkCmdPipelineBarrier(cmd_buf, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, (u32)barriers.size, barriers.data);
    }

    arr_terminate(&barriers);
}

intern void build_contiguous_slot_ranges(const rtexture_pool_item_ref *tslots, u32 tslot_count, array<pool_slot_range> *out_ranges)
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
        arr_push_back(out_ranges, {.base_ind = first_slot, .count = run_count});
        first_slot = slot;
        prev_slot = slot;
        run_count = 1;
    }
    arr_push_back(out_ranges, {.base_ind = first_slot, .count = run_count});
}

b32 vkr_init_texture_pool(vkr_texture_pool *pool, const vkr_texture_pool_cfg &cfg)
{
    asrt(pool);
    asrt(cfg.vk);
    asrt(cfg.persist_fl);
    asrt(cfg.slot_count != 0);
    asrt(cfg.tmeta.dims != uvec2{0u});
    asrt(cfg.tmeta.fmt != RFMT_INVALID);
    asrt(cfg.scratch_stack);

    pool->vk = cfg.vk;
    pool->scratch_stack = cfg.scratch_stack;
    pool->tmeta = cfg.tmeta;

    ilog("Initializing %s %s %ux%u texture pool (%s) with %u slots each having %u mips",
         test_flags(cfg.tmeta.flags, RTEXTURE_FLAG_CUBEMAP) ? "cube" : "2d",
         get_rformat_str(cfg.tmeta.fmt),
         cfg.tmeta.dims.w,
         cfg.tmeta.dims.h,
         cfg.pool_name,
         cfg.slot_count,
         cfg.tmeta.mip_levels);

    init_slot_pool(&pool->tpool, cfg.slot_count, cfg.persist_fl);
    arr_init(&pool->pending_staging_buffers, cfg.persist_fl);

    bool is_cubemap = test_flags(cfg.tmeta.flags, RTEXTURE_FLAG_CUBEMAP);
    vkr_image_cfg img_cfg{};
    img_cfg.dims = {cfg.tmeta.dims.x, cfg.tmeta.dims.y, 1};
    img_cfg.type = VK_IMAGE_TYPE_2D;
    img_cfg.format = get_vk_format(cfg.tmeta.fmt);
    img_cfg.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_cfg.usage = cfg.image_usage;
    img_cfg.im_create_flags = is_cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    img_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    img_cfg.alloc_flags = 0;
    img_cfg.samples = VK_SAMPLE_COUNT_1_BIT;
    img_cfg.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    img_cfg.mip_levels = cfg.tmeta.mip_levels;
    img_cfg.array_layers = (int)get_layers_per_slot(*pool) * cfg.slot_count;
    img_cfg.vma_alloc = &pool->vk->inst.device.vma_alloc;
    
    // Pool name doesn't need to stay valid - vma keeps a local copy and copies this passed in str
    img_cfg.vma_alloc_name = cfg.pool_name;

    int err = vkr_init_image(&pool->image, img_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_cleanup_staging_buffers(pool);
        terminate_slot_pool(&pool->tpool);
        return false;
    }

    vkr_image_view_cfg view_cfg{};
    view_cfg.image = &pool->image;
    view_cfg.view_type = is_cubemap ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view_cfg.srange.aspectMask = get_vk_aspect_flags(img_cfg.format);
    view_cfg.srange.baseMipLevel = 0;
    view_cfg.srange.levelCount = img_cfg.mip_levels;
    view_cfg.srange.baseArrayLayer = 0;
    view_cfg.srange.layerCount = img_cfg.array_layers;
    err = vkr_init_image_view(&pool->view, view_cfg, pool->vk);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_terminate_image(&pool->image, pool->vk);
        vkr_cleanup_staging_buffers(pool);
        terminate_slot_pool(&pool->tpool);
        return false;
    }
    return true;
}

void vkr_cleanup_staging_buffers(vkr_texture_pool *pool)
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
    ilog("Terminating texture pool %s with %u of %u used", pool->image.mem_info.pName, get_slot_used_count(pool->tpool), pool->tpool.slots.size);
    vkr_cleanup_staging_buffers(pool);
    vkr_terminate_image_view(pool->view, pool->vk);
    vkr_terminate_image(&pool->image, pool->vk);
    terminate_slot_pool(&pool->tpool);
    arr_terminate(&pool->pending_staging_buffers);
    (*pool) = {};
}

void vkr_transition_texture_layouts(vkr_texture_pool *pool,
                                    VkCommandBuffer cmd_buf,
                                    vkr_texture_pool_layout intent,
                                    const rtexture_pool_item_ref *tslots,
                                    u32 tslot_count)
{
    asrt(tslots && tslot_count > 0);
    array<pool_slot_range> ranges{};
    arr_init(&ranges, pool->scratch_stack);
    build_contiguous_slot_ranges(tslots, tslot_count, &ranges);
    transition_ranges_to_intent(pool, cmd_buf, ranges.data, ranges.size, intent);
    arr_terminate(&ranges);
}

void vkr_transition_pool_layout(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent)
{
    pool_slot_range range{.base_ind = 0, .count = (u32)pool->tpool.slots.size};
    transition_ranges_to_intent(pool, cmd_buf, &range, 1, intent);
}

b32 vkr_upload_to_texture_slots(vkr_texture_pool *pool,
                                VkCommandBuffer cmd_buf,
                                const vkr_source_image_data *src_images,
                                const rtexture_pool_item_ref *tslots,
                                u32 count)
{
    asrt(pool);
    asrt(count > 0);
    asrt(src_images);
    asrt(tslots);

    vk_format_info fmt_info = get_vk_format_info(pool->tmeta.fmt);
    vkr_buffer_cfg staging_cfg{};
    staging_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    staging_cfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    staging_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    staging_cfg.vma_alloc = &pool->vk->inst.device.vma_alloc;
    staging_cfg.vma_alloc_name = "staging-buffer";
    staging_cfg.buffer_size =
        calculate_vk_image_buffer_size(fmt_info, pool->tmeta.dims.w, pool->tmeta.dims.h, pool->tmeta.mip_levels, get_layers_per_slot(*pool) * count);
    vkr_buffer staging{};
    s32 result = vkr_init_buffer(&staging, staging_cfg);
    if (result != err_code::VKR_NO_ERROR) {
        return false;
    }

    // Each image has its mips included with the data, bug the staged buffer needs to be formatted with mip level having
    // all images for that layer (ie im 1 mip 0, im 2 mip 0, im 3 mip 0, im 1 mip 1, im 2 mip 1, im 3 mip 1, etc) so
    // that we can only have one buffer image copy per mip level to do all images at that level at once
    for (u32 im_i = 0; im_i < count; ++im_i) {
        sizet src_offset = 0;
        sizet dest_offset = 0;
        for (u32 mipi = 0; mipi < pool->tmeta.mip_levels; ++mipi) {
            sizet mip_sz = calculate_vk_image_size(fmt_info, pool->tmeta.dims.w, pool->tmeta.dims.h, mipi, 1);
            auto src = (const void*)((sizet)src_images[im_i].data + src_offset);
            auto dest = (void*)((sizet)staging.mem_info.pMappedData + im_i * mip_sz + dest_offset);
            memcpy(dest, src, mip_sz);
            dest_offset += mip_sz * count;
            src_offset += mip_sz;
        }
        strncpy(tslots[im_i].item->name, src_images[im_i].name, SMALL_STR_LEN-1);
    }
    arr_push_back(&pool->pending_staging_buffers, staging);

    array<pool_slot_range> ranges;
    arr_init(&ranges, pool->scratch_stack);
    build_contiguous_slot_ranges(tslots, count, &ranges);

    transition_ranges_to_intent(pool, cmd_buf, ranges.data, ranges.size, VKR_TEXTURE_POOL_LAYOUT_TRANSFER_DST);

    // Because we moved the data to the staging buffer as all images per mip level we can now do image buffer copy per
    // mip level instead of per layer per mip level
    array<VkBufferImageCopy> regions{};
    arr_init(&regions, pool->scratch_stack);
    sizet buff_offset{0};
    for (u32 mipi = 0; mipi < pool->tmeta.mip_levels; ++mipi) {
        for (u32 rangei = 0; rangei < ranges.size; ++rangei) {
            VkBufferImageCopy region{};
            region.bufferOffset = buff_offset;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = mipi;
            region.imageSubresource.baseArrayLayer = get_layer_from_slot(*pool, ranges[rangei].base_ind);
            region.imageSubresource.layerCount = get_layers_per_slot(*pool) * ranges[rangei].count;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {pool->tmeta.dims.x, pool->tmeta.dims.y, 1};
            arr_push_back(&regions, region);
            buff_offset += calculate_vk_image_size(fmt_info, pool->tmeta.dims.w, pool->tmeta.dims.h, mipi, region.imageSubresource.layerCount);
        }
    }
    vkCmdCopyBufferToImage(cmd_buf, staging.hndl, pool->image.hndl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size, regions.data);
    arr_terminate(&regions);
    
    transition_ranges_to_intent(pool, cmd_buf, ranges.data, ranges.size, VKR_TEXTURE_POOL_LAYOUT_SHADER_READ);

    arr_terminate(&ranges);
    return true;
}

b32 vkr_acquire_texture_slots(vkr_texture_pool *pool, u32 src_image_count, rtexture_pool_item_ref *slots_out)
{
    asrt(pool);
    asrt(slots_out);
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

u32 vkr_release_texture_slots(vkr_texture_pool *pool, const rtexture_pool_handle *tslots, u32 tslot_count)
{
    asrt(pool);
    u32 cnt{0};
    for (u32 i = 0; i < tslot_count; ++i) {
        cnt += (u32)release_slot(&pool->tpool, tslots[i]);
    }
    return cnt;
}

} // namespace nslib
