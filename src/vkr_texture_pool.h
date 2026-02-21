#pragma once

#include "containers/slot_pool.h"

#include "vkr_context.h"

namespace nslib
{

namespace err_code
{
enum vkr_texture_pool
{
    VKR_TEXTURE_POOL_NO_ERROR,
    VKR_TEXTURE_POOL_INIT_IMAGE_FAIL,
    VKR_TEXTURE_POOL_INIT_IMAGE_VIEW_FAIL,
    VKR_TEXTURE_POOL_INVALID_RANGE,
    VKR_TEXTURE_POOL_INVALID_SOURCE_IMAGE,
    VKR_TEXTURE_POOL_OUT_OF_SLOTS,
    VKR_TEXTURE_POOL_UNSUPPORTED_LAYOUT,
    VKR_TEXTURE_POOL_STAGE_BUFFER_FAIL,
};
}

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
    const char *staging_buffer_name{"texture_pool_staging"};
    mem_arena *arena{nullptr};
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

struct vkr_texture_pool
{
    vkr_image image{};
    VkImageView view{VK_NULL_HANDLE};

    uvec2 dims{};
    VkFormat format{VK_FORMAT_UNDEFINED};
    vkr_texture_pool_type type{VKR_TEXTURE_POOL_TYPE_2D_ARRAY};
    slot_pool<vkr_texture_item> tpool{};

    // These buffers back pending transfer commands and must stay alive until the caller
    // has finished submitting and waiting on the command buffer using this pool.
    array<vkr_buffer> pending_staging_buffers{};

    const vkr_context *vk{nullptr};
};

b32 vkr_init_texture_pool(vkr_texture_pool *pool, const vkr_texture_pool_cfg &cfg, const vkr_context *vk);
void vkr_terminate_texture_pool(vkr_texture_pool *pool);

void vkr_texture_pool_cleanup_staging_buffers(vkr_texture_pool *pool);

int vkr_transition_pool_layout(vkr_texture_pool *pool,
                               VkCommandBuffer cmd_buf,
                               vkr_texture_pool_layout intent,
                               const texture_pool_handle *handles,
                               u32 handle_count);

int vkr_transition_pool_layout(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent);
int vkr_transition_pool_layout_layers(vkr_texture_pool *pool,
                                      VkCommandBuffer cmd_buf,
                                      vkr_texture_pool_layout intent,
                                      u32 base_layer,
                                      u32 layer_count);
int vkr_transition_pool_layout_slot(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, vkr_texture_pool_layout intent, u32 slot_index);

bool vkr_pool_has_available_slots(const vkr_texture_pool *pool, u32 slot_count);

int vkr_acquire_slots(vkr_texture_pool *pool,
                      VkCommandBuffer cmd_buf,
                      const vkr_texture_source_image *src_images,
                      u32 src_image_count,
                      u32 slot_count,
                      u32 *out_slots);

int vkr_texture_pool_acquire_slot(vkr_texture_pool *pool, VkCommandBuffer cmd_buf, const vkr_texture_source_image *src_image, u32 *out_slot);

void vkr_texture_pool_release_slot(vkr_texture_pool *pool, u32 slot_index);
void vkr_texture_pool_release_slots(vkr_texture_pool *pool, const u32 *slot_indices, u32 slot_count);

} // namespace nslib
