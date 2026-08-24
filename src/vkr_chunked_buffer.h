#pragma once

#include "vkr_context.h"
#include "containers/array.h"

namespace nslib
{

struct vkr_chunked_buffer_cfg
{
    vkr_buffer_cfg buffer_cfg{};
    sizet chunk_size{};
    sizet section_count{};
    mem_arena *chunk_tracking_arena{nullptr};
};

struct vkr_chunked_buffer
{
    vkr_buffer buffer{};
    array<u32> free_chunks{};
    sizet chunk_size{};
    sizet chunk_count{};
    sizet section_count{};
    sizet used_chunk_count{};
    sizet next_chunk_index{};
};

sizet vkr_get_chunk_offset(const vkr_chunked_buffer *chunk_buf, idx_t chunk_index, idx_t section = 0);
void *vkr_get_chunk_ptr(const vkr_chunked_buffer *chunk_buf, idx_t chunk_index, idx_t section = 0);
VkDescriptorBufferInfo vkr_get_chunk_desc_info(const vkr_chunked_buffer *chunk_buf, idx_t chunk_index, idx_t section = 0, u64 range_override = 0);
b8 vkr_init_chunked_buffer(vkr_chunked_buffer *chunk_buf, const vkr_chunked_buffer_cfg &cfg);
void vkr_terminate_chunked_buffer(vkr_chunked_buffer *chunk_buf, vkr_context *vk);
idx_t vkr_acquire_chunk(vkr_chunked_buffer *chunk_buf);
void vkr_release_chunk(vkr_chunked_buffer *chunk_buf, idx_t chunk_index);


} // namespace nslib
