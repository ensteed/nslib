#include <cstring>

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


asset_id generate_id()
{
    // Generate in this format 774f0899-9666471a-b1f9
    asset_id ret{};
    u32 r1 = rand();
    u32 r2 = rand();
    u16 r3 = rand();
    string st;
    str_printf(&st, "%08x-%08x-%04x", r1, r2, r3);
    ret.id = hash_type(st, 0, 0);
    return ret;
}

void init_cache(asset_cache *cg, mem_arena *arena)
{
    arr_init(&cg->pools, arena);
}

void init_cache_default_types(asset_cache *cg, mem_arena *arena)
{
    init_cache(cg, arena);

    // NOTE: Manually update this on adding differe resource types
    add_pool<mesh>(ASSET_TYPE_MEMORY_BUDGET[mesh::type_id], ASSET_TYPE_ITEM_BUDGET[mesh::type_id], cg);
    add_pool<texture>(ASSET_TYPE_MEMORY_BUDGET[texture::type_id], ASSET_TYPE_ITEM_BUDGET[texture::type_id], cg);
    add_pool<material>(ASSET_TYPE_MEMORY_BUDGET[material::type_id], ASSET_TYPE_ITEM_BUDGET[material::type_id], cg);
}

void terminate_cache_default_types(asset_cache *cg)
{

    // NOTE: Manually update this on adding differe resource types
    remove_pool<mesh>(cg);
    remove_pool<texture>(cg);
    remove_pool<material>(cg);
    terminate_cache(cg);
}

void terminate_cache(asset_cache *cg)
{
    for (int i = 0; i < cg->pools.size; ++i) {
        mem_free(cg->pools[i], cg->pools.arena);
        cg->pools[i] = {};
    }
    arr_terminate(&cg->pools);
}

} // namespace nslib
