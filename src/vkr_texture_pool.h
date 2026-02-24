#pragma once

#include "containers/slot_pool.h"

#include "vkr_context.h"

namespace nslib
{

enum vkr_texture_pool_type
{
    VKR_TEXTURE_POOL_TYPE_2D_ARRAY,
    VKR_TEXTURE_POOL_TYPE_CUBE_ARRAY,
};

enum vkr_texture_pool_layout
{
    VKR_TEXTURE_POOL_LAYOUT_TRANSFER_DST,
    VKR_TEXTURE_POOL_LAYOUT_SHADER_READ,
};

struct vkr_texture_pool_cfg
{
    uvec2 dims{};
    VkFormat format{VK_FORMAT_UNDEFINED};
    u32 slot_count{};
    u32 mip_levels{1};
    vkr_texture_pool_type type{VKR_TEXTURE_POOL_TYPE_2D_ARRAY};
    VkImageUsageFlags image_usage{VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};
    const char *image_name{nullptr};
    mem_arena *arena{nullptr};
    mem_arena *frame_scratch{nullptr};
};

struct vkr_texture_source_image
{
    const void *data{};
    sizet data_size{};
};

struct vkr_texture_item
{
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

using texture_pool_handle = slot_handle<vkr_texture_item>;
using texture_slot_item_ref = slot_item_ref<vkr_texture_item>;

struct vkr_texture_pool
{
    vkr_image image{};
    VkImageView view{VK_NULL_HANDLE};

    uvec2 dims;
    u32 mip_levels;
    VkFormat format;
    vkr_texture_pool_type type;
    slot_pool<vkr_texture_item> tpool;

    // These buffers back pending transfer commands and must stay alive until the caller
    // has finished submitting and waiting on the command buffer using this pool.
    array<vkr_buffer> pending_staging_buffers;
    mem_arena *frame_scratch;
    const vkr_context *vk;
};

b32 vkr_init_texture_pool(vkr_texture_pool *pool, const vkr_texture_pool_cfg &cfg, const vkr_context *vk);
void vkr_terminate_texture_pool(vkr_texture_pool *pool);

void vkr_cleanup_staging_buffers(vkr_texture_pool *pool);

b32 vkr_transition_texture_layouts(vkr_texture_pool *pool,
                                   VkCommandBuffer cmd_buf,
                                   vkr_texture_pool_layout intent,
                                   const texture_slot_item_ref *tslots,
                                   u32 tslot_count);

b32 vkr_transition_pool_layout(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent);

b32 vkr_upload_to_texture_slots(vkr_texture_pool *pool,
                                VkCommandBuffer cmd_buf,
                                const void *src_image_data,
                                const texture_slot_item_ref *tslots,
                                u32 count);

// Handles out must be large enough to store a handle for each source image or there will be crashes/undefined behavior
b32 vkr_acquire_texture_slots(vkr_texture_pool *pool, u32 src_image_count, texture_slot_item_ref *slots_out);

// Returns the number of successful slots released. If a handle is no longer valid, the slot release will return false
u32 vkr_release_texture_slots(vkr_texture_pool *pool, const texture_pool_handle *tslots, u32 tslot_count);

} // namespace nslib
