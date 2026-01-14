#pragma once

#include "containers/string.h"

namespace nslib
{
struct aid
{
    string str;
    u64 id{0};
};

void set_aid(aid *id, const string &str);
void set_aid(aid *id, const char *str);

aid make_aid(const string &str);
aid make_aid(const char *str);

inline bool is_valid(const aid &id)
{
    return id.id != 0;
}

pup_func(aid)
{
    pup_member_info(str, vinfo);
    if (ar->opmode == archive_opmode::UNPACK) {
        val.id = hash_type(val.str, 0, 0);
    }
}

const string &to_str(const aid &rid);

inline u64 hash_type(const aid &id, u64, u64)
{
    return id.id;
}

aid generate_id();

inline bool operator==(const aid &lhs, const aid &rhs)
{
    return lhs.id == rhs.id;
}

inline bool operator!=(const aid &lhs, const aid &rhs)
{
    return !(lhs == rhs);
}

} // namespace nslib
