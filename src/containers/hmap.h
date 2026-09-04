#pragma once
#include "hash_table.h"
#include "../containers/string.h"
#include "../hashfuncs.h"

namespace nslib
{
constexpr inline sizet HMAP_DEFAULT_BUCKET_COUNT = 16;
constexpr inline float HMAP_DEFAULT_LOAD_FACTOR = HASH_TABLE_DEFAULT_LOAD_FACTOR;

template<typename Key, typename Val, auto HashF = hash_type_default<Key>>
struct hmap_item
{
    using hash_key = Key;
    using key_type = Key;
    using mapped_type = Val;
    using iterator = hmap_item<Key, Val, HashF> *;
    using const_iterator = const hmap_item<Key, Val, HashF> *;

    // Hash function the table uses for Key - a compile time constant so it inlines in to lookups
    static constexpr hash_func<Key> *hashf = HashF;

    // The key should be treated as const for users - it is a huge pain to mark it as const though so just don't change
    // it directly
    Key key{};
    // Val can be changed directly however
    Val val{};
};

// Because hmap uses arrays as its memory management, all of the default dtor/copy ctor, assignment operator, etc
// should work just fine
template<typename Key, typename Val, auto HashF = hash_type_default<Key>>
using hmap = hash_table<hmap_item<Key, Val, HashF>>;

template<typename Key, typename Val, auto HashF>
bool hash_table_item_match(const hmap_item<Key, Val, HashF> &item, const Key &k)
{
    return item.key == k;
}

template<typename Key, typename Val, auto HashF>
const Key &hash_table_item_key(const hmap_item<Key, Val, HashF> &item)
{
    return item.key;
}

template<typename Key, typename Val, auto HashF>
const Val &hash_table_item_value(const hmap_item<Key, Val, HashF> &item)
{
    return item.val;
}

template<typename Key, typename Val, auto HashF>
void set_hash_table_item_value(hmap_item<Key, Val, HashF> &item, const Key &k, const Val &val)
{
    item.key = k;
    item.val = val;
}

template<typename Key, typename Val, auto HashF>
void hmap_print_internal(const hmap<Key, Val, HashF> *hm)
{
    for (sizet i = 0; i < hm->items.size; ++i) {
        ilog("Item: %lu  key:%s  val:%s", i, ls(hm->items[i].key), ls(hm->items[i].val));
    }
    for (sizet i = 0; i < hm->slots.size; ++i) {
        ilog("Slot: %lu  dist:%u  fp:%u  idx:%u",
             i,
             hm->slots[i].dist_and_fp >> 8,
             hm->slots[i].dist_and_fp & HASH_SLOT_FP_MASK,
             hm->slots[i].idx);
    }
}

template<typename Key, typename Val, auto HashF>
void hmap_init(hmap<Key, Val, HashF> *hm,
               mem_arena *arena,
               sizet initial_capacity = HMAP_DEFAULT_BUCKET_COUNT)
{
    hash_table_init(hm, arena, initial_capacity, HMAP_DEFAULT_LOAD_FACTOR);
}

template<typename Key, typename Val, auto HashF>
void hmap_rehash(hmap<Key, Val, HashF> *hm, sizet new_size)
{
    hash_table_rehash(hm, new_size);
}

template<typename Key, typename Val, auto HashF>
float hmap_load_factor(const hmap<Key, Val, HashF> *hm, sizet hm_entry_count)
{
    return hash_table_load_factor(hm, hm_entry_count);
}

template<typename Key, typename Val, auto HashF>
float hmap_current_load_factor(const hmap<Key, Val, HashF> *hm, sizet hm_entry_count)
{
    return hash_table_current_load_factor(hm);
}

template<typename Key, typename Val, auto HashF>
sizet hmap_count(const hmap<Key, Val, HashF> *hm)
{
    return hash_table_count(hm);
}

// Erase item and return the item now occupying its position, or null if it was the last item. Erasing moves the last
// item in to the erased slot, so pointers to the erased item and to the last item are invalidated - all others stay
// valid. Iterating forward and continuing from the returned pointer visits every remaining item exactly once.
template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_erase(hmap<Key, Val, HashF> *hm, typename hmap<Key, Val, HashF>::iterator item)
{
    return hash_table_erase(hm, item);
}

// Remove the entry for key k from the map. If val is not null, fill it with the value of the item removed.
// NOTE: see hmap_erase for pointer invalidation.
template<typename Key, typename Val, auto HashF>
bool hmap_remove(hmap<Key, Val, HashF> *hm, const Key &k, Val *val = nullptr)
{
    sizet ind = hash_table_find_index(hm, k);
    if (ind != INVALID_ID) {
        if (val) {
            *val = hm->items[ind].val;
        }
        hash_table_erase_index(hm, ind);
        return true;
    }
    return false;
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_insert_or_set(hmap<Key, Val, HashF> *hm, const Key &k, const Val &val, bool set_if_exists)
{
    return hash_table_insert_or_set(hm, k, val, set_if_exists);
}

// Insert a new item into the map. If the key already exists, return null. If the key does not exist, insert it and
// return the inserted item
template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_insert(hmap<Key, Val, HashF> *hm, const Key &k, const Val &val)
{
    return hmap_insert_or_set(hm, k, val, false);
}

// Call hmap_insert for all items in src on dest. Returns the number of new items inserted. If not_inserted is set,
// fills the array with keys from src that were not inserted in dest (most likely because they already existed)
template<typename Key, typename Val, auto HashF>
sizet hmap_insert(hmap<Key, Val, HashF> *dest, const hmap<Key, Val, HashF> *src, array<Key> *not_inserted = nullptr)
{
    sizet cnt{0};
    auto iter = hmap_begin(src);
    while (iter) {
        auto ins = hmap_insert(dest, iter->key, iter->val);
        if (ins) {
            ++cnt;
        }
        else if (not_inserted) {
            arr_emplace_back(not_inserted, iter->key);
        }
        iter = hmap_next(src, iter);
    }
    return cnt;
}

// Insert a new item into the map. If the key already exists, set the value and return the item. If the key does not
// exist, create it. This may increase the hmap capacity and rehash if the new size is greater
template<typename Key, typename Val, auto HashF>
void hmap_set(hmap<Key, Val, HashF> *hm, const Key &k, const Val &val)
{
    auto result = hmap_insert_or_set(hm, k, val, true);
    asrt(result);
}

// Call hmap_set for all items in src on dest.
template<typename Key, typename Val, auto HashF>
void hmap_set(hmap<Key, Val, HashF> *dest, const hmap<Key, Val, HashF> *src)
{
    auto iter = hmap_begin(src);
    while (iter) {
        hmap_set(dest, iter->key, iter->val);
        iter = hmap_next(src, iter);
    }
}

// Find the item under key k and return it, if nothing is found then create a default constructed item and return it
template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_find_or_insert(hmap<Key, Val, HashF> *hm, const Key &k)
{
    auto found = hash_table_find(hm, k);
    if (found) {
        return found;
    }
    return hmap_insert(hm, k, {});
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_find(hmap<Key, Val, HashF> *hm, const Key &k)
{
    return hash_table_find(hm, k);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::const_iterator hmap_find(const hmap<Key, Val, HashF> *hm, const Key &k)
{
    return hash_table_find(hm, k);
}

template<typename Key, typename Val, auto HashF>
bool hmap_empty(const hmap<Key, Val, HashF> *hm)
{
    return hash_table_empty(hm);
}

template<typename Key, typename Val, auto HashF>
void hmap_clear(hmap<Key, Val, HashF> *hm)
{
    hash_table_clear(hm);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_begin(hmap<Key, Val, HashF> *hm)
{
    return hash_table_begin(hm);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::const_iterator hmap_begin(const hmap<Key, Val, HashF> *hm)
{
    return hash_table_begin(hm);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_rbegin(hmap<Key, Val, HashF> *hm)
{
    return hash_table_rbegin(hm);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::const_iterator hmap_rbegin(const hmap<Key, Val, HashF> *hm)
{
    return hash_table_rbegin(hm);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_next(hmap<Key, Val, HashF> *hm, typename hmap<Key, Val, HashF>::iterator item)
{
    return hash_table_next(hm, item);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::const_iterator hmap_next(const hmap<Key, Val, HashF> *hm, typename hmap<Key, Val, HashF>::const_iterator item)
{
    return hash_table_next(hm, item);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::iterator hmap_prev(hmap<Key, Val, HashF> *hm, typename hmap<Key, Val, HashF>::iterator item)
{
    return hash_table_prev(hm, item);
}

template<typename Key, typename Val, auto HashF>
hmap<Key, Val, HashF>::const_iterator hmap_prev(const hmap<Key, Val, HashF> *hm, typename hmap<Key, Val, HashF>::const_iterator item)
{
    return hash_table_prev(hm, item);
}

template<typename Key, typename Val, auto HashF>
void hmap_terminate(hmap<Key, Val, HashF> *hm)
{
    hash_table_terminate(hm);
}

template<class ArchiveT, class K, class T, auto HashF>
void pack_unpack(ArchiveT *ar, hmap<K, T, HashF> &val, const pack_var_info &vinfo)
{
    sizet sz = val.items.size;
    pup_var(ar, sz, {"count"});
    sizet i{0};
    if (ar->opmode == archive_opmode::UNPACK) {
        while (i < sz) {
            K item_key{};
            T item_val{};
            pup_var(ar, item_key, {ls("{%d}_key", i)});
            pup_var(ar, item_val, {ls("{%d}_val", i)});
            hmap_set(&val, item_key, item_val);
            ++i;
        }
    }
    else {
        auto iter = hmap_begin(&val);
        while (iter) {
            pup_var(ar, iter->key, {ls("{%d}_key", i)});
            pup_var(ar, iter->val, {ls("{%d}_val", i)});
            iter = hmap_next(&val, iter);
            ++i;
        }
    }
}

} // namespace nslib
