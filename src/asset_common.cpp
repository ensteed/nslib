#include <cstring>

#include "containers/string.h"
#include "asset_common.h"
#include "hashfuncs.h"

// robj type headers
#include "model.h"

namespace nslib
{

intern const sizet ASSET_TYPE_SIZE[ASSET_TYPE_USER] = {sizeof(mesh)};

void set_aid(aid *id, const string &str)
{
    id->str = str;
    id->id = hash_type(id->str, 0, 0);
}

void set_aid(aid *id, const char *str)
{
    id->str = str;
    id->id = hash_type(id->str, 0, 0);
}

aid make_aid(const string &str)
{
    return {.str{str}, .id = hash_type(str, 0, 0)};
}

aid make_aid(const char *str)
{
    return {.str{str}, .id = hash_type(str, 0, 0)};
}

const string &to_str(const aid &rid)
{
    return rid.str;
}

aid generate_id()
{
    // Generate in this format 774f0899-9666471a-b1f9
    aid ret{};
    u32 r1 = rand();
    u32 r2 = rand();
    u16 r3 = rand();
    str_printf(&ret.str, "%08x-%08x-%04x", r1, r2, r3);
    ret.id = hash_type(ret.str, 0, 0);
    return ret;
}

void init_cache_group(asset_cache_group *cg, mem_arena *arena)
{
    arr_init(&cg->caches, arena);
}

void init_cache_group_default_types(asset_cache_group *cg, mem_arena *arena)
{
    init_cache_group(cg, arena);

    // NOTE: Manually update this on adding differe resource types
    add_cache<mesh>(ASSET_TYPE_MEMORY_BUDGET[mesh::type_id], ASSET_TYPE_ITEM_BUDGET[mesh::type_id], cg);
    add_cache<texture>(ASSET_TYPE_MEMORY_BUDGET[texture::type_id], ASSET_TYPE_ITEM_BUDGET[texture::type_id], cg);
    add_cache<material>(ASSET_TYPE_MEMORY_BUDGET[material::type_id], ASSET_TYPE_ITEM_BUDGET[material::type_id], cg);
}

void terminate_cache_group_default_types(asset_cache_group *cg)
{

    // NOTE: Manually update this on adding differe resource types
    remove_cache<mesh>(cg);
    remove_cache<texture>(cg);
    remove_cache<material>(cg);
    terminate_cache_group(cg);
}

void terminate_cache_group(asset_cache_group *cg)
{
    for (int i = 0; i < cg->caches.size; ++i) {
        mem_free(cg->caches[i], cg->caches.arena);
        cg->caches[i] = {};
    }
    arr_terminate(&cg->caches);
}

} // namespace nslib
