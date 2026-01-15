#pragma once
#include "hash_table.h"
#include "../util.h"
#include "../containers/string.h"
#include "../hashfuncs.h"

namespace nslib
{
constexpr inline sizet HMAP_DEFAULT_BUCKET_COUNT = 16;
constexpr inline float HMAP_DEFAULT_LOAD_FACTOR = HASH_TABLE_DEFAULT_LOAD_FACTOR;

enum hmap_bucket_flags
{
    HMAP_BUCKET_FLAG_USED = 1
};

template<typename Key, typename Val>
struct hmap_item
{
    using hash_key = Key;
    using key_type = Key;
    using mapped_type = Val;
    using iterator = hmap_item<Key, Val> *;
    using const_iterator = const hmap_item<Key, Val> *;

    // The key should be treated as const for users - it is a huge pain to mark it as const though so just don't change
    // it directly
    Key key{};
    // Val can be changed directly however
    Val val{};
    // Mess with these outside of the hmap functions at your own risk - they are used for the linked list of items in
    // the hmap
    sizet next{INVALID_IND};
    sizet prev{INVALID_IND};
};

template<typename Key, typename Val>
using hmap_bucket = hash_bucket<hmap_item<Key, Val>>;

template<class Key>
using hash_func = u64(const Key &, u64, u64);

// Because hmap uses an array as it's memory management, all of the default dtor/copy ctor, assignment operator, etc
// should work just fine
template<typename Key, typename Val>
using hmap = hash_table<hmap_item<Key, Val>>;

template<typename Key, typename Val>
bool hash_table_item_match(const hmap_item<Key, Val> &item, const Key &k)
{
    return item.key == k;
}

template<typename Key, typename Val>
const Key &hash_table_item_key(const hmap_item<Key, Val> &item)
{
    return item.key;
}

template<typename Key, typename Val>
const Val &hash_table_item_value(const hmap_item<Key, Val> &item)
{
    return item.val;
}

template<typename Key, typename Val>
void set_hash_table_item_value(hmap_item<Key, Val> &item, const Key &k, const Val &val)
{
    item.key = k;
    item.val = val;
}

template<typename Key, typename Val>
void hmap_print_internal(const array<hmap_bucket<Key, Val>> &buckets)
{
    for (sizet i = 0; i < buckets.size; ++i) {
        auto b = &buckets[i];
        ilog("Bucket: %lu  hval:%lu  prev:%lu  next:%lu  item [key:%s  val:%s  prev:%lu  next:%lu]",
             i,
             b->hashed_v,
             b->prev,
             b->next,
             ls(b->item.key),
             ls(b->item.val),
             b->item.prev,
             b->item.next);
    }
}

template<typename Key, typename Val>
void hmap_init(hmap<Key, Val> *hm,
               hash_func<Key> *hashf = hash_type,
               mem_arena *arena = get_global_arena(),
               sizet initial_capacity = HMAP_DEFAULT_BUCKET_COUNT)
{
    hash_table_init(hm, hashf, arena, initial_capacity, HMAP_DEFAULT_LOAD_FACTOR);
}

template<typename Key, typename Val>
void hmap_rehash(hmap<Key, Val> *hm, sizet new_size)
{
    hash_table_rehash(hm, new_size);
}

template<typename Key, typename Val>
float hmap_load_factor(const hmap<Key, Val> *hm, sizet hm_entry_count)
{
    return hash_table_load_factor(hm, hm_entry_count);
}

template<typename Key, typename Val>
float hmap_current_load_factor(const hmap<Key, Val> *hm, sizet hm_entry_count)
{
    return hash_table_current_load_factor(hm);
}

// NOTE: erasing removes/compacts buckets and can invalidate other iterators/pointers.
template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_erase(hmap<Key, Val> *hm, typename hmap<Key, Val>::iterator item)
{
    return hash_table_erase(hm, item);
}

// Remove the entry for key k from the map. If val is not null, fill it with the value of the item removed.
// NOTE: erasing removes/compacts buckets and can invalidate other iterators/pointers.
template<typename Key, typename Val>
bool hmap_remove(hmap<Key, Val> *hm, const Key &k, Val *val = nullptr)
{
    sizet bckt_ind = hash_table_find_bucket(hm, k);
    if (bckt_ind != INVALID_IND) {
        if (val) {
            *val = hm->buckets[bckt_ind].item.val;
        }
        hash_table_remove_bucket(hm, bckt_ind);
        return true;
    }
    return false;
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_insert_or_set(hmap<Key, Val> *hm, const Key &k, const Val &val, bool set_if_exists)
{
    return hash_table_insert_or_set(hm, k, val, set_if_exists);
}

// Insert a new item into the map. If the key already exists, return null. If the key does not exist, insert it and
// return the inserted item
template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_insert(hmap<Key, Val> *hm, const Key &k, const Val &val)
{
    return hmap_insert_or_set(hm, k, val, false);
}

// Call hmap_insert for all items in src on dest. Returns the number of new items inserted. If not_inserted is set,
// fills the array with keys from src that were not inserted in dest (most likely because they already existed)
template<typename Key, typename Val>
sizet hmap_insert(hmap<Key, Val> *dest, const hmap<Key, Val> *src, array<Key> *not_inserted = nullptr)
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
template<typename Key, typename Val>
void hmap_set(hmap<Key, Val> *hm, const Key &k, const Val &val)
{
    auto result = hmap_insert_or_set(hm, k, val, true);
    asrt(result);
}

// Call hmap_set for all items in src on dest.
template<typename Key, typename Val>
void hmap_set(hmap<Key, Val> *dest, const hmap<Key, Val> *src)
{
    auto iter = hmap_begin(src);
    while (iter) {
        hmap_set(dest, iter->key, iter->val);
        iter = hmap_next(src, iter);
    }
}

// Find the item under key k and return it, if nothing is found then create a default constructed item and return it
template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_find_or_insert(hmap<Key, Val> *hm, const Key &k)
{
    sizet bucket_ind = hash_table_find_bucket(hm, k);
    if (bucket_ind != INVALID_IND) {
        return &hm->buckets[bucket_ind].item;
    }
    else {
        return hmap_insert(hm, k, {});
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_find(hmap<Key, Val> *hm, const Key &k)
{
    sizet bucket_ind = hash_table_find_bucket(hm, k);
    if (bucket_ind != INVALID_IND) {
        return &hm->buckets[bucket_ind].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_find(const hmap<Key, Val> *hm, const Key &k)
{
    sizet bucket_ind = hash_table_find_bucket(hm, k);
    if (bucket_ind != INVALID_IND) {
        return &hm->buckets[bucket_ind].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
bool hmap_empty(const hmap<Key, Val> *hm)
{
    return hash_table_empty(hm);
}

template<typename Key, typename Val>
void hmap_clear(hmap<Key, Val> *hm)
{
    hash_table_clear(hm);
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_begin(hmap<Key, Val> *hm)
{
    return hash_table_begin(hm);
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_begin(const hmap<Key, Val> *hm)
{
    return hash_table_begin(hm);
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_rbegin(hmap<Key, Val> *hm)
{
    return hash_table_rbegin(hm);
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_rbegin(const hmap<Key, Val> *hm)
{
    return hash_table_rbegin(hm);
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_next(hmap<Key, Val> *hm, typename hmap<Key, Val>::iterator item)
{
    return hash_table_next(hm, item);
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_next(const hmap<Key, Val> *hm, typename hmap<Key, Val>::const_iterator item)
{
    return hash_table_next(hm, item);
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_prev(hmap<Key, Val> *hm, typename hmap<Key, Val>::iterator item)
{
    return hash_table_prev(hm, item);
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_prev(const hmap<Key, Val> *hm, typename hmap<Key, Val>::const_iterator item)
{
    return hash_table_prev(hm, item);
}

template<typename Key, typename Val>
void hmap_terminate(hmap<Key, Val> *hm)
{
    hash_table_terminate(hm);
}

template<class ArchiveT, class K, class T>
void pack_unpack(ArchiveT *ar, hmap<K, T> &val, const pack_var_info &vinfo)
{
    sizet sz = val.count;
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
