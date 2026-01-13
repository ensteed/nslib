#include <stdlib.h>
#include <cstring>

#include "logging.h"
#include "platform.h"
#include "memory.h"

#define DO_DEBUG_FL_ALLOC false
#define DO_DEBUG_LINEAR_ALLOC false
#define DO_DEBUG_STACK_ALLOC false

namespace nslib
{

intern mem_arena *g_fl_arena{};
intern mem_arena *g_stack_arena{};
intern mem_arena *g_frame_linear_arena{};

intern sizet calc_padding(sizet base_addr, sizet alignment)
{
    sizet multiplier = (base_addr / alignment) + 1;
    sizet aligned_addr = multiplier * alignment;
    sizet padding = aligned_addr - base_addr;
    return padding;
}

intern sizet calc_padding_with_header(sizet base_addr, sizet alignment, sizet header_size)
{
    sizet padding = calc_padding(base_addr, alignment);
    sizet needed_space = header_size;

    if (padding < needed_space) {
        // Header does not fit - Calculate next aligned address that header fits
        needed_space -= padding;

        // How many alignments I need to fit the header
        if (needed_space % alignment > 0) {
            padding += alignment * (1 + (needed_space / alignment));
        }
        else {
            padding += alignment * (needed_space / alignment);
        }
    }

    return padding;
}

intern b32 mul_overflow_sizet(const sizet &a, const sizet &b, sizet *out)
{
    if (a == 0 || b == 0) {
        *out = 0;
        return false;
    }

    sizet max_val = (sizet)-1;
    if (a > (max_val / b)) {
        return true;
    }

    *out = a * b;
    return false;
}

intern void find_first(mem_free_list *mfl, sizet size, sizet alignment, sizet *padding, mem_node **prev_node, mem_node **found_node)
{
    // Iterate list and return the first free block with a size >= than given size
    mem_node *it = mfl->free_list.head, *it_prev = nullptr;

    while (it != nullptr) {
        *padding = calc_padding_with_header((sizet)it, alignment, sizeof(alloc_header));
        sizet required_space = size + *padding;
        if (it->data.block_size >= required_space) {
            break;
        }
        it_prev = it;
        it = it->next;
    }
    *prev_node = it_prev;
    *found_node = it;
}

intern void find_best(mem_free_list *mfl, sizet size, sizet alignment, sizet *padding, mem_node **prev_node, mem_node **found_node)
{
    // Iterate WHOLE list keeping a pointer to the best fit
    sizet smallest_diff = std::numeric_limits<sizet>::max();
    mem_node *best_block = nullptr;
    mem_node *best_prev = nullptr;
    sizet best_padding = 0;
    mem_node *it = mfl->free_list.head, *it_prev = nullptr;
    while (it != nullptr) {
        sizet cur_padding = calc_padding_with_header((sizet)it, alignment, sizeof(alloc_header));
        sizet required_space = size + cur_padding;
        if (it->data.block_size >= required_space && (it->data.block_size - required_space < smallest_diff)) {
            best_block = it;
            best_prev = it_prev;
            best_padding = cur_padding;
            smallest_diff = it->data.block_size - required_space;
        }
        it_prev = it;
        it = it->next;
    }
    *prev_node = best_prev;
    *found_node = best_block;
    *padding = best_padding;
}

intern void find(mem_free_list *mfl, sizet size, sizet alignment, sizet *padding, mem_node **prev_node, mem_node **found_node)
{
    switch (mfl->p_policy) {
    case FIND_FIRST:
        find_first(mfl, size, alignment, padding, prev_node, found_node);
        break;
    case FIND_BEST:
        find_best(mfl, size, alignment, padding, prev_node, found_node);
        break;
    }
}

intern void coalescence(mem_free_list *mfl, mem_node *prev_node, mem_node *free_node)
{
    if (free_node->next != nullptr && (sizet)free_node + free_node->data.block_size == (sizet)free_node->next) {
        free_node->data.block_size += free_node->next->data.block_size;
        ll_remove(&mfl->free_list, free_node, free_node->next);
    }

    if (prev_node != nullptr && (sizet)prev_node + prev_node->data.block_size == (sizet)free_node) {
        prev_node->data.block_size += free_node->data.block_size;
        ll_remove(&mfl->free_list, prev_node, free_node);
    }
}

intern void *mem_free_list_alloc(mem_arena *arena, sizet size, sizet alignment_p)
{
    sizet alloc_header_size = sizeof(alloc_header);
    if (size < sizeof(mem_node)) {
        size = sizeof(mem_node);
    }
    sizet alignment(alignment_p);

    // Padding is the amount of padding we need considering the passed in alignment and the size of our header address
    sizet padding{};

    // Search through the free list for a free block that has enough space to allocate our data
    mem_node *affected_node{}, *prev_node{};
    find(&arena->mfl, size, alignment, &padding, &prev_node, &affected_node);
    asrt(affected_node && "Not enough memory");

    // The total required size for this block (including header and alignment paddnig which are both included in padding)
    sizet required_size = size + padding;

    // The amount of alignment needed for the returned data address to be aligned
    sizet alignment_padding = padding - alloc_header_size;

    // Now subtract our required block size from the chosen node block size - this is the remaining of the chunk that we
    // don't need
    sizet rest = affected_node->data.block_size - required_size;

    // Only split if the remainder can satisfy a minimal allocation (node + header with padding) - use default min
    // alignment as that is the smallest alignment that can be used with this allocator (and the user size must at least
    // be sizeof mem_node to be added to free list)
    sizet min_padding = calc_padding_with_header(
        (sizet)affected_node + required_size,
        DEFAULT_MIN_ALIGNMENT,
        sizeof(alloc_header));
    sizet min_required = sizeof(mem_node) + min_padding;
    
    if (rest >= min_required) {
        // We have to split the block into the data block and a free block of size 'rest'
        mem_node *new_free_node = (mem_node *)((sizet)affected_node + required_size);
        new_free_node->data.block_size = rest;
        ll_insert(&arena->mfl.free_list, affected_node, new_free_node);
    }
    else {
        required_size += rest;
        rest = 0;
    }

    ll_remove(&arena->mfl.free_list, prev_node, affected_node);

    //////////////////////////////////////////////////////////////////////////////////////////
    // The affected node is at the start of the block, and the block as allocated like this //
    // Alignment Padding | Header | Aligned Base Address                                    //
    //////////////////////////////////////////////////////////////////////////////////////////
    sizet header_addr = (sizet)affected_node + alignment_padding;
    sizet aligned_data_addr = (sizet)affected_node + padding;
    ((alloc_header *)header_addr)->block_size = required_size;
    ((alloc_header *)header_addr)->algn_padding = alignment_padding;

    arena->used += required_size;
    arena->peak = std::max(arena->peak, arena->used);

#if DO_DEBUG_FL_ALLOC
    dlog("Blck:%p Hdr:%p Dptr:%p RqstS:%lu RqrdS:%lu BlkSz:%lu AlgnPdng:%lu Pdng:%lu Mused:%lu Rest:%lu",
         (void *)affected_node,
         (void *)header_addr,
         (void *)aligned_data_addr,
         size,
         required_size,
         ((alloc_header *)header_addr)->block_size,
         alignment_padding,
         padding,
         arena->used,
         rest);
#endif
    return (void *)aligned_data_addr;
}

intern sizet mem_free_list_linear_block_size(void *ptr)
{
    // Insert it in a sorted position by the address number
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(alloc_header);
    auto aheader = (alloc_header *)header_addr;
    return aheader->block_size;
}

intern sizet mem_free_list_linear_block_user_size(void *ptr)
{
    // Insert it in a sorted position by the address number
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(alloc_header);
    auto aheader = (alloc_header *)header_addr;
    return aheader->block_size - (aheader->algn_padding + sizeof(alloc_header));
}

intern void mem_free_list_free(mem_arena *arena, void *ptr)
{
    // Insert it in a sorted position by the address number

    //////////////////////////////////////////////////////////////////////////////////////////
    // The affected node is at the start of the block, and the block as allocated like this //
    // Alignment Padding | Header | Aligned Base Address                                    //
    //////////////////////////////////////////////////////////////////////////////////////////
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(alloc_header);
    auto aheader = (alloc_header *)header_addr;

#if DO_DEBUG_FL_ALLOC
    sizet algn_padding = aheader->algn_padding;
#endif

    mem_node *free_node = (mem_node *)(header_addr - aheader->algn_padding);
    free_node->data.block_size = aheader->block_size;
    free_node->next = nullptr;

    // Start at the head of the free list, and keep going along the free list until we get to a node that has a higher
    // address than the node we are freeing - ie insert our free node in such a way so the lower mem addresses are used
    // up first
    mem_node *it = arena->mfl.free_list.head;
    mem_node *it_prev = nullptr;
    while (it != nullptr) {
        if (free_node < it) {
            ll_insert(&arena->mfl.free_list, it_prev, free_node);
            break;
        }
        it_prev = it;
        it = it->next;
    }
    if (it == nullptr) {
        ll_insert(&arena->mfl.free_list, it_prev, free_node);
    }

    arena->used -= free_node->data.block_size;
#if DO_DEBUG_FL_ALLOC
    sizet orig_sz = free_node->data.block_size - algn_padding - sizeof(alloc_header);
    dlog("Dptr:%p FHdr:%p AHdr:%p BlckS:%lu APdng:%lu Size:%lu Mused:%lu",
         ptr,
         (void *)free_node,
         (void *)aheader,
         free_node->data.block_size,
         algn_padding,
         orig_sz,
         arena->used);
#endif
    asrt(arena->used <= arena->total_size);

    // Merge contiguous nodes
    coalescence(&arena->mfl, it_prev, free_node);
}

intern void *mem_pool_alloc(mem_arena *arena)
{
    mem_node *free_pos = ll_pop_front(&arena->mpool.free_list);
    asrt(free_pos);
    arena->used += arena->mpool.chunk_size;
    arena->peak = std::max(arena->peak, arena->used);
    return (void *)free_pos;
}

intern sizet mem_pool_block_size(mem_arena *arena, void *ptr)
{
    return arena->mpool.chunk_size;
}

intern void mem_pool_free(mem_arena *mem, void *ptr)
{
    mem->used -= mem->mpool.chunk_size;
    ll_push_front(&mem->mpool.free_list, (mem_node *)ptr);
}

intern void *mem_stack_alloc(mem_arena *arena, sizet size, sizet alignment)
{
    sizet current_addr = (sizet)arena->start + arena->mstack.offset;
    sizet padding = calc_padding_with_header(current_addr, alignment, sizeof(stack_alloc_header));

    asrt((arena->mstack.offset + padding + size) <= arena->total_size);

    sizet next_addr = current_addr + padding;
    sizet header_addr = next_addr - sizeof(stack_alloc_header);
    auto hdr = (stack_alloc_header *)header_addr;
    hdr->padding = padding;
    hdr->block_size = padding + size;

    // Set the prev in the header so we can set the arena->mstack.prev when freeing this node
    hdr->prev = arena->mstack.prev;

    arena->mstack.offset += (padding + size);
    arena->used = arena->mstack.offset;
    arena->peak = std::max(arena->peak, arena->used);

#if DO_DEBUG_STACK_ALLOC
    dlog("ptr:%p rqst:%lu pdg:%lu blk:%lu used:%lu", (void *)next_addr, size, padding, padding + size, arena->used);
#endif
    arena->mstack.prev = (void *)next_addr;
    return arena->mstack.prev;
}

intern void mem_stack_free(mem_arena *arena, void *ptr)
{
    // Assert that we are freeing the stack in the correct order - the arena prev should match the ptr
    asrt(ptr == arena->mstack.prev);

    // Move offset back to clear address
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(stack_alloc_header);
    auto alloc_header = (stack_alloc_header *)header_addr;

#if DO_DEBUG_STACK_ALLOC
    sizet rqst_size = ((sizet)arena->start + arena->mstack.offset) - current_addr;
#endif
    // Set our arena prev to the block that preceded the block we are freeing - this is to make sure our stack allocs
    // and frees are in the correct order
    arena->mstack.prev = alloc_header->prev;

    arena->mstack.offset = current_addr - alloc_header->padding - (sizet)arena->start;
    arena->used = arena->mstack.offset;

#if DO_DEBUG_STACK_ALLOC
    dlog("ptr:%p rqst:%lu pdg:%lu blk:%lu used:%lu", ptr, rqst_size, alloc_header->padding, rqst_size + alloc_header->padding, arena->used);
#endif
}

intern void *mem_linear_alloc(mem_arena *arena, sizet size, sizet alignment)
{
    sizet header_size = sizeof(alloc_header);
    sizet padding = header_size;
    sizet block_addr = (sizet)arena->start + arena->mlin.offset;

    // Alignment is required. Find the next aligned memory address and update offset
    if ((alignment != 0) && (((block_addr + header_size) % alignment) != 0)) {
        padding = calc_padding_with_header(block_addr, alignment, header_size);
    }

    asrt(arena->mlin.offset + padding + size <= arena->total_size);

    // Setting up a block header is purely to make realloc work with a linear allocator
    auto alignment_padding = padding - header_size;
    auto hdr_address = block_addr + alignment_padding;
    auto hdr = (alloc_header *)hdr_address;
    hdr->algn_padding = alignment_padding;
    hdr->block_size = padding + size;

    arena->mlin.offset += padding + size;
    sizet next_addr = hdr_address + header_size;
    arena->used = arena->mlin.offset;
    arena->peak = std::max(arena->peak, arena->used);

#if DO_DEBUG_LINEAR_ALLOC
    dlog("Dptr:%p BlckS:%lu Mused:%lu", (void *)next_addr, padding + size, arena->used);
#endif
    return (void *)next_addr;
}

intern void mem_linear_free(mem_arena *, void *)
{
    // NO OP
}

void *mem_alloc(sizet bytes, mem_arena *arena, sizet alignment)
{
    if (bytes == 0) {
        return nullptr;
    }
    if (alignment < DEFAULT_MIN_ALIGNMENT) {
        alignment = DEFAULT_MIN_ALIGNMENT;
    }

    void *ret{nullptr};
    if (arena) {
        switch (arena->alloc_type) {
        case (mem_alloc_type::FREE_LIST):
            ret = mem_free_list_alloc(arena, bytes, alignment);
            break;
        case (mem_alloc_type::POOL):
            bytes = (bytes >= sizeof(mem_node)) ? bytes : sizeof(mem_node);
            asrt((bytes == arena->mpool.chunk_size) && "Requested byte size must match pool block size");
            ret = mem_pool_alloc(arena);
            break;
        case (mem_alloc_type::STACK):
            ret = mem_stack_alloc(arena, bytes, alignment);
            break;
        case (mem_alloc_type::LINEAR):
            ret = mem_linear_alloc(arena, bytes, alignment);
            break;
        }
    }
    else {
        ret = platform_alloc(bytes);
    }
    return ret;
}

void *mem_calloc(sizet nmemb, sizet memb, mem_arena *arena, sizet alignment)
{
    sizet bytes{};
    asrt(!mul_overflow_sizet(nmemb, memb, &bytes) && "Mult overflow");
    auto ret = mem_alloc(bytes, arena, alignment);
    if (ret) {
        memset(ret, 0, bytes);
    }
    return ret;
}

void *mem_realloc(void *ptr, sizet new_size, mem_arena *arena, sizet alignment, bool free_ptr_after_copy)
{
    if (arena) {
        // Create a new block and copy the mem to it from the old block (we use the lesser of the block sizes)
        auto new_block = mem_alloc(new_size, arena, alignment);
        if (ptr && new_block) {
            sizet old_block_size = mem_block_user_size(ptr, arena);
            sizet block_size{new_size};
            asrt(old_block_size > 0);

            // We only want to copy the lesser size of the blocks bytes
            if (new_size > old_block_size) {
                block_size = old_block_size;
            }

            memcpy(new_block, ptr, block_size);
        }
        if (free_ptr_after_copy) {
            mem_free(ptr, arena);
        }
        return new_block;
    }
    else {
        return platform_realloc(ptr, new_size);
    }
}

sizet mem_block_size(void *ptr, mem_arena *arena)
{
    if (arena->alloc_type == mem_alloc_type::FREE_LIST || arena->alloc_type == mem_alloc_type::LINEAR) {
        return mem_free_list_linear_block_size(ptr);
    }
    else if (arena->alloc_type == mem_alloc_type::POOL) {
        return mem_pool_block_size(arena, ptr);
    }
    else if (arena->alloc_type == mem_alloc_type::STACK) {
        auto header_addr = (sizet)ptr - sizeof(stack_alloc_header);
        auto header = (stack_alloc_header *)header_addr;
        return header->block_size;
    }
    return 0;
}

sizet mem_block_user_size(void *ptr, mem_arena *arena)
{
    if (arena->alloc_type == mem_alloc_type::FREE_LIST || arena->alloc_type == mem_alloc_type::LINEAR) {
        return mem_free_list_linear_block_user_size(ptr);
    }
    else if (arena->alloc_type == mem_alloc_type::POOL) {
        return mem_pool_block_size(arena, ptr);
    }
    else if (arena->alloc_type == mem_alloc_type::STACK) {
        auto header_addr = (sizet)ptr - sizeof(stack_alloc_header);
        auto header = (stack_alloc_header *)header_addr;
        return header->block_size - header->padding;
    }
    return 0;
}

void mem_free(void *ptr, mem_arena *arena)
{
    if (ptr && arena) {
        switch (arena->alloc_type) {
        case (mem_alloc_type::FREE_LIST):
            mem_free_list_free(arena, ptr);
            break;
        case (mem_alloc_type::POOL):
            mem_pool_free(arena, ptr);
            break;
        case (mem_alloc_type::STACK):
            mem_stack_free(arena, ptr);
            break;
        case (mem_alloc_type::LINEAR):
            mem_linear_free(arena, ptr);
            break;
        }
    }
    else if (ptr) {
        platform_free(ptr);
    }
}

void mem_reset_arena(mem_arena *arena)
{
    arena->used = 0;
    arena->peak = 0;

    switch (arena->alloc_type) {
    case (mem_alloc_type::POOL): {
        arena->mpool.free_list.head = nullptr;
        // Create a linked-list with all free positions
        sizet nchunks = arena->total_size / arena->mpool.chunk_size;
        for (sizet i = 0; i < nchunks; ++i) {
            sizet address = (sizet)arena->start + i * arena->mpool.chunk_size;
            ll_push_front(&arena->mpool.free_list, (mem_node *)address);
        }
    } break;
    case (mem_alloc_type::FREE_LIST): {
        mem_node *first_node = (mem_node *)arena->start;
        first_node->data.block_size = arena->total_size;
        first_node->next = nullptr;
        arena->mfl.free_list.head = nullptr;
        slnode<free_header> *dummy = nullptr;
        ll_insert(&arena->mfl.free_list, dummy, first_node);
        arena->alloc_type = mem_alloc_type::FREE_LIST;
    } break;
    case (mem_alloc_type::STACK): {
        arena->mstack.offset = 0;
        arena->mstack.prev = nullptr;
    } break;
    case (mem_alloc_type::LINEAR): {
        arena->mlin.offset = 0;
    } break;
    }
}

void mem_init_arena(mem_arena *arena, sizet total_size, mem_alloc_type mtype, mem_arena *upstream, const char *name)
{
    arena->total_size = total_size;
    arena->alloc_type = mtype;
    arena->upstream_allocator = upstream;
    arena->name = name;
    ilog("Initializing %s (%s) arena with %lu available", name, mem_arena_type_str(arena->alloc_type), arena->total_size);

    // Make sure user filled out a size before passsing in
    asrt(arena->total_size != 0);

    // If pool allocator total size must be multiple of chunk size, and chunk size must not be zero
    asrt(arena->alloc_type != mem_alloc_type::POOL ||
         (((arena->total_size % arena->mpool.chunk_size) == 0) && (arena->mpool.chunk_size >= DEFAULT_MIN_ALIGNMENT)));

    if (!arena->upstream_allocator) {
        arena->start = platform_alloc(arena->total_size);
    }
    else {
        arena->start = mem_alloc(arena->total_size, arena->upstream_allocator);
    }

    mem_reset_arena(arena);
}

void mem_init_fl_arena(mem_arena *arena, sizet total_size, mem_arena *upstream, const char *name)
{
    mem_init_arena(arena, total_size, mem_alloc_type::FREE_LIST, upstream, name);
}

void mem_init_stack_arena(mem_arena *arena, sizet total_size, mem_arena *upstream, const char *name)
{
    mem_init_arena(arena, total_size, mem_alloc_type::STACK, upstream, name);
}

void mem_init_lin_arena(mem_arena *arena, sizet total_size, mem_arena *upstream, const char *name)
{
    mem_init_arena(arena, total_size, mem_alloc_type::LINEAR, upstream, name);
}

void mem_init_pool_arena(mem_arena *arena, sizet chunk_size, sizet chunk_count, mem_arena *upstream, const char *name)
{
    auto min_sz = sizeof(mem_node);
    arena->mpool.chunk_size = chunk_size >= min_sz ? chunk_size : min_sz;
    mem_init_arena(arena, arena->mpool.chunk_size * chunk_count, mem_alloc_type::POOL, upstream, name);
}

void mem_terminate_arena(mem_arena *arena)
{
    ilog("Terminating %s (%s) arena with %lu used of %lu allocated and %lu peak",
         arena->name,
         mem_arena_type_str(arena->alloc_type),
         arena->used,
         arena->total_size,
         arena->peak);
    mem_reset_arena(arena);
    if (arena->upstream_allocator) {
        mem_free(arena->start, arena->upstream_allocator);
    }
    else {
        platform_free(arena->start);
    }
    arena->start = nullptr;
}

const char *mem_arena_type_str(mem_alloc_type atype)
{
    switch (atype) {
    case (mem_alloc_type::FREE_LIST):
        return "free list";
    case (mem_alloc_type::POOL):
        return "pool";
    case (mem_alloc_type::STACK):
        return "stack";
    case (mem_alloc_type::LINEAR):
        return "linear";
    default:
        return "unknown";
    }
}

mem_arena *mem_global_arena()
{
    return g_fl_arena;
}

void mem_set_global_arena(mem_arena *arena)
{
    if (arena) {
        asrt(arena->alloc_type == mem_alloc_type::FREE_LIST);
    }
    g_fl_arena = arena;
}

mem_arena *mem_global_stack_arena()
{
    return g_stack_arena;
}

void mem_set_global_stack_arena(mem_arena *arena)
{
    if (arena) {
        asrt(arena->alloc_type == mem_alloc_type::STACK);
    }
    g_stack_arena = arena;
}

mem_arena *mem_global_frame_lin_arena()
{
    return g_frame_linear_arena;
}

void mem_set_global_frame_lin_arena(mem_arena *arena)
{
    if (arena) {
        asrt(arena->alloc_type == mem_alloc_type::LINEAR);
    }
    g_frame_linear_arena = arena;
}

} // namespace nslib
