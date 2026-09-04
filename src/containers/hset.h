#pragma once
#include "hash_table.h"
#include "../util.h"
#include "../containers/string.h"
#include "../hashfuncs.h"

namespace nslib
{
constexpr inline sizet HSET_DEFAULT_BUCKET_COUNT = 16;
constexpr inline float HSET_DEFAULT_LOAD_FACTOR = HASH_TABLE_DEFAULT_LOAD_FACTOR;

template<typename Val, auto HashF = hash_type_default<Val>>
struct hset_item
{
    using hash_key = Val;
    using key_type = Val;
    using mapped_type = Val;
    using iterator = const hset_item<Val, HashF> *;
    using const_iterator = const hset_item<Val, HashF> *;

    // Hash function the table uses for Val - a compile time constant so it inlines in to lookups
    static constexpr hash_func<Val> *hashf = HashF;

    Val val{};
};

// Because hset uses arrays as its memory management, all of the default dtor/copy ctor, assignment operator, etc
// should work just fine
template<typename Val, auto HashF = hash_type_default<Val>>
using hset = hash_table<hset_item<Val, HashF>>;

template<typename Val, auto HashF>
bool hash_table_item_match(const hset_item<Val, HashF> &item, const Val &v)
{
    return item.val == v;
}

template<typename Val, auto HashF>
const Val &hash_table_item_key(const hset_item<Val, HashF> &item)
{
    return item.val;
}

template<typename Val, auto HashF>
const Val &hash_table_item_value(const hset_item<Val, HashF> &item)
{
    return item.val;
}

template<typename Val, auto HashF>
void set_hash_table_item_value(hset_item<Val, HashF> &item, const Val &v, const Val &)
{
    item.val = v;
}

template<typename Val, auto HashF>
void hset_print_internal(const hset<Val, HashF> *hs)
{
    for (sizet i = 0; i < hs->items.size; ++i) {
        dlog("Item: %lu  val:%s", i, ls(hs->items[i].val));
    }
    for (sizet i = 0; i < hs->slots.size; ++i) {
        dlog("Slot: %lu  dist:%u  fp:%u  idx:%u",
             i,
             hs->slots[i].dist_and_fp >> 8,
             hs->slots[i].dist_and_fp & HASH_SLOT_FP_MASK,
             hs->slots[i].idx);
    }
}

template<typename Val, auto HashF>
void hset_init(hset<Val, HashF> *hs, mem_arena *arena, sizet initial_capacity = HSET_DEFAULT_BUCKET_COUNT)
{
    hash_table_init(hs, arena, initial_capacity, HSET_DEFAULT_LOAD_FACTOR);
}

template<typename Val, auto HashF>
void hset_rehash(hset<Val, HashF> *hs, sizet new_size)
{
    hash_table_rehash(hs, new_size);
}

template<typename Val, auto HashF>
float hset_load_factor(const hset<Val, HashF> *hs, sizet hs_entry_count)
{
    return hash_table_load_factor(hs, hs_entry_count);
}

template<typename Val, auto HashF>
float hset_current_load_factor(const hset<Val, HashF> *hs, sizet hs_entry_count)
{
    return hash_table_current_load_factor(hs);
}

template<typename Val, auto HashF>
sizet hset_count(const hset<Val, HashF> *hs)
{
    return hash_table_count(hs);
}

// Erase item and return the item now occupying its position, or null if it was the last item. Erasing moves the last
// item in to the erased slot, so pointers to the erased item and to the last item are invalidated - all others stay
// valid. Iterating forward and continuing from the returned pointer visits every remaining item exactly once.
template<typename Val, auto HashF>
hset<Val, HashF>::iterator hset_erase(hset<Val, HashF> *hs, typename hset<Val, HashF>::iterator item)
{
    return hash_table_erase(hs, item);
}

// Remove the entry for value v from the set.
// NOTE: see hset_erase for pointer invalidation.
template<typename Val, auto HashF>
bool hset_remove(hset<Val, HashF> *hs, const Val &v)
{
    return hash_table_remove(hs, v);
}

template<typename Val, auto HashF>
hset<Val, HashF>::iterator hset_find(const hset<Val, HashF> *hs, const Val &v)
{
    return hash_table_find(hs, v);
}

// Insert a new item into the set. If the value already exists, return null. If it does not exist, insert it and
// return the inserted item.
template<typename Val, auto HashF>
hset<Val, HashF>::iterator hset_insert(hset<Val, HashF> *hs, const Val &val)
{
    return hash_table_insert_or_set(hs, val, val, false);
}

// Call hset_insert for all items in src on dest. Returns the number of new items inserted. If not_inserted is set,
// fills the array with vals from src that were not inserted in dest (most likely because they already existed)
template<typename Val, auto HashF>
sizet hset_insert(hset<Val, HashF> *dest, const hset<Val, HashF> *src, array<Val> *not_inserted = nullptr)
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

template<typename Val, auto HashF>
bool hset_empty(const hset<Val, HashF> *hs)
{
    return hash_table_empty(hs);
}

template<typename Val, auto HashF>
void hset_clear(hset<Val, HashF> *hs)
{
    hash_table_clear(hs);
}

template<typename Val, auto HashF>
hset<Val, HashF>::iterator hset_begin(const hset<Val, HashF> *hs)
{
    return hash_table_begin(hs);
}

template<typename Val, auto HashF>
hset<Val, HashF>::iterator hset_rbegin(const hset<Val, HashF> *hs)
{
    return hash_table_rbegin(hs);
}

template<typename Val, auto HashF>
hset<Val, HashF>::iterator hset_next(const hset<Val, HashF> *hs, typename hset<Val, HashF>::iterator item)
{
    return hash_table_next(hs, item);
}

template<typename Val, auto HashF>
hset<Val, HashF>::iterator hset_prev(const hset<Val, HashF> *hs, typename hset<Val, HashF>::iterator item)
{
    return hash_table_prev(hs, item);
}

template<typename Val, auto HashF>
void hset_terminate(hset<Val, HashF> *hs)
{
    hash_table_terminate(hs);
}

template<class ArchiveT, class T, auto HashF>
void pack_unpack(ArchiveT *ar, hset<T, HashF> &val, const pack_var_info &vinfo)
{
    sizet sz = val.items.size;
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
