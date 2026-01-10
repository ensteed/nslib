#pragma once
#include "array.h"
#include "../util.h"
#include "../containers/string.h"
#include "../hashfuncs.h"

namespace nslib
{
constexpr inline sizet HMAP_DEFAULT_BUCKET_COUNT = 16;
constexpr inline float HMAP_DEFAULT_LOAD_FACTOR = 0.75f;

enum hmap_bucket_flags
{
    HMAP_BUCKET_FLAG_USED = 1
};

template<typename Key, typename Val>
struct hmap_item
{
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
struct hmap_bucket
{
    hmap_item<Key, Val> item{};
    u64 hashed_v{};
    sizet next{INVALID_IND};
    sizet prev{INVALID_IND};
};

template<class Key>
using hash_func = u64(const Key &, u64, u64);

// Because hmap uses an array as it's memory management, all of the default dtor/copy ctor, assignment operator, etc
// should work just fine
template<typename Key, typename Val>
struct hmap
{
    using mapped_type = Val;
    using key_type = Key;
    using value_type = hmap_item<Key, Val>;
    using iterator = hmap_item<Key, Val> *;
    using const_iterator = const hmap_item<Key, Val> *;

    hash_func<Key> *hashf{};
    u64 seed0{};
    u64 seed1{};
    // If this is set outside the range 0.0f to 1.0f auto rehashing on insert will not happen
    float load_factor{0.0f};
    sizet head{};
    sizet count{0};
    array<hmap_bucket<Key, Val>> buckets{};
};

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
             to_cstr(b->item.key),
             to_cstr(b->item.val),
             b->item.prev,
             b->item.next);
    }
}

template<typename Key, typename Val>
void hmap_init(hmap<Key, Val> *hm,
               hash_func<Key> *hashf = hash_type,
               mem_arena *arena = mem_global_arena(),
               sizet initial_capacity = HMAP_DEFAULT_BUCKET_COUNT)
{
    hm->hashf = hashf;
    hm->seed0 = generate_rand_seed();
    hm->seed1 = generate_rand_seed();
    hm->head = INVALID_IND;
    hm->load_factor = HMAP_DEFAULT_LOAD_FACTOR;
    hm->count = 0;
    arr_init(&hm->buckets, arena, initial_capacity);
    arr_resize(&hm->buckets, initial_capacity);
}

template<typename Key, typename Val>
void hmap_rehash(hmap<Key, Val> *hm, sizet new_size)
{
    // Make tmp copy of buckets and head index, then clear the hashmap, then resize the buckets to the new size
    auto tmp = hm->buckets;
    sizet ind = hm->head;
    hmap_clear(hm);
    arr_resize(&hm->buckets, new_size);
    // And finally insert all of the items in the tmp copy in to the new bucket array
    while (is_valid(ind)) {
        hmap_insert(hm, tmp[ind].item.key, tmp[ind].item.val);
        ind = tmp[ind].item.next;
    }
}

template<typename Key, typename Val>
float hmap_load_factor(const hmap<Key, Val> *hm, sizet hm_entry_count)
{
    return (float)hm_entry_count / (float)hm->buckets.size;
}

template<typename Key, typename Val>
float hmap_current_load_factor(const hmap<Key, Val> *hm, sizet hm_entry_count)
{
    return hmap_load_factor(hm, hm->count);
}

template<typename Key, typename Val>
bool hmap_should_rehash_on_insert(const hmap<Key, Val> *hm)
{
    if (hm->load_factor >= 0.0f && hm->load_factor <= 1.0f) {
        return hmap_load_factor(hm, hm->count + 1) > hm->load_factor;
    }
    return false;
}

template<typename Key, typename Val>
sizet hmap_find_bucket(const hmap<Key, Val> *hm, const Key &k)
{
    asrt(hm->hashf);
    if (hm->buckets.size == 0) {
        return INVALID_IND;
    }
    u64 hashval = hm->hashf(k, hm->seed0, hm->seed1);
    sizet fnd_bckt = INVALID_IND;
    sizet bckt_ind = hashval % hm->buckets.size;
    sizet cur_bckt_ind = bckt_ind;
    sizet i = 0;
    
    // Find the correct bucket first - hashed_v mod bucket count should give us bckt_ind if a match
    while (i < hm->buckets.size && is_valid(hm->buckets[cur_bckt_ind].prev) &&
           (bckt_ind != (hm->buckets[cur_bckt_ind].hashed_v % hm->buckets.size))) {
        ++i;
        cur_bckt_ind = (hashval + i) % hm->buckets.size;
    }

    // If we found a matching bucket
    if (i < hm->buckets.size && is_valid(hm->buckets[cur_bckt_ind].prev) &&
        (bckt_ind == (hm->buckets[cur_bckt_ind].hashed_v % hm->buckets.size))) {
        // Follow the bucket linked list to check for matches on any of the items in the bucket
        while (is_valid(cur_bckt_ind)) {
            if ((hashval != hm->buckets[cur_bckt_ind].hashed_v || k != hm->buckets[cur_bckt_ind].item.key)) {
                cur_bckt_ind = hm->buckets[cur_bckt_ind].next;
            }
            else {
                fnd_bckt = cur_bckt_ind;
                cur_bckt_ind = INVALID_IND;
            }
        }
    }
    return fnd_bckt;
}

template<typename Key, typename Val>
void hmap_copy_bucket(hmap<Key, Val> *hm, sizet dest_ind, sizet src_ind)
{
    auto cur_b = &hm->buckets[src_ind];

    // If we are the tail node, we need to point the head node's prev dest ind.
    if (!is_valid(cur_b->next)) {
        auto cur_bckt = src_ind;
        while (hm->buckets[cur_bckt].prev != src_ind) {
            cur_bckt = hm->buckets[cur_bckt].prev;
        }
        if (cur_bckt != src_ind) {
            hm->buckets[cur_bckt].prev = dest_ind;
        }
    }

    if (hm->buckets[cur_b->prev].next == src_ind) {
        hm->buckets[cur_b->prev].next = dest_ind;
    }
    if (is_valid(cur_b->next) && hm->buckets[cur_b->next].prev == src_ind) {
        hm->buckets[cur_b->next].prev = dest_ind;
    }

    auto previ_b = &hm->buckets[cur_b->item.prev];
    if (previ_b->item.next == src_ind) {
        previ_b->item.next = dest_ind;
    }
    if (is_valid(cur_b->item.next) && hm->buckets[cur_b->item.next].item.prev == src_ind) {
        hm->buckets[cur_b->item.next].item.prev = dest_ind;
    }

    hm->buckets[dest_ind] = hm->buckets[src_ind];
    if (hm->buckets[dest_ind].prev == src_ind) {
        hm->buckets[dest_ind].prev = dest_ind;
    }
    if (hm->buckets[dest_ind].item.prev == src_ind) {
        hm->buckets[dest_ind].item.prev = dest_ind;
    }
    if (hm->buckets[hm->head].item.prev == src_ind) {
        hm->buckets[hm->head].item.prev = dest_ind;
    }
}

template<typename Key, typename Val>
void hmap_clear_bucket(hmap<Key, Val> *hm, sizet bckt_ind)
{
    asrt(bckt_ind < hm->buckets.size);

    // If our next index is valid, use it to get the next bucket - set the next bucket's prev index to our prev index
    if (is_valid(hm->buckets[bckt_ind].next)) {
        hm->buckets[hm->buckets[bckt_ind].next].prev = hm->buckets[bckt_ind].prev;
    }

    // If head is pointing to us (we are the last bucket), then make head point to our prev
    if (hm->buckets[hm->head].item.prev == bckt_ind) {
        hm->buckets[hm->head].item.prev = hm->buckets[bckt_ind].item.prev;
    }

    // If head is us, then head should point to our next index
    if (hm->head == bckt_ind && is_valid(hm->buckets[bckt_ind].item.next)) {
        hm->head = hm->buckets[bckt_ind].item.next;
    }

    // If we are the tail node, we need to point the head node's prev to our prev. If we are the tail and the head node,
    // this will just point the head nodes's (us) prev to our prev (us) essentially doing nothing, so we just skip it in
    // that case (where the head node ind ends up as our ind)
    if (!is_valid(hm->buckets[bckt_ind].next)) {
        auto cur_bckt = bckt_ind;
        while (hm->buckets[cur_bckt].prev != bckt_ind) {
            cur_bckt = hm->buckets[cur_bckt].prev;
        }
        if (cur_bckt != bckt_ind) {
            hm->buckets[cur_bckt].prev = hm->buckets[bckt_ind].prev;
        }
    }

    // If the prev bucket's next index is invalid, it means we are the head node of the bucket. We only want to remove
    // the node from the bucket ll if we are NOT the head node - or technically if we are the head node but the only
    // node. The thing is, in that case, our next index will already be invalid anyways.
    if (is_valid(hm->buckets[hm->buckets[bckt_ind].prev].next)) { // || hm->buckets[bckt_ind].prev == bckt_ind) {
        hm->buckets[hm->buckets[bckt_ind].prev].next = hm->buckets[bckt_ind].next;
        hm->buckets[bckt_ind].next = INVALID_IND;
    }

    // Set the previous ind to invalid - we don't set the hashed_v on purpose that is never used
    hm->buckets[bckt_ind].prev = INVALID_IND;

    // Do the same thing we did for the bucket indexes except now with the item indices
    if (is_valid(hm->buckets[bckt_ind].item.next)) {
        hm->buckets[hm->buckets[bckt_ind].item.next].item.prev = hm->buckets[bckt_ind].item.prev;
    }
    // If the previous item has a valid next index (ie if we are not the head node) then set the previous item's next
    // index to our next index to continue the chain
    if (is_valid(hm->buckets[hm->buckets[bckt_ind].item.prev].item.next)) {
        hm->buckets[hm->buckets[bckt_ind].item.prev].item.next = hm->buckets[bckt_ind].item.next;
    }

    // Reset the bucket item
    hm->buckets[bckt_ind].item = {};
}

template<typename Key, typename Val>
void hmap_remove_bucket(hmap<Key, Val> *hm, sizet bckt_ind)
{
    asrt(bckt_ind < hm->buckets.size);
    if (!is_valid(hm->buckets[bckt_ind].prev)) {
        return;
    }
    bool was_last = (hm->count == 1);
    sizet hole_ind = bckt_ind;
    sizet bckt_cnt = hm->buckets.size;
    hmap_clear_bucket(hm, bckt_ind);
    sizet next_bckt = (hole_ind + 1) % bckt_cnt;
    while (next_bckt != hole_ind) {
        if (!is_valid(hm->buckets[next_bckt].prev)) {
            break;
        }
        // Compare circular distance from the bucket's ideal slot to its current location
        // versus the open hole; if the hole is closer, this entry must slide back.
        sizet target_bckt = hm->buckets[next_bckt].hashed_v % bckt_cnt;
        sizet dist_to_next = (next_bckt + bckt_cnt - target_bckt) % bckt_cnt;
        sizet dist_to_hole = (hole_ind + bckt_cnt - target_bckt) % bckt_cnt;
        if (dist_to_hole < dist_to_next) {
            hmap_copy_bucket(hm, hole_ind, next_bckt);
            hole_ind = next_bckt;
        }
        next_bckt = (next_bckt + 1) % bckt_cnt;
    }
    hm->buckets[hole_ind] = {};
    --hm->count;
    if (was_last) {
        hm->head = INVALID_IND;
    }
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_erase(hmap<Key, Val> *hm, typename hmap<Key, Val>::iterator item)
{
    hmap_item<Key, Val> *ret{};
    if (!item || !is_valid(item->prev)) {
        return ret;
    }
    if (is_valid(item->next)) {
        ret = &hm->buckets[item->next].item;
    }

    // Item is the first member of hmap_bucket so the pointers align.
    auto bckt = (hmap_bucket<Key, Val> *)item;
    sizet bckt_ind = (sizet)(bckt - hm->buckets.data);
    asrt(bckt_ind < hm->buckets.size);
    hmap_remove_bucket(hm, bckt_ind);
    return ret;
}

// Remove the entry for key k from the map. If val is not null, fill it with the value of the item removed.
template<typename Key, typename Val>
bool hmap_remove(hmap<Key, Val> *hm, const Key &k, Val *val = nullptr)
{
    sizet bckt_ind = hmap_find_bucket(hm, k);
    if (bckt_ind != INVALID_IND) {
        if (val) {
            *val = hm->buckets[bckt_ind].item.val;
        }
        hmap_remove_bucket(hm, bckt_ind);
        return true;
    }
    return false;
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_insert_or_set(hmap<Key, Val> *hm, const Key &k, const Val &val, bool set_if_exists)
{
    asrt(hm->hashf);
    if (hm->buckets.size == 0) {
        return nullptr;
    }
    if (hmap_should_rehash_on_insert(hm)) {
        hmap_rehash(hm, hm->buckets.size * 2);
    }

    u64 hashval = hm->hashf(k, hm->seed0, hm->seed1);
    sizet bckt_ind = hashval % hm->buckets.size;

    // Find an un-occupied bucket - a bucket is unoccupied if the prev bucket index is invalid. Head nodes have the prev
    // bucket index pointing to the last item in the bucket (the last item does not have its next pointing to the first
    // item however).
    auto cur_bckt_ind = bckt_ind;
    auto head_bckt_ind = INVALID_IND;
    sizet i{};
    while (is_valid(hm->buckets[cur_bckt_ind].prev)) {
        // If we found a match (both hashed_v and the actual key) then return null
        if (hm->buckets[cur_bckt_ind].hashed_v == hashval && hm->buckets[cur_bckt_ind].item.key == k) {
            if (set_if_exists) {
                hm->buckets[cur_bckt_ind].item.val = val;
                return &hm->buckets[cur_bckt_ind].item;
            }
            else {
                return nullptr;
            }
        }

        // The head bucket will be the first bucket we find with the same hashed bucket as ours
        if (!is_valid(head_bckt_ind) && (hm->buckets[cur_bckt_ind].hashed_v % hm->buckets.size) == bckt_ind) {
            head_bckt_ind = cur_bckt_ind;
        }

        cur_bckt_ind = (hashval + ++i) % hm->buckets.size;
    }

    // New bucket insertion
    if (!is_valid(head_bckt_ind)) {
        head_bckt_ind = cur_bckt_ind;
    }

    // Check the bucket ll for existing items and return null if any
    sizet n = hm->buckets[head_bckt_ind].next;
    while (is_valid(n)) {
        if (hm->buckets[n].hashed_v == hashval && hm->buckets[n].item.key == k) {
            if (set_if_exists) {
                hm->buckets[n].item.val = val;
                return &hm->buckets[n].item;
            }
            else {
                return nullptr;
            }
        }
        n = hm->buckets[n].next;
    }

    // The bucket item's next and prev should both be invalid
    asrt(!is_valid(hm->buckets[cur_bckt_ind].item.next));
    asrt(!is_valid(hm->buckets[cur_bckt_ind].item.prev));

    // Set the key/value/hashed_v
    hm->buckets[cur_bckt_ind].hashed_v = hashval;
    hm->buckets[cur_bckt_ind].item.key = k;
    hm->buckets[cur_bckt_ind].item.val = val;

    // If this is the first item, we create head pointing to itself as prev and leave next invalid
    if (!is_valid(hm->head)) {
        hm->head = cur_bckt_ind;
        hm->buckets[cur_bckt_ind].item.prev = cur_bckt_ind;
    }
    else {
        // Our bucket's prev should point to the last item, which is head's prev, our next will remain pointing to
        // invalid to indicate it is the last item. Then head's prev should point to us
        hm->buckets[cur_bckt_ind].item.prev = hm->buckets[hm->head].item.prev;
        hm->buckets[hm->head].item.prev = cur_bckt_ind;

        // And finally the bucket before us (which was previously the last bucket) should now point to us as next
        asrt(!is_valid(hm->buckets[hm->buckets[cur_bckt_ind].item.prev].item.next));
        hm->buckets[hm->buckets[cur_bckt_ind].item.prev].item.next = cur_bckt_ind;
    }

    // And now, we need to insert the item in the bucket item's linked list chain
    asrt(!is_valid(hm->buckets[cur_bckt_ind].prev));

    // If we are appending to a bucket ll rather than inserting the head bucket node
    sizet head_prev_ind = cur_bckt_ind;
    if (cur_bckt_ind != head_bckt_ind) {
        // Make sure the head bucket has a valid prev ind
        asrt(is_valid(hm->buckets[head_bckt_ind].prev));

        // Set our prev to the bucket that was previously at the end
        hm->buckets[cur_bckt_ind].prev = hm->buckets[head_bckt_ind].prev;

        // Asssert the previously end bucket's next index is invalid, and then set it to us
        asrt(!is_valid(hm->buckets[hm->buckets[cur_bckt_ind].prev].next));
        hm->buckets[hm->buckets[cur_bckt_ind].prev].next = cur_bckt_ind;

        // Set the head bucket's prev ind to us
    }
    // If we are the head bucket and our next ind is valid, it means we are inserting this item to a previously deleted
    // head bucket, and need to find the end of the bucket ll to set our prev too
    else if (is_valid(hm->buckets[cur_bckt_ind].next)) {
        while (is_valid(hm->buckets[head_prev_ind].next)) {
            head_prev_ind = hm->buckets[head_prev_ind].next;
        }
    }
    hm->buckets[head_bckt_ind].prev = head_prev_ind;

    ++hm->count;
    return &hm->buckets[cur_bckt_ind].item;
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
    sizet bucket_ind = hmap_find_bucket(hm, k);
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
    sizet bucket_ind = hmap_find_bucket(hm, k);
    if (bucket_ind != INVALID_IND) {
        return &hm->buckets[bucket_ind].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_find(const hmap<Key, Val> *hm, const Key &k)
{
    sizet bucket_ind = hmap_find_bucket(hm, k);
    if (bucket_ind != INVALID_IND) {
        return &hm->buckets[bucket_ind].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
bool hmap_empty(const hmap<Key, Val> *hm)
{
    return hm->count == 0;
}

template<typename Key, typename Val>
void hmap_clear(hmap<Key, Val> *hm)
{
    hm->head = INVALID_IND;
    hm->count = 0;
    arr_clear_to(&hm->buckets, {});
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_begin(hmap<Key, Val> *hm)
{
    if (is_valid(hm->head)) {
        return &hm->buckets[hm->head].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_begin(const hmap<Key, Val> *hm)
{
    if (is_valid(hm->head)) {
        return &hm->buckets[hm->head].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_rbegin(hmap<Key, Val> *hm)
{
    if (is_valid(hm->head)) {
        asrt(is_valid(hm->buckets[hm->head].item.prev));
        return &hm->buckets[hm->buckets[hm->head].item.prev].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_rbegin(const hmap<Key, Val> *hm)
{
    if (is_valid(hm->head)) {
        asrt(is_valid(hm->buckets[hm->head].item.prev));
        return &hm->buckets[hm->buckets[hm->head].item.prev].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_next(hmap<Key, Val> *hm, typename hmap<Key, Val>::iterator item)
{
    if (item && is_valid(item->next)) {
        return &hm->buckets[item->next].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_next(const hmap<Key, Val> *hm, typename hmap<Key, Val>::const_iterator item)
{
    if (item && is_valid(item->next)) {
        return &hm->buckets[item->next].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::iterator hmap_prev(hmap<Key, Val> *hm, typename hmap<Key, Val>::iterator item)
{
    if (item && item != &hm->buckets[hm->head].item && is_valid(item->prev)) {
        return &hm->buckets[item->prev].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap<Key, Val>::const_iterator hmap_prev(const hmap<Key, Val> *hm, typename hmap<Key, Val>::const_iterator item)
{
    if (item && item != &hm->buckets[hm->head].item && is_valid(item->prev)) {
        return &hm->buckets[item->prev].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
void hmap_terminate(hmap<Key, Val> *hm)
{
    arr_terminate(&hm->buckets);
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
            pup_var(ar, item_key, {to_cstr("{%d}", i)});
            pup_var(ar, item_val, {to_cstr("{%d}", i)});
            hmap_set(&val, item_key, item_val);
            ++i;
        }
    }
    else {
        auto iter = hmap_begin(&val);
        while (iter) {
            pup_var(ar, iter->key, {to_cstr("{%d}", i)});
            pup_var(ar, iter->val, {to_cstr("{%d}", i)});
            iter = hmap_next(&val, iter);
            ++i;
        }
    }
}

} // namespace nslib
