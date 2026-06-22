#pragma once
#include "archive_common.h"
namespace nslib
{

struct string;
struct asset_id
{
    u64 id{0};
};

inline constexpr const asset_id INVALID_ASSET_ID{INVALID_ID};

asset_id make_asset_id(const string &str);
asset_id make_asset_id(const char *str);

inline bool is_valid(const asset_id &id)
{
    return id.id != 0;
}

pup_func(asset_id)
{
    pup_member(id);
}

string to_str(const asset_id &rid);

inline u64 hash_type(const asset_id &id, u64, u64)
{
    return id.id;
}

asset_id generate_asset_id();

op_eq_func(asset_id)
{
    return lhs.id == rhs.id;
}

op_neq_func(asset_id);
} // namespace nslib
