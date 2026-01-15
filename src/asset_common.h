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
    asset_id id;                                                                                                                           \
    string name;                                                                                                                           \
    u64 flags;                                                                                                                             \
    mem_arena *arena;

#define PUP_ASSET                                                                                                                          \
    pup_member(id);                                                                                                                        \
    pup_member(flags)

template<class T>
struct asset_pool
{
    using asset_t = T;
    using iterator = hmap<asset_id, handle<T>>::iterator;
    using const_iterator = hmap<asset_id, handle<T>>::const_iterator;

    // This is the total cache arena memory
    mem_arena rarena{};

    // Resource id to pointer to resource obj
    hmap<asset_id, handle<T>> rmap{};
    // This is
    mem_arena rpool{};
    mem_arena rhandles{};
};

using mesh_pool = asset_pool<struct mesh>;
using material_pool = asset_pool<struct material>;
using texture_pool = asset_pool<struct texture>;

struct asset_cache
{
    array<void *> pools{};
};

// Initialize cache group with arena - all caches added to group will use this area
void init_cache(asset_cache *cache, mem_arena *arena);

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_default_types(asset_cache *cache, mem_arena *arena);

void terminate_cache(asset_cache *cache);

// Terminate all of the default robj types from the above enum and typedefs
void terminate_cache_default_types(asset_cache *cache);

// Initialize cache of type T with memory and item budget. If either go over we assert.
// The item budget determines approximately how much of the memory budget we use on the asset robj itself,
// the robj handles, and the id to handle map. The remainder is set aside for robj dynamic allocations (such
// as texture allocating pixels or mesh allocating verts).
template<class T>
void init_asset_pool(asset_pool<T> *pool, sizet memory_budget, u32 item_budget, mem_arena *upstream)
{
    mem_init_fl_arena(&pool->rarena, memory_budget, upstream, T::type_str);
    mem_init_pool_arena<T>(&pool->rpool, item_budget, &pool->rarena, T::type_str);
    mem_init_pool_arena<ref_counter>(&pool->rhandles, item_budget, &pool->rarena, T::type_str);
    hmap_init(&pool->rmap, hash_type, &pool->rarena, HMAP_DEFAULT_BUCKET_COUNT);
}

template<class T>
void terminate_asset_pool(asset_pool<T> *pool)
{
    hmap_terminate(&pool->rmap);
    // This will invalidate any handles we have for this cache
    asrt(pool->rpool.used == 0 && "Terminating cache with handles in use");
    asrt(pool->rhandles.used == 0 && "Terminating cache with handles in use");
    mem_terminate_arena(&pool->rpool);
    mem_terminate_arena(&pool->rhandles);
    asrt(pool->rarena.used == 0 && "Terminating cache with resource memory in use (leak)");
    mem_terminate_arena(&pool->rarena);
}

// Add and initialize a cache to the passed in cache group
template<class T>
asset_pool<T> *add_pool(sizet memory_budget, u32 item_budget, asset_cache *cache)
{
    if ((T::type_id + 1) > cache->pools.size) {
        arr_resize(&cache->pools, T::type_id + 1);
    }
    if (!cache->pools[T::type_id]) {
        auto pool = mem_calloc<asset_pool<T>>(1, cache->pools.arena);
        init_asset_pool(pool, memory_budget, item_budget, cache->pools.arena);
        cache->pools[T::type_id] = pool;
    }
    return (asset_pool<T> *)cache->pools[T::type_id];
}

template<class T>
asset_pool<T> *get_pool(const asset_cache *cache)
{
    if (T::type_id < cache->pools.size) {
        return (asset_pool<T> *)cache->pools[T::type_id];
    }
    return nullptr;
}

template<class T>
asset_pool<T>::iterator pool_begin(asset_pool<T> *pool)
{
    return hmap_begin(&pool->rmap);
}

template<class T>
asset_pool<T>::const_iterator pool_begin(const asset_pool<T> *pool)
{
    return hmap_begin(&pool->rmap);
}

template<class T>
asset_pool<T>::iterator pool_rbegin(asset_pool<T> *pool)
{
    return hmap_rbegin(&pool->rmap);
}

template<class T>
asset_pool<T>::const_iterator pool_rbegin(const asset_pool<T> *pool)
{
    return hmap_rbegin(&pool->rmap);
}

template<class T>
asset_pool<T>::iterator pool_next(asset_pool<T> *pool, typename asset_pool<T>::iterator iter)
{
    return hmap_next(&pool->rmap, iter);
}

template<class T>
asset_pool<T>::const_iterator pool_next(const asset_pool<T> *pool, typename asset_pool<T>::const_iterator iter)
{
    return hmap_next(&pool->rmap, iter);
}

template<class T>
asset_pool<T>::iterator pool_prev(asset_pool<T> *pool, typename asset_pool<T>::iterator iter)
{
    return hmap_prev(&pool->rmap, iter);
}

template<class T>
asset_pool<T>::const_iterator pool_prev(const asset_pool<T> *pool, typename asset_pool<T>::const_iterator iter)
{
    return hmap_prev(&pool->rmap, iter);
}

// Remove and terminates cache from the cache group - true on success or if the cache is not there false
template<class T>
bool remove_pool(asset_pool<T> *pool, asset_cache *cache)
{
    if (pool) {
        asrt(pool == cache->pools[T::type_id]);
        terminate_asset_pool(pool);
        mem_free(pool, cache->pools.arena);
        cache->pools[T::type_id] = {};
        return true;
    }
    return false;
}

// Remove and terminate a cache of type rtype from the cache group - true on success or if the cache is not there false
template<class T>
bool remove_pool(asset_cache *cache)
{
    auto pool = get_pool<T>(cache);
    return remove_pool(pool, cache);
}

template<class T>
handle<T> add_asset(asset_pool<T> *pool, handle_obj_terminate_func<T> *on_obj_terminated, const asset_id &id = generate_id())
{
    asrt(sizeof(T) == pool->rpool.mpool.chunk_size);
    T *ret = mem_calloc<T>(1, &pool->rpool);
    ret->id = id;
    auto hndl = make_handle(ret, on_obj_terminated, pool, &pool->rpool, &pool->rhandles);
    auto item = hmap_insert(&pool->rmap, id, hndl);
    if (item) {
        return item->val;
    }
    return {};
}

template<class T>
handle<T> add_asset(asset_pool<T> *pool, handle_obj_terminate_func<T> *on_obj_terminated, const T &copy, const asset_id &new_id = generate_id())
{
    auto cpy = add_asset<T>(pool, on_obj_terminated, new_id);
    if (cpy) {
        *cpy = copy;
        cpy->id = new_id;
    }
    return cpy;
}

template<class T>
handle<T> add_asset(asset_cache *cache, handle_obj_terminate_func<T> *on_obj_terminated, const T &copy, const asset_id &new_id = generate_id())
{
    auto pool = get_pool<T>(cache);
    return pool ? add_asset(pool, on_obj_terminated, copy, new_id) : handle<T>{};
}

template<class T>
handle<T> add_asset(asset_cache *cache, handle_obj_terminate_func<T> *on_obj_terminated, const asset_id &id = generate_id())
{
    auto pool = get_pool<T>(cache);
    return pool ? add_asset(pool, on_obj_terminated, id) : handle<T>{};
}

template<class T>
handle<T> get_asset(const asset_pool<T> *pool, const asset_id &id)
{
    auto item = hmap_find(&pool->rmap, id);
    if (item) {
        return item->val;
    }
    return {};
}

template<class T>
handle<T> get_asset(const asset_cache *cache, const asset_id &id)
{
    auto pool = get_pool<T>(cache);
    return get_asset(pool, id);
}

template<class T>
void init_asset(T *robj, const string &name, mem_arena *arena)
{
    robj->arena = arena;
    str_init(&robj->name, arena);
    robj->name = name;
}

template<class T>
void terminate_asset(T *robj)
{
    str_terminate(&robj->name);
}

template<class T>
bool remove_asset(asset_pool<T> *pool, const asset_id &id)
{
    return hmap_remove(id, &pool->rmam);
}

template<class T>
bool remove_asset(asset_pool<T> *pool, const T &item)
{
    return remove_asset(pool, item.id);
}

template<class T>
bool remove_asset(asset_cache *cache, const asset_id &id)
{
    auto pool = get_pool<T>(cache);
    return pool ? remove_asset(pool, id) : false;
}

template<class T>
bool remove_asset(asset_cache *cache, const T &item)
{
    auto pool = get_pool<T>(cache);
    return pool ? remove_asset(pool, item) : false;
}

} // namespace nslib
