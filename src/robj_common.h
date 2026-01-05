#pragma once

#include "archive_common.h"
#include "rid.h"
#include "handle.h"
#include "containers/hmap.h"

namespace nslib
{

enum robj_type : u32
{
    ROBJ_TYPE_MESH,
    ROBJ_TYPE_TEXTURE,
    ROBJ_TYPE_MATERIAL,
    ROBJ_TYPE_USER,
};

enum robj_flags : u32
{
    ROBJ_FLAG_DIRTY = (1 << 0)
};

const sizet ROBJ_TYPE_DEFAULT_BUDGET[ROBJ_TYPE_USER] = {256, 256, 256};

#define ROBJ(type)                                                                                                                         \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = ROBJ_TYPE_##type;                                                                                 \
    rid id;                                                                                                                                \
    string name;                                                                                                                           \
    u64 flags;                                                                                                                             \
    mem_arena *arena;

#define PUP_ROBJ                                                                                                                           \
    pup_member(id);                                                                                                                        \
    pup_member(flags);

template<class T>
struct robj_cache
{
    using robj_type = T;
    using iterator = hmap<rid, handle<T>>::iterator;
    using const_iterator = hmap<rid, handle<T>>::const_iterator;

    // Resource id to pointer to resource obj
    hmap<rid, handle<T>> rmap{};
    mem_arena arena{};
    mem_arena handle_arena{};
};

using mesh_cache = robj_cache<struct mesh>;
using material_cache = robj_cache<struct material>;

struct robj_cache_group
{
    array<void *> caches{};
};

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group(robj_cache_group *cg, mem_arena *arena);

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group_default_types(robj_cache_group *cg, mem_arena *arena);

void terminate_cache_group(robj_cache_group *cg);

// Terminate all of the default robj types from the above enum and typedefs
void terminate_cache_group_default_types(robj_cache_group *cg);

// Initialize cache of type rtype with a mem pool of total size item_budget * sizeof(item_size)
template<class T>
void init_cache(robj_cache<T> *cache, sizet item_budget, mem_arena *upstream)
{
    hmap_init(&cache->rmap, hash_type, upstream, HMAP_DEFAULT_BUCKET_COUNT);
    mem_init_pool_arena<T>(&cache->arena, item_budget, upstream, T::type_str);
    mem_init_pool_arena<ref_counter>(&cache->handle_arena, item_budget, upstream, T::type_str);
}

template<class T>
void terminate_cache(robj_cache<T> *cache)
{
    hmap_terminate(&cache->rmap);
    // This will invalidate any handles we have for this cache
    asrt(cache->arena.used == 0 && "Terminating cache with handles in use");
    asrt(cache->handle_arena.used == 0 && "Terminating cache with handles in use");
    mem_terminate_arena(&cache->arena);
    mem_terminate_arena(&cache->handle_arena);
}

// Add and initialize a cache to the passed in cache group
template<class T>
robj_cache<T> *add_cache(sizet item_budget, robj_cache_group *cg)
{
    if ((T::type_id + 1) > cg->caches.size) {
        arr_resize(&cg->caches, T::type_id + 1);
    }
    if (!cg->caches[T::type_id]) {
        auto cache = mem_calloc<robj_cache<T>>(1, cg->caches.arena);
        init_cache(cache, item_budget, cg->caches.arena);
        cg->caches[T::type_id] = cache;
    }
    return (robj_cache<T> *)cg->caches[T::type_id];
}

template<class T>
robj_cache<T> *get_cache(const robj_cache_group *cg)
{
    if (T::type_id < cg->caches.size) {
        return (robj_cache<T> *)cg->caches[T::type_id];
    }
    return nullptr;
}

template<class T>
robj_cache<T>::iterator cache_begin(robj_cache<T> *cache)
{
    return hmap_begin(&cache->rmap);
}

template<class T>
robj_cache<T>::const_iterator cache_begin(const robj_cache<T> *cache)
{
    return hmap_begin(&cache->rmap);
}

template<class T>
robj_cache<T>::iterator cache_rbegin(robj_cache<T> *cache)
{
    return hmap_rbegin(&cache->rmap);
}

template<class T>
robj_cache<T>::const_iterator cache_rbegin(const robj_cache<T> *cache)
{
    return hmap_rbegin(&cache->rmap);
}

template<class T>
robj_cache<T>::iterator cache_next(robj_cache<T> *cache, typename robj_cache<T>::iterator iter)
{
    return hmap_next(&cache->rmap, iter);
}

template<class T>
robj_cache<T>::const_iterator cache_next(const robj_cache<T> *cache, typename robj_cache<T>::const_iterator iter)
{
    return hmap_next(&cache->rmap, iter);
}

template<class T>
robj_cache<T>::iterator cache_prev(robj_cache<T> *cache, typename robj_cache<T>::iterator iter)
{
    return hmap_prev(&cache->rmap, iter);
}

template<class T>
robj_cache<T>::const_iterator cache_prev(const robj_cache<T> *cache, typename robj_cache<T>::const_iterator iter)
{
    return hmap_prev(&cache->rmap, iter);
}

// Remove and terminates cache from the cache group - true on success or if the cache is not there false
template<class T>
bool remove_cache(robj_cache<T> *cache, robj_cache_group *cg)
{
    if (cache) {
        asrt(cache == cg->caches[T::type_id]);
        terminate_cache(cache);
        mem_free(cache, cg->caches.arena);
        cg->caches[T::type_id] = {};
        return true;
    }
    return false;
}

// Remove and terminate a cache of type rtype from the cache group - true on success or if the cache is not there false
template<class T>
bool remove_cache(robj_cache_group *cg)
{
    auto cache = get_cache<T>(cg);
    return remove_cache(cache, cg);
}

template<class T>
handle<T> add_robj(robj_cache<T> *cache, handle_obj_terminate_func<T> *on_obj_terminated, const rid &id = generate_id())
{
    asrt(sizeof(T) == cache->arena.mpool.chunk_size);
    T *ret = mem_calloc<T>(1, &cache->arena);
    ret->id = id;
    auto hndl = make_handle(ret, on_obj_terminated, cache, &cache->arena, &cache->handle_arena);
    auto item = hmap_insert(&cache->rmap, id, hndl);
    if (item) {
        return item->val;
    }
    return {};
}

template<class T>
handle<T> add_robj(robj_cache<T> *cache, handle_obj_terminate_func<T> *on_obj_terminated, const T &copy, const rid &new_id = generate_id())
{
    auto cpy = add_robj<T>(cache, on_obj_terminated, new_id);
    if (cpy) {
        *cpy = copy;
        cpy->id = new_id;
    }
    return cpy;
}

template<class T>
handle<T> get_robj(const robj_cache<T> *cache, const rid &id)
{
    auto item = hmap_find(&cache->rmap, id);
    if (item) {
        return item->val;
    }
    return {};
}

template<class T>
handle<T> get_robj(const robj_cache_group *cg, const rid &id)
{
    auto cache = get_cache<T>(cg);
    return get_robj(cache, id);
}

template<class T>
void init_robj(T *robj, const string &name, mem_arena *arena) {
    robj->arena = arena;
    str_init(&robj->name, arena);
    robj->name = name;
}

template<class T>
void terminate_robj(T *robj) {
    str_terminate(&robj->name);
}

template<class T>
bool remove_robj(robj_cache<T> *cache, const rid &id)
{
    return hmap_remove(id, &cache->rmam);
}

template<class T>
bool remove_robj(const T &item, robj_cache<T> *cache)
{
    return remove_robj(cache, item.id);
}
} // namespace nslib
