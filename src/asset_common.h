#pragma once

#include "archive_common.h"
#include "rid.h"
#include "handle.h"
#include "containers/hmap.h"

namespace nslib
{

enum asset_type : u32
{
    ASSET_TYPE_MESH,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_USER,
};

enum asset_flags : u32
{
    ASSET_FLAG_DIRTY = (1 << 0)
};

const sizet ASSET_TYPE_MEMORY_BUDGET[ASSET_TYPE_USER] = {1 * MB_SIZE, 1 * MB_SIZE, 1 * MB_SIZE};
const sizet ASSET_TYPE_ITEM_BUDGET[ASSET_TYPE_USER] = {256, 256, 100};

#define ASSET(type)                                                                                                                        \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = ASSET_TYPE_##type;                                                                                \
    aid id;                                                                                                                                \
    string name;                                                                                                                           \
    u64 flags;                                                                                                                             \
    mem_arena *arena;

#define PUP_ASSET                                                                                                                          \
    pup_member(id);                                                                                                                        \
    pup_member(flags)

template<class T>
struct asset_cache
{
    using asset_t = T;
    using iterator = hmap<aid, handle<T>>::iterator;
    using const_iterator = hmap<aid, handle<T>>::const_iterator;

    // This is the total cache arena memory
    mem_arena rarena{};

    // Resource id to pointer to resource obj
    hmap<aid, handle<T>> rmap{};
    // This is
    mem_arena rpool{};
    mem_arena rhandles{};
};

using mesh_cache = asset_cache<struct mesh>;
using material_cache = asset_cache<struct material>;
using texture_cache = asset_cache<struct texture>;

struct asset_cache_group
{
    array<void *> caches{};
};

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group(asset_cache_group *cg, mem_arena *arena);

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group_default_types(asset_cache_group *cg, mem_arena *arena);

void terminate_cache_group(asset_cache_group *cg);

// Terminate all of the default robj types from the above enum and typedefs
void terminate_cache_group_default_types(asset_cache_group *cg);

// Initialize cache of type T with memory and item budget. If either go over we assert.
// The item budget determines approximately how much of the memory budget we use on the asset robj itself,
// the robj handles, and the id to handle map. The remainder is set aside for robj dynamic allocations (such
// as texture allocating pixels or mesh allocating verts).
template<class T>
void init_cache(asset_cache<T> *cache, sizet memory_budget, u32 item_budget, mem_arena *upstream)
{
    mem_init_fl_arena(&cache->rarena, memory_budget, upstream, T::type_str);
    mem_init_pool_arena<T>(&cache->rpool, item_budget, &cache->rarena, T::type_str);
    mem_init_pool_arena<ref_counter>(&cache->rhandles, item_budget, &cache->rarena, T::type_str);
    hmap_init(&cache->rmap, hash_type, &cache->rarena, HMAP_DEFAULT_BUCKET_COUNT);
}

template<class T>
void terminate_cache(asset_cache<T> *cache)
{
    hmap_terminate(&cache->rmap);
    // This will invalidate any handles we have for this cache
    asrt(cache->rpool.used == 0 && "Terminating cache with handles in use");
    asrt(cache->rhandles.used == 0 && "Terminating cache with handles in use");
    mem_terminate_arena(&cache->rpool);
    mem_terminate_arena(&cache->rhandles);
    asrt(cache->rarena.used == 0 && "Terminating cache with resource memory in use (leak)");
    mem_terminate_arena(&cache->rarena);
}

// Add and initialize a cache to the passed in cache group
template<class T>
asset_cache<T> *add_cache(sizet memory_budget, u32 item_budget, asset_cache_group *cg)
{
    if ((T::type_id + 1) > cg->caches.size) {
        arr_resize(&cg->caches, T::type_id + 1);
    }
    if (!cg->caches[T::type_id]) {
        auto cache = mem_calloc<asset_cache<T>>(1, cg->caches.arena);
        init_cache(cache, memory_budget, item_budget, cg->caches.arena);
        cg->caches[T::type_id] = cache;
    }
    return (asset_cache<T> *)cg->caches[T::type_id];
}

template<class T>
asset_cache<T> *get_cache(const asset_cache_group *cg)
{
    if (T::type_id < cg->caches.size) {
        return (asset_cache<T> *)cg->caches[T::type_id];
    }
    return nullptr;
}

template<class T>
asset_cache<T>::iterator cache_begin(asset_cache<T> *cache)
{
    return hmap_begin(&cache->rmap);
}

template<class T>
asset_cache<T>::const_iterator cache_begin(const asset_cache<T> *cache)
{
    return hmap_begin(&cache->rmap);
}

template<class T>
asset_cache<T>::iterator cache_rbegin(asset_cache<T> *cache)
{
    return hmap_rbegin(&cache->rmap);
}

template<class T>
asset_cache<T>::const_iterator cache_rbegin(const asset_cache<T> *cache)
{
    return hmap_rbegin(&cache->rmap);
}

template<class T>
asset_cache<T>::iterator cache_next(asset_cache<T> *cache, typename asset_cache<T>::iterator iter)
{
    return hmap_next(&cache->rmap, iter);
}

template<class T>
asset_cache<T>::const_iterator cache_next(const asset_cache<T> *cache, typename asset_cache<T>::const_iterator iter)
{
    return hmap_next(&cache->rmap, iter);
}

template<class T>
asset_cache<T>::iterator cache_prev(asset_cache<T> *cache, typename asset_cache<T>::iterator iter)
{
    return hmap_prev(&cache->rmap, iter);
}

template<class T>
asset_cache<T>::const_iterator cache_prev(const asset_cache<T> *cache, typename asset_cache<T>::const_iterator iter)
{
    return hmap_prev(&cache->rmap, iter);
}

// Remove and terminates cache from the cache group - true on success or if the cache is not there false
template<class T>
bool remove_cache(asset_cache<T> *cache, asset_cache_group *cg)
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
bool remove_cache(asset_cache_group *cg)
{
    auto cache = get_cache<T>(cg);
    return remove_cache(cache, cg);
}

template<class T>
handle<T> add_robj(asset_cache<T> *cache, handle_obj_terminate_func<T> *on_obj_terminated, const aid &id = generate_id())
{
    asrt(sizeof(T) == cache->rpool.mpool.chunk_size);
    T *ret = mem_calloc<T>(1, &cache->rpool);
    ret->id = id;
    auto hndl = make_handle(ret, on_obj_terminated, cache, &cache->rpool, &cache->rhandles);
    auto item = hmap_insert(&cache->rmap, id, hndl);
    if (item) {
        return item->val;
    }
    return {};
}

template<class T>
handle<T> add_robj(asset_cache<T> *cache, handle_obj_terminate_func<T> *on_obj_terminated, const T &copy, const aid &new_id = generate_id())
{
    auto cpy = add_robj<T>(cache, on_obj_terminated, new_id);
    if (cpy) {
        *cpy = copy;
        cpy->id = new_id;
    }
    return cpy;
}

template<class T>
handle<T> get_robj(const asset_cache<T> *cache, const aid &id)
{
    auto item = hmap_find(&cache->rmap, id);
    if (item) {
        return item->val;
    }
    return {};
}

template<class T>
handle<T> get_robj(const asset_cache_group *cg, const aid &id)
{
    auto cache = get_cache<T>(cg);
    return get_robj(cache, id);
}

template<class T>
void init_robj(T *robj, const string &name, mem_arena *arena)
{
    robj->arena = arena;
    str_init(&robj->name, arena);
    robj->name = name;
}

template<class T>
void terminate_robj(T *robj)
{
    str_terminate(&robj->name);
}

template<class T>
bool remove_robj(asset_cache<T> *cache, const aid &id)
{
    return hmap_remove(id, &cache->rmam);
}

template<class T>
bool remove_robj(const T &item, asset_cache<T> *cache)
{
    return remove_robj(cache, item.id);
}
} // namespace nslib
