#pragma once

#include <new>
#include "basic_types.h"
#include "containers/linked_list.h"

namespace nslib
{

static constexpr inline const sizet DEFAULT_MIN_ALIGNMENT = 8;
static constexpr inline const sizet SIMD_MIN_ALIGNMENT = 16;

enum struct mem_alloc_type
{
    FREE_LIST,
    POOL,
    STACK,
    LINEAR
};

struct free_header
{
    sizet block_size{};
};

struct alloc_header
{
    sizet block_size;
    sizet algn_padding;
};

struct stack_alloc_header
{
    sizet padding;
    sizet block_size;
    void *prev;
};

enum placement_policy
{
    FIND_FIRST,
    FIND_BEST
};

using mem_node = slnode<free_header>;

struct mem_free_list
{
    placement_policy p_policy{FIND_FIRST};
    slist<free_header> free_list;
};

struct mem_pool
{
    sizet chunk_size;
    slist<free_header> free_list;
};

struct mem_stack
{
    sizet offset;
    void *prev;
};

struct mem_linear
{
    sizet offset;
};

using alloc_func = void *(sizet, void *);
using free_func = void(void *, void *);

struct pf_alloc_funcs
{
    alloc_func *alloc{};
    free_func *free{};
    void *user;
};

struct mem_arena
{
    /// Input parameter for alloc functions
    sizet total_size;

    /// Input parameter for which type of allocator we want
    mem_alloc_type alloc_type;

    /// If null, the allocator memory pool will be allocated with platform_alloc, otherwise the
    /// upstream_allocator's alloc will be used with ns_alloc (same is true for free). Once an allocator gets its memory,
    /// don't change this to something different as it will likely crash (cant free from an allocator different than allocated from)
    mem_arena *upstream_allocator{nullptr};

    // If upstream isn't set, and any of these functinos are not null, they will be used to do the allocation
    pf_alloc_funcs pf_funcs{};

    // Name to use in debug/etc applications
    const char *name{"default"};

    sizet used{0};
    sizet peak{0};
    void *start{nullptr};
    union
    {
        mem_free_list mfl;
        mem_pool mpool;
        mem_stack mstack;
        mem_linear mlin;
    };
};

// The size of the allocated block including padding and header
sizet mem_block_size(void *ptr, mem_arena *arena);

// The size of the allocated block the user requested and got back
sizet mem_block_user_size(void *ptr, mem_arena *arena);

void *mem_alloc(sizet size, mem_arena *arena, sizet alignment = DEFAULT_MIN_ALIGNMENT);

template<class T>
T *mem_alloc(mem_arena *arena, sizet n = 1)
{
    auto alignment = alignof(T);
    return (T *)mem_alloc(sizeof(T) * n, arena, (alignment > DEFAULT_MIN_ALIGNMENT) ? alignment : DEFAULT_MIN_ALIGNMENT);
}

void *mem_calloc(sizet nmemb, sizet memb, mem_arena *arena, sizet alignment = DEFAULT_MIN_ALIGNMENT);

template<class T>
T *mem_calloc(sizet nmemb, mem_arena *arena)
{
    auto alignment = alignof(T);
    return (T *)mem_calloc(nmemb, sizeof(T), arena, (alignment > DEFAULT_MIN_ALIGNMENT) ? alignment : DEFAULT_MIN_ALIGNMENT);
}

void *mem_realloc(void *ptr, sizet size, mem_arena *arena, sizet alignment = DEFAULT_MIN_ALIGNMENT, bool free_ptr_after_cpy = true);

template<class T>
T *mem_realloc(T *ptr, mem_arena *arena, bool free_ptr_after_cpy)
{
    auto alignment = alignof(T);
    return (T *)mem_realloc(
        ptr, sizeof(T), arena, (alignment > DEFAULT_MIN_ALIGNMENT) ? alignment : DEFAULT_MIN_ALIGNMENT, free_ptr_after_cpy);
}

void mem_free(void *item, mem_arena *arena);

template<class T, class... Args>
T *mem_new(mem_arena *arena, Args &&...args)
{
    T *item = mem_alloc<T>(arena);
    new (item) T(static_cast<Args &&>(args)...);
    return item;
}

template<class T>
void mem_delete(T *item, mem_arena *arena)
{
    item->~T();
    mem_free(item, arena);
}

// Reset the store without actually freeing the memory so it can be reused
void reset_arena(mem_arena *arena);
void init_arena(mem_arena *arena,
                sizet total_size,
                mem_alloc_type atype,
                mem_arena *upstream,
                const char *name,
                bool skip_log = false,
                const pf_alloc_funcs &pf_funcs = {});

void init_pool_arena(mem_arena *arena,
                     sizet chunk_size,
                     sizet chunk_count,
                     mem_arena *upstream,
                     const char *name,
                     bool skip_log = false,
                     const pf_alloc_funcs &pf_funcs = {});

template<class T>
void init_pool_arena(mem_arena *arena,
                     sizet chunk_count,
                     mem_arena *upstream,
                     const char *name,
                     bool skip_log = false,
                     const pf_alloc_funcs &pf_funcs = {})
{
    init_pool_arena(arena, sizeof(T), chunk_count, upstream, name, skip_log, pf_funcs);
}

void init_fl_arena(mem_arena *arena,
                   sizet total_size,
                   mem_arena *upstream,
                   const char *name,
                   bool skip_log = false,
                   const pf_alloc_funcs &pf_funcs = {});
void init_stack_arena(mem_arena *arena,
                      sizet total_size,
                      mem_arena *upstream,
                      const char *name,
                      bool skip_log = false,
                      const pf_alloc_funcs &pf_funcs = {});
void init_lin_arena(mem_arena *arena,
                    sizet total_size,
                    mem_arena *upstream,
                    const char *name,
                    bool skip_log = false,
                    const pf_alloc_funcs &pf_funcs = {});

void terminate_arena(mem_arena *arena, bool skip_log = false);
const char *arena_type_str(mem_alloc_type atype);

mem_arena *get_global_arena();

// This must be a free list arena
void set_global_arena(mem_arena *arena);

mem_arena *get_global_stack_arena();
void set_global_stack_arena(mem_arena *arena);

mem_arena *get_global_frame_lin_arena();
void set_global_frame_lin_arena(mem_arena *arena);

} // namespace nslib
