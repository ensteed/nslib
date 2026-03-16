#include <climits>
#include "vkr_chunked_buffer.h"

namespace nslib
{

u64 vkr_get_chunk_offset(const vkr_chunked_buffer *chunk_buf, u32 chunk_index)
{
    asrt(chunk_buf);
    asrt(chunk_index < chunk_buf->chunk_count);
    return chunk_buf->chunk_size * (u64)chunk_index;
}

void *vkr_get_chunk_ptr(const vkr_chunked_buffer *chunk_buf, u32 chunk_index)
{
    asrt(chunk_buf);
    asrt(chunk_index < chunk_buf->chunk_count);
    asrt(chunk_buf->buffer.mem_info.pMappedData);
    auto base = (u8 *)chunk_buf->buffer.mem_info.pMappedData;
    return base + chunk_buf->chunk_size * chunk_index;
}

VkDescriptorBufferInfo vkr_get_chunk_desc_info(const vkr_chunked_buffer *chunk_buf, u32 chunk_index, u64 range_override)
{
    VkDescriptorBufferInfo info{};
    info.buffer = chunk_buf->buffer.hndl;
    info.offset = vkr_get_chunk_offset(chunk_buf, chunk_index);
    info.range = (range_override != 0) ? range_override : chunk_buf->chunk_size;
    return info;
}

b32 vkr_init_chunked_buffer(vkr_chunked_buffer *chunk_buf, const vkr_chunked_buffer_cfg &cfg)
{
    asrt(chunk_buf);
    asrt(chunk_buf->free_chunks.size == 0 && chunk_buf->free_chunks.capacity == 0);
    asrt(cfg.chunk_size && cfg.buffer_cfg.buffer_size && cfg.buffer_cfg.vma_alloc);
    asrt(cfg.buffer_cfg.buffer_size % cfg.chunk_size == 0);
    asrt(cfg.chunk_tracking_arena);

    sizet chunk_count = cfg.buffer_cfg.buffer_size / cfg.chunk_size;
    asrt(chunk_count != 0 && chunk_count < UINT_MAX);

    vkr_buffer_cfg buf_cfg = cfg.buffer_cfg;
    // We need this memory to be host visible and mapped no matter what - thats the whole point of this thing
    buf_cfg.alloc_flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    ilog("Creating %lu byte %s with %lu byte chunks (%lu slots)",
         cfg.buffer_cfg.buffer_size,
         cfg.buffer_cfg.vma_alloc_name,
         cfg.chunk_size,
         cfg.buffer_cfg.buffer_size / cfg.chunk_size);

    int err = vkr_init_buffer(&chunk_buf->buffer, buf_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return false;
    }

    // Mapping needs to have worked or this whole program is invalid
    asrt(chunk_buf->buffer.mem_info.pMappedData);

    chunk_buf->chunk_size = cfg.chunk_size;
    chunk_buf->chunk_count = chunk_count;
    chunk_buf->used_chunk_count = 0;
    chunk_buf->next_chunk_index = 0;

    arr_init(&chunk_buf->free_chunks, cfg.chunk_tracking_arena, chunk_buf->chunk_count);
    return true;
}

void vkr_terminate_chunked_buffer(vkr_chunked_buffer *chunk_buf, vkr_context *vk)
{
    ilog("Terminating chunked buffer %s (%lu used chunks, %lu chunk size, %lu total chunk count)",
         chunk_buf->buffer.mem_info.pName,
         chunk_buf->used_chunk_count,
         chunk_buf->chunk_size,
         chunk_buf->chunk_count);
    asrt(chunk_buf);
    asrt(vk);
    arr_terminate(&chunk_buf->free_chunks);
    vkr_terminate_buffer(&chunk_buf->buffer, vk);
    chunk_buf->chunk_count = 0;
    chunk_buf->chunk_size = 0;
    chunk_buf->used_chunk_count = 0;
    chunk_buf->next_chunk_index = 0;
}

u32 vkr_acquire_chunk(vkr_chunked_buffer *chunk_buf)
{
    asrt(chunk_buf);
    asrt(chunk_buf->buffer.mem_info.pMappedData);
    u32 ret{INVALID_IDX};

    // Use any available chunks from free list first
    if (chunk_buf->free_chunks.size > 0) {
        ret = *arr_back(&chunk_buf->free_chunks);
        arr_pop_back(&chunk_buf->free_chunks);
    }
    // Otherwise allocate a new chunk - assert within range
    else {
        asrt(chunk_buf->next_chunk_index < chunk_buf->chunk_count);
        ret = (u32)chunk_buf->next_chunk_index;
        ++chunk_buf->next_chunk_index;
    }

    ++chunk_buf->used_chunk_count;
    return ret;
}

void vkr_release_chunk(vkr_chunked_buffer *chunk_buf, u32 chunk_index)
{
    asrt(chunk_buf);
    asrt(chunk_index < chunk_buf->chunk_count);
    asrt(chunk_buf->used_chunk_count > 0);
    --chunk_buf->used_chunk_count;
    arr_push_back(&chunk_buf->free_chunks, chunk_index);
}

} // namespace nslib
