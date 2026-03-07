#pragma once

#include "archive_common.h"

namespace nslib
{

struct version_info
{
    int major;
    int minor;
    int patch;
};
pup_func(version_info)
{
    pup_member(major);
    pup_member(minor);
    pup_member(patch);
}

template<class F, class S>
struct key_val_pair
{
    F key;
    S value;
};

template<class F, class S>
struct pair
{
    F first;
    S second;
};

u64 generate_rand_seed();
u64 generate_unique_id();
u16 get_username_hash();
inline size_t align_up(size_t block, size_t alignment)
{
    return ((block + alignment - 1) / alignment) * alignment;
}

pup_func_tt(key_val_pair)
{
    pup_member(key);
    pup_member(value);
}

pup_func_tt(pair)
{
    pup_member(first);
    pup_member(second);
}

} // namespace nslib
