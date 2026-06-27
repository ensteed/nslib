#pragma once
#include "archive_common.h"
namespace nslib
{

struct string;

// Opaque u64 resource id - used for assets, render resources, and anywhere a hashed/unique handle is needed
struct rid
{
    u64 id{0};
};

inline constexpr const rid INVALID_RID{0};

rid make_rid(const string &str);
rid make_rid(const char *str);

inline bool is_valid(const rid &id)
{
    return id.id != 0;
}

pup_func(rid)
{
    pup_member(id);
}

string to_str(const rid &id);

inline u64 hash_type(const rid &id, u64, u64)
{
    return id.id;
}

rid generate_rid();

op_eq_func(rid)
{
    return lhs.id == rhs.id;
}

op_neq_func(rid);
} // namespace nslib
