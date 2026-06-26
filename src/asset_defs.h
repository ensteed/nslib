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
    ASSET_TYPE_GEOMETRY,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_TECHNIQUE,
    ASSET_TYPE_SHADER,
    ASSET_TYPE_USER,
};

enum asset_bits : u32
{
    ASSET_DIRTY_BIT,
    ASSET_LOADED_BIT,
    ASSET_USER_BASE_BIT,
};
using asset_flags = u32;

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
    mem_arena *fl;                                                                                                                         \
    mem_arena *frame_lin;                                                                                                                  \
    mem_arena *stack;

#define PUP_ASSET                                                                                                                          \
    pup_member(id);                                                                                                                        \
    pup_member(flags)

template<typename T>
using asset_handle = slot_handle<T>;

template<typename T>
using asset_item_ref = slot_item_ref<T>;

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

struct geometry;
struct material;
struct texture;
struct technique;
struct shader;

using geometry_pool = asset_pool<geometry>;
using geometry_handle = asset_handle<geometry>;
using geometry_item_ref = asset_item_ref<geometry>;

using texture_pool = asset_pool<texture>;
using texture_handle = asset_handle<texture>;
using texture_item_ref = asset_item_ref<texture>;

using material_pool = asset_pool<material>;
using material_handle = asset_handle<material>;
using material_item_ref = asset_item_ref<material>;

using technique_pool = asset_pool<technique>;
using technique_handle = asset_handle<technique>;
using technique_item_ref = asset_item_ref<technique>;

using shader_pool = asset_pool<shader>;
using shader_handle = asset_handle<shader>;
using shader_item_ref = asset_item_ref<shader>;

} // namespace nslib
