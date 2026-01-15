#pragma once

#include "asset_defs.h"
#include "containers/slot_pool.h"
#include "containers/hmap.h"

namespace nslib
{

template<typename T>
struct asset_pool
{
    using asset_t = T;
    using iterator = asset_item<T>;
    using const_iterator = asset_item<const T>;

    // This is the total cache arena memory
    mem_arena rarena{};

    // Asset id to slot handle
    hmap<asset_id, asset_handle<T>> rmap{};
    slot_pool<T> assets{};
};

struct asset_cache
{
    small_str name{};
    // This contains the memory for all pools and should be large enough - will asrt if not
    mem_arena arena;
    array<void *> pools{};
};

// Initialize cache which will allocate a chunk of memory overall_mem_budget big for its arena using upstream passed in
// All pools added to cache will use the cache's arena as upstream, so overall mem budget must be large enough to fit
// all assets in all pools
void init_cache(asset_cache *cache, sizet overall_mem_budget, mem_arena *upstream, const char *name);

// Initialize asset pools for all asset types the engine knows about. If overall mem budget is 0, will use the budgets
// for each asset type. Both u32 and sizet budget pointers must be the same size as the number of default asset types
void init_cache_default_types(asset_cache *cache,
                              const char *name,
                              mem_arena *upstream,
                              const u32 *item_budgets = ASSET_POOL_ITEM_BUDGETS,
                              const sizet *mem_budgets = ASSET_POOL_MEMORY_BUDGETS,
                              sizet overall_mem_budget = 0);

void terminate_cache(asset_cache *cache);

// Terminate all of the default asset types from the above enum and typedefs
void terminate_cache_default_types(asset_cache *cache);

template<typename T>
asset_pool<T> *get_pool(const asset_cache *cache)
{
    asrt(cache);
    if (T::type_id < cache->pools.size) {
        return (asset_pool<T> *)cache->pools[T::type_id];
    }
    return nullptr;
}

template<typename T>
asset_pool<T>::iterator pool_begin(asset_pool<T> *pool)
{
    asrt(pool);
    return pool_next(pool, {.hndl{.index = (u32)-1}});
}

template<typename T>
asset_pool<T>::const_iterator pool_begin(const asset_pool<T> *pool)
{
    asrt(pool);
    return pool_next(pool, {.hndl{.index = (u32)-1}});
}

template<typename T>
asset_pool<T>::iterator pool_rbegin(asset_pool<T> *pool)
{
    asrt(pool);
    return pool_prev(pool, {.hndl{.index = (u32)pool->assets.slots.size}});
}

template<typename T>
asset_pool<T>::const_iterator pool_rbegin(const asset_pool<T> *pool)
{
    asrt(pool);
    return pool_prev(pool, {.hndl{.index = (u32)pool->assets.slots.size}});
}

template<typename T>
asset_pool<T>::iterator pool_next(asset_pool<T> *pool, typename asset_pool<T>::iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index + 1;
    while (ind < pool->assets.slots.size) {
        auto hndl = get_slot_current_handle(&pool->assets, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool->assets.slots[ind].item};
        }
        ++ind;
    }
    return {};
}

template<typename T>
asset_pool<T>::const_iterator pool_next(const asset_pool<T> *pool, typename asset_pool<T>::const_iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index + 1;
    while (ind < pool->assets.slots.size) {
        auto hndl = get_slot_current_handle(&pool->assets, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool->assets.slots[ind].item};
        }
        ++ind;
    }
    return {};
}

template<typename T>
asset_pool<T>::iterator pool_prev(asset_pool<T> *pool, typename asset_pool<T>::iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index - 1;
    // We utilize u32 wrapping here
    while (ind < pool->assets.slots.size) {
        auto hndl = get_slot_current_handle(&pool->assets, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool->assets.slots[ind].item};
        }
        --ind;
    }
    return {};
}

template<typename T>
asset_pool<T>::const_iterator pool_prev(const asset_pool<T> *pool, typename asset_pool<T>::const_iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index - 1;
    // We utilize u32 wrapping here
    while (ind < pool->assets.slots.size) {
        auto hndl = get_slot_current_handle(&pool->assets, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool->assets.slots[ind].item};
        }
        --ind;
    }
    return {};
}

template<typename T>
bool destroy_pool(asset_pool<T> *pool, asset_cache *cache)
{
    asrt(pool);
    asrt(pool == cache->pools[T::type_id]);
    terminate_asset_pool(pool);
    mem_free(pool, cache->pools.arena);
    cache->pools[T::type_id] = {};
    return true;
}

template<typename T>
bool destroy_pool(asset_cache *cache)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return destroy_pool(pool, cache);
}

template<typename T>
asset_item<T> create_asset(asset_pool<T> *pool, const char *name)
{
    asrt(pool);
    auto hndl = acquire_slot(&pool->assets);
    if (!is_valid(hndl)) {
        return {};
    }
    auto sl = get_slot_item(&pool->assets, hndl);
    asrt(sl);

    sl->arena = &pool->rarena;    
    sl->id = generate_asset_id();
    auto item = hmap_insert(&pool->rmap, sl->id, hndl);
    if (!item) {
        release_slot(&pool->assets, hndl);
        wlog("Generated id %lu had collision", sl->id.id);
        return {};
    }
    str_init(&sl->name, sl->arena);
    if (name) {
        str_copy(&sl->name, name);
    }
    init_asset(sl);
    return {.hndl = hndl, .item = sl};
}

template<typename T>
asset_item<T> create_asset(asset_pool<T> *pool, const T &copy, const char *new_asset_name)
{
    asrt(pool);
    auto cpy = create_asset(pool, nullptr);
    if (is_valid(cpy)) {
        asset_id gen_id = cpy.item->id;
        *cpy.item = copy;
        cpy.item->arena = &pool->rarena;
        cpy.item->id = gen_id;
        if (new_asset_name) {
            str_copy(&cpy.item->name, new_asset_name);
        }
    }
    return cpy;
}

template<typename T>
asset_item<T> create_asset(asset_cache *cache, const char *new_asset_name)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return pool ? create_asset(pool, new_asset_name) : asset_item<T>{};
}

template<typename T>
asset_item<T> create_asset(asset_cache *cache, const T &copy, const char *new_asset_name)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return pool ? create_asset(pool, copy, new_asset_name) : asset_item<T>{};
}

template<typename T>
T *get_asset(asset_pool<T> *pool, asset_handle<T> hndl)
{
    asrt(pool);
    return get_slot_item(&pool->assets, hndl);
}

template<typename T>
const T *get_asset(const asset_pool<T> *pool, asset_handle<T> hndl)
{
    asrt(pool);
    return get_slot_item(&pool->assets, hndl);
}

template<typename T>
T *get_asset(asset_cache *cache, asset_handle<T> hndl)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return pool ? get_asset(pool, hndl) : nullptr;
}

template<typename T>
const T *get_asset(const asset_cache *cache, asset_handle<T> hndl)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return pool ? get_asset(pool, hndl) : nullptr;
}

template<typename T>
asset_item<T> find_asset(asset_pool<T> *pool, asset_id id)
{
    asrt(pool);
    asset_item<T> ret{};
    auto item = hmap_find(&pool->rmap, id);
    if (item) {
        ret.hndl = item->val;
        ret.item = get_asset(pool, ret.hndl);
    }
    return ret;
}

template<typename T>
asset_item<const T> find_asset(const asset_pool<T> *pool, asset_id id)
{
    asrt(pool);
    asset_item<const T> ret{};
    auto item = hmap_find(&pool->rmap, id);
    if (item) {
        ret.hndl = item->val;
        ret.item = get_asset(pool, ret.hndl);
    }
    return ret;
}

template<typename T>
asset_item<T> find_asset(asset_cache *cache, asset_id id)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return pool ? find_asset(pool, id) : asset_item<T>{};
}

template<typename T>
asset_item<const T> find_asset(const asset_cache *cache, asset_id id)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return pool ? find_asset(pool, id) : asset_item<const T>{};
}

template<typename T>
bool destroy_asset(asset_pool<T> *pool, asset_handle<T> hndl)
{
    asrt(pool);
    auto sl = get_asset(pool, hndl);
    if (!sl) {
        return false;
    }

    str_terminate(&sl->name);
    terminate_asset(sl);

    asrt(hmap_remove(&pool->rmap, sl->id));
    asrt(release_slot(&pool->assets, hndl));
    return true;
}

template<typename T>
bool destroy_asset(asset_cache *cache, asset_handle<T> hndl)
{
    asrt(cache);
    auto pool = get_pool<T>(cache);
    return pool ? destroy_asset(pool, hndl) : false;
}

// Initialize cache of type T with memory and item budget. If either go over we assert.
// The item budget determines approximately how much of the memory budget we use on the asset meta itself,
// the asset handles, and the id to handle map. The remainder is set aside for asset dynamic allocations (such
// as texture allocating pixels or mesh allocating verts). When adding pool to a cache, this is automatically called
// passing in the cache's arena as upstream so the cache arena must be large enough to handle all pools
template<typename T>
void init_asset_pool(asset_pool<T> *pool, sizet memory_budget, u32 item_budget, mem_arena *upstream)
{
    asrt(pool);
    init_fl_arena(&pool->rarena, memory_budget, upstream, T::type_str);
    init_slot_pool(&pool->assets, item_budget, &pool->rarena);
    hmap_init(&pool->rmap, hash_type, &pool->rarena, HMAP_DEFAULT_BUCKET_COUNT);
}

template<typename T>
void terminate_asset_pool(asset_pool<T> *pool)
{
    asrt(pool);
    for (auto item = pool_begin(pool); is_valid(item); item = pool_next(pool, item)) {
        destroy_asset(pool, item.hndl);
    }
    asrt(pool->rmap.count == 0);
    hmap_terminate(&pool->rmap);
    // This will invalidate any handles we have for this cache
    terminate_slot_pool(&pool->assets);
    asrt(pool->rarena.used == 0 && "Terminating cache with resource memory in use (leak)");
    terminate_arena(&pool->rarena);
}

template<typename T>
asset_pool<T> *create_pool(asset_cache *cache, sizet memory_budget, u32 item_budget)
{
    asrt(cache);
    if ((T::type_id + 1) > cache->pools.size) {
        arr_resize(&cache->pools, T::type_id + 1);
    }
    // It's a bug if we add a pool that is already there.. don't want to allow that
    asrt(!cache->pools[T::type_id]);

    // Clear the pool mem when allocating
    auto pool = mem_calloc<asset_pool<T>>(1, &cache->arena);
    init_asset_pool(pool, memory_budget, item_budget, &cache->arena);
    cache->pools[T::type_id] = pool;

    return (asset_pool<T> *)cache->pools[T::type_id];
}

} // namespace nslib
