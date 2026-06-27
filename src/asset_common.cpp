
#include <cstring>

#include "util.h"
#include "containers/string.h"
#include "asset_common.h"
#include "hashfuncs.h"

// robj type headers
#include "model.h"

namespace nslib
{

intern const sizet ASSET_TYPE_SIZE[ASSET_TYPE_USER] = {sizeof(geometry)};

rid make_rid(const string &str)
{
    return {.id = hash_type(str, 0, 0)};
}

rid make_rid(const char *str)
{
    return {.id = hash_type(str, 0, 0)};
}

string to_str(const rid &id)
{
    return to_str(id.id);
}

rid generate_rid()
{
    return {.id = generate_unique_id()};
}

void init_asset_cache(asset_cache *cache, sizet overall_mem_budget, const mem_arena_set &upstream, const char *cache_name)
{
    asrt(cache);
    asrt(cache->arena.total_size == 0 && cache->pools.size == 0);
    strncpy(cache->name, cache_name, SMALL_STR_LEN - 1);
    cache->name[SMALL_STR_LEN - 1] = 0;
    init_fl_arena(&cache->arena, overall_mem_budget, upstream.free_list, cache_name);
    cache->frame_linear = upstream.frame_linear;
    cache->stack = upstream.stack;
    arr_init(&cache->pools, &cache->arena, ASSET_TYPE_USER);
}

#define POOL_SIZE(type) sizeof(asset_pool<type>)
constexpr sizet get_asset_pool_sizes()
{
    return POOL_SIZE(geometry) + POOL_SIZE(texture) + POOL_SIZE(material) + POOL_SIZE(technique) + POOL_SIZE(shader);
}

intern sizet get_default_cache_budget(const sizet *mem_budgets)
{
    // Add in some overhead for each allocation - really sizeof alloc_header works to be enough - be we add some
    // alignment padding wiggle room in there too
    sizet alloc_overhead = (sizeof(alloc_header) + 32) * (ASSET_TYPE_USER * 2 + 1);
    sizet asset_pool_sizes = get_asset_pool_sizes();
    sizet ret{sizeof(sizet) * ASSET_TYPE_USER + alloc_overhead + asset_pool_sizes};

    // Add in cache budgets
    for (int i = 0; i < ASSET_TYPE_USER; ++i) {
        ret += mem_budgets[i];
    }
    return ret;
}

#define CREATE_POOL(type) create_asset_pool<type>(cache, mem_budgets[type::type_id], item_budgets[type::type_id])
#define DESTROY_POOL(type) destroy_asset_pool<type>(cache)

void init_asset_cache_default_types(asset_cache *cache,
                                    const char *name,
                                    const mem_arena_set &upstream,
                                    const u32 *item_budgets,
                                    const sizet *mem_budgets,
                                    sizet overall_mem_budget)
{
    asrt(cache);
    if (overall_mem_budget == 0) {
        overall_mem_budget = get_default_cache_budget(mem_budgets);
    }

    init_asset_cache(cache, overall_mem_budget, upstream, name);

    // NOTE: Manually update this on adding different asset types
    CREATE_POOL(geometry);
    CREATE_POOL(texture);
    CREATE_POOL(material);
    CREATE_POOL(technique);
    CREATE_POOL(shader);
}

void terminate_asset_cache_default_types(asset_cache *cache)
{
    asrt(cache);
    // NOTE: Manually update this on adding different asset types
    DESTROY_POOL(geometry);
    DESTROY_POOL(texture);
    DESTROY_POOL(material);
    DESTROY_POOL(technique);
    DESTROY_POOL(shader);
    terminate_asset_cache(cache);
}

void terminate_asset_cache(asset_cache *cache)
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
