#pragma once

#include "containers/string.h"

namespace nslib
{
struct asset_id
{
    u64 id{0};
};

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

asset_id generate_id();

inline bool operator==(const asset_id &lhs, const asset_id &rhs)
{
    return lhs.id == rhs.id;
}

inline bool operator!=(const asset_id &lhs, const asset_id &rhs)
{
    return !(lhs == rhs);
}

} // namespace nslib
