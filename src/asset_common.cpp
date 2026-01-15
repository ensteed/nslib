
#include <cstring>

#include "util.h"
#include "containers/string.h"
#include "asset_common.h"
#include "hashfuncs.h"

// robj type headers
#include "model.h"

namespace nslib
{

intern const sizet ASSET_TYPE_SIZE[ASSET_TYPE_USER] = {sizeof(mesh)};

asset_id make_asset_id(const string &str)
{
    return {.id = hash_type(str, 0, 0)};
}

asset_id make_asset_id(const char *str)
{
    return {.id = hash_type(str, 0, 0)};
}

string to_str(const asset_id &rid)
{
    return to_str(rid.id);
}

asset_id generate_asset_id()
{
    return {.id = generate_unique_id()};
}

void init_cache(asset_cache *cache, sizet overall_mem_budget, mem_arena *upstream, const char *cache_name)
{
    asrt(cache);
    asrt(cache->arena.total_size == 0 && cache->pools.size == 0);
    strncpy(cache->name, cache_name, SMALL_STR_LEN - 1);
    cache->name[SMALL_STR_LEN - 1] = 0;
    init_fl_arena(&cache->arena, overall_mem_budget, upstream, cache_name);
    arr_init(&cache->pools, &cache->arena, ASSET_TYPE_USER);
}

intern sizet get_default_cache_budget(const sizet* mem_budgets)
{
    // Add in some overhead for each allocation - really sizeof alloc_header works to be enough - be we add some
    // alignment padding wiggle room in there too
    sizet alloc_overhead = (sizeof(alloc_header) + 32) * (ASSET_TYPE_USER * 2 + 1);
    sizet asset_pool_sizes =
        sizeof(asset_pool<mesh>) + sizeof(asset_pool<texture>) + sizeof(asset_pool<material>);
    sizet ret{sizeof(sizet) * ASSET_TYPE_USER + alloc_overhead + asset_pool_sizes};
    
    // Add in cache budgets
    for (int i = 0; i < ASSET_TYPE_USER; ++i) {
        ret += mem_budgets[i];
    }
    return ret;
}

void init_cache_default_types(asset_cache *cache,
                              const char *name,
                              mem_arena *upstream,
                              const u32 *item_budgets,
                              const sizet *mem_budgets,
                              sizet overall_mem_budget)
{
    asrt(cache);
    if (overall_mem_budget == 0) {
        overall_mem_budget = get_default_cache_budget(mem_budgets);
    }
    
    init_cache(cache, overall_mem_budget, upstream, name);
    
    // NOTE: Manually update this on adding different asset types
    create_pool<mesh>(cache, ASSET_POOL_MEMORY_BUDGETS[mesh::type_id], ASSET_POOL_ITEM_BUDGETS[mesh::type_id]);
    create_pool<texture>(cache, ASSET_POOL_MEMORY_BUDGETS[texture::type_id], ASSET_POOL_ITEM_BUDGETS[texture::type_id]);
    create_pool<material>(cache, ASSET_POOL_MEMORY_BUDGETS[material::type_id], ASSET_POOL_ITEM_BUDGETS[material::type_id]);
}

void terminate_cache_default_types(asset_cache *cache)
{
    asrt(cache);
    // NOTE: Manually update this on adding different asset types
    destroy_pool<mesh>(cache);
    destroy_pool<texture>(cache);
    destroy_pool<material>(cache);
    terminate_cache(cache);
}

void terminate_cache(asset_cache *cache)
{
    asrt(cache);
    for (int i = 0; i < cache->pools.size; ++i) {
        mem_free(cache->pools[i], &cache->arena);
        cache->pools[i] = {};
    }
    arr_terminate(&cache->pools);
    terminate_arena(&cache->arena);
}

} // namespace nslib
