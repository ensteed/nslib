#pragma once
#include "asset_id.h"

namespace nslib
{

template<typename T>
struct slot_handle;

template<typename T>
struct slot_item_ref;

enum asset_type : u32
{
    ASSET_TYPE_MESH,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_TECHNIQUE,
    ASSET_TYPE_SHADER,
    ASSET_TYPE_USER,
};

enum asset_flags : u32
{
    ASSET_FLAG_DIRTY = (1 << 0),
    ASSET_FLAG_LOADED = (1 << 1),
};

const sizet ASSET_POOL_MEMORY_BUDGETS[ASSET_TYPE_USER] = {
    1 * MB_SIZE,
    200 * MB_SIZE,
    1 * MB_SIZE,
    1 * MB_SIZE,
    100 * MB_SIZE,
};
const u32 ASSET_POOL_ITEM_BUDGETS[ASSET_TYPE_USER] = {
    256,
    20,
    256,
    128,
    128,
};

#define ASSET(type, ext)                                                                                                                   \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = ASSET_TYPE_##type;                                                                                \
    static constexpr const char *file_ext = "." #ext;                                                                                      \
    asset_id id;                                                                                                                           \
    string name;                                                                                                                           \
    u64 flags;                                                                                                                             \
    mem_arena *arena;

#define PUP_ASSET                                                                                                                          \
    pup_member(id);                                                                                                                        \
    pup_member(flags)

template<typename T>
using asset_handle = slot_handle<T>;

template<typename T>
using asset_item = slot_item_ref<T>;

template<typename T>
struct asset_ref
{
    union
    {
        asset_handle<T> hndl;
        asset_id id;
    };
};

template<typename T>
struct asset_pool;

struct mesh;
struct material;
struct texture;
using mesh_pool = asset_pool<mesh>;
using mesh_handle = asset_handle<mesh>;
using mesh_item = asset_item<mesh>;
using material_pool = asset_pool<material>;
using material_handle = asset_handle<material>;
using material_item = asset_item<material>;
using texture_pool = asset_pool<texture>;
using texture_handle = asset_handle<texture>;
using texture_item = asset_item<texture>;
} // namespace nslib
