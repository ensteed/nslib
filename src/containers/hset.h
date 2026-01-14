#pragma once
#include "hash_table.h"
#include "../util.h"
#include "../containers/string.h"
#include "../hashfuncs.h"

namespace nslib
{
constexpr inline sizet HSET_DEFAULT_BUCKET_COUNT = 16;
constexpr inline float HSET_DEFAULT_LOAD_FACTOR = HASH_TABLE_DEFAULT_LOAD_FACTOR;

enum hset_bucket_flags
{
    HSET_BUCKET_FLAG_USED = 1
};

template<typename Val>
struct hset_item
{
    using hash_key = Val;
    using key_type = Val;
    using mapped_type = Val;
    using iterator = const hset_item<Val> *;
    using const_iterator = const hset_item<Val> *;

    Val val{};
    sizet next{INVALID_IND};
    sizet prev{INVALID_IND};
};

template<typename Val>
using hset_bucket = hash_bucket<hset_item<Val>>;

template<class Val>
using hash_func = u64(const Val &, u64, u64);

// Since hset manages memory, but we want it to act like a built in type in terms of copying and equality testing, we
// have to write copy ctor, dtor, assignment operator, and equality operators.
template<typename Val>
using hset = hash_table<hset_item<Val>>;

template<typename Val>
bool hash_table_item_match(const hset_item<Val> &item, const Val &v)
{
    return item.val == v;
}

template<typename Val>
const Val &hash_table_item_key(const hset_item<Val> &item)
{
    return item.val;
}

template<typename Val>
const Val &hash_table_item_value(const hset_item<Val> &item)
{
    return item.val;
}

template<typename Val>
void set_hash_table_item_value(hset_item<Val> &item, const Val &v, const Val &)
{
    item.val = v;
}

template<typename Val>
void hset_print_internal(const array<hset_bucket<Val>> &buckets)
{
    for (sizet i = 0; i < buckets.size; ++i) {
        auto b = &buckets[i];
        dlog("Bucket: %lu  hval:%lu  prev:%lu  next:%lu  item [val:%s  prev:%lu  next:%lu]",
             i,
             b->hashed_v,
             b->prev,
             b->next,
             ls(b->item.val),
             b->item.prev,
             b->item.next);
    }
}

template<typename Val>
void hset_init(hset<Val> *hs,
               mem_arena *arena = mem_global_arena(),
               hash_func<Val> *hashf = hash_type,
               sizet initial_capacity = HSET_DEFAULT_BUCKET_COUNT)
{
    hash_table_init(hs, hashf, arena, initial_capacity, HSET_DEFAULT_LOAD_FACTOR);
}

template<typename Val>
void hset_rehash(hset<Val> *hs, sizet new_size)
{
    hash_table_rehash(hs, new_size);
}

template<typename Val>
float hset_load_factor(const hset<Val> *hs, sizet hs_entry_count)
{
    return hash_table_load_factor(hs, hs_entry_count);
}

template<typename Val>
float hset_current_load_factor(const hset<Val> *hs, sizet hs_entry_count)
{
    return hash_table_current_load_factor(hs);
}

// NOTE: erasing removes/compacts buckets and can invalidate other iterators/pointers.
template<typename Val>
hset<Val>::iterator hset_erase(hset<Val> *hs, typename hset<Val>::iterator item)
{
    return hash_table_erase(hs, item);
}

// Remove the entry for key k from the map.
// NOTE: erasing removes/compacts buckets and can invalidate other iterators/pointers.
template<typename Val>
bool hset_remove(hset<Val> *hs, const Val &v)
{
    sizet bckt_ind = hash_table_find_bucket(hs, v);
    if (bckt_ind != INVALID_IND) {
        hash_table_remove_bucket(hs, bckt_ind);
        return true;
    }
    return false;
}

template<typename Val>
hset<Val>::iterator hset_find(const hset<Val> *hs, const Val &v)
{
    sizet bucket_ind = hash_table_find_bucket(hs, v);
    if (bucket_ind != INVALID_IND) {
        return &hs->buckets[bucket_ind].item;
    }
    return nullptr;
}

// Insert a new item into the set. If the value already exists, return null. If it does not exist, insert it and
// return the inserted item.
template<typename Val>
hset<Val>::iterator hset_insert(hset<Val> *hs, const Val &val)
{
    return hash_table_insert_or_set(hs, val, val, false);
}

// Call hset_insert for all items in src on dest. Returns the number of new items inserted. If not_inserted is set,
// fills the array with vals from src that were not inserted in dest (most likely because they already existed)
template<typename Val>
sizet hset_insert(hset<Val> *dest, const hset<Val> *src, array<Val> *not_inserted = nullptr)
{
    sizet cnt{0};
    auto iter = hset_begin(src);
    while (iter) {
        auto ins = hset_insert(dest, iter->val);
        if (ins) {
            ++cnt;
        }
        else if (not_inserted) {
            arr_emplace_back(not_inserted, iter->val);
        }
        iter = hset_next(src, iter);
    }
    return cnt;
}

template<typename Val>
bool hset_empty(const hset<Val> *hs)
{
    return hash_table_empty(hs);
}

template<typename Val>
void hset_clear(hset<Val> *hs)
{
    hash_table_clear(hs);
}

template<typename Val>
hset<Val>::iterator hset_begin(const hset<Val> *hs)
{
    return hash_table_begin(hs);
}

template<typename Val>
hset<Val>::iterator hset_rbegin(const hset<Val> *hs)
{
    return hash_table_rbegin(hs);
}

template<typename Val>
hset<Val>::iterator hset_next(const hset<Val> *hs, typename hset<Val>::iterator item)
{
    return hash_table_next(hs, item);
}

template<typename Val>
hset<Val>::iterator hset_prev(const hset<Val> *hs, typename hset<Val>::iterator item)
{
    return hash_table_prev(hs, item);
}

template<typename Val>
void hset_terminate(hset<Val> *hs)
{
    hash_table_terminate(hs);
}

template<class ArchiveT, class T>
void pack_unpack(ArchiveT *ar, hset<T> &val, const pack_var_info &vinfo)
{
    sizet sz = val.count;
    pup_var(ar, sz, {"count"});
    sizet i{0};
    if (ar->opmode == archive_opmode::UNPACK) {
        while (i < sz) {
            T item{};
            pup_var(ar, item, {ls("{%d}", i)});
            hset_insert(&val, item);
            ++i;
        }
    }
    else {
        auto iter = hset_begin(&val);
        while (iter) {
            // We know we are packing in to the archive so we can just remove the constness
            pup_var(ar, const_cast<T &>(iter->val), {ls("{%d}", i)});
            iter = hset_next(&val, iter);
            ++i;
        }
    }
}

} // namespace nslib
