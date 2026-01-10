#pragma once
#include "array.h"
#include "../util.h"
#include "../containers/string.h"
#include "../hashfuncs.h"

namespace nslib
{
constexpr inline sizet HSET_DEFAULT_BUCKET_COUNT = 16;
constexpr inline float HSET_DEFAULT_LOAD_FACTOR = 0.75f;

enum hset_bucket_flags
{
    HSET_BUCKET_FLAG_USED = 1
};

template<typename Val>
struct hset_item
{
    Val val{};
    sizet next{INVALID_IND};
    sizet prev{INVALID_IND};
};

template<typename Val>
struct hset_bucket
{
    hset_item<Val> item{};
    u64 hashed_v{};
    sizet next{INVALID_IND};
    sizet prev{INVALID_IND};
};

template<class Val>
using hash_func = u64(const Val&, u64, u64);

// Since hset manages memory, but we want it to act like a built in type in terms of copying and equality testing, we
// have to write copy ctor, dtor, assignment operator, and equality operators.
template<typename Val>
struct hset
{
    using value_type = hset_item<Val>;

    // The iterator really always must be constant because it would mess up the set to change anything in an hset item
    using iterator = const value_type *;

    hash_func<Val> *hashf{};
    u64 seed0{};
    u64 seed1{};
    array<hset_bucket<Val>> buckets{};
    sizet head{};
    sizet count{0};
    // If this is set outside the range 0.0f to 1.0f auto rehashing on insert will not happen
    float load_factor{0.0f};
};

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
             to_cstr(b->item.val),
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
    hs->hashf = hashf;
    hs->seed0 = generate_rand_seed();
    hs->seed1 = generate_rand_seed();
    hs->head = INVALID_IND;
    hs->load_factor = HSET_DEFAULT_LOAD_FACTOR;
    hs->count = 0;
    arr_init(&hs->buckets, arena, initial_capacity);
    arr_resize(&hs->buckets, initial_capacity);
}

template<typename Val>
void hset_rehash(hset<Val> *hs, sizet new_size)
{
    // Make tmp copy of buckets and head index, then clear the hashmap, then resize the buckets to the new size
    auto tmp = hs->buckets;
    sizet ind = hs->head;
    hmap_clear(hs);
    arr_resize(&hs->buckets, new_size);
    // And finally insert all of the items in the tmp copy in to the new bucket array
    while (is_valid(ind)) {
        hmap_insert(hs, tmp[ind].item.key, tmp[ind].item.val);
        ind = tmp[ind].item.next;
    }
}

template<typename Val>
float hset_load_factor(const hset<Val> *hs, sizet hs_entry_count)
{
    return (float)hs_entry_count / (float)hs->buckets.size;
}

template<typename Val>
float hset_current_load_factor(const hset<Val> *hs, sizet hs_entry_count)
{
    return hset_load_factor(hs, hs->count);
}

template<typename Val>
bool hset_should_rehash_on_insert(const hset<Val> *hs)
{
    if (hs->load_factor >= 0.0f && hs->load_factor <= 1.0f) {
        return hset_load_factor(hs, hs->count + 1) > hs->load_factor;
    }
    return false;
}

template<typename Val>
sizet hset_find_bucket(const hset<Val> *hs, const Val &v)
{
    asrt(hs->hashf);
    if (hs->buckets.size == 0) {
        return false;
    }
    u64 hashval = hs->hashf(v, hs->seed0, hs->seed1);
    sizet fnd_bckt = INVALID_IND;
    sizet bckt_ind = hashval % hs->buckets.size;
    sizet cur_bckt_ind = bckt_ind;
    sizet i = 0;

    // Find the correct bucket first - hashed_v mod bucket count should give us bckt_ind if a match
    while (i < hs->buckets.size && is_valid(hs->buckets[cur_bckt_ind].prev) &&
           (bckt_ind != (hs->buckets[cur_bckt_ind].hashed_v % hs->buckets.size))) {
        ++i;
        cur_bckt_ind = (hashval + i) % hs->buckets.size;
    }

    // If we found a matching bucket
    if (i < hs->buckets.size && is_valid(hs->buckets[cur_bckt_ind].prev) &&
        (bckt_ind == (hs->buckets[cur_bckt_ind].hashed_v % hs->buckets.size))) {
        // Follow the bucket linked list to check for matches on any of the items in the bucket
        while (is_valid(cur_bckt_ind)) {
            if ((hashval != hs->buckets[cur_bckt_ind].hashed_v || k != hs->buckets[cur_bckt_ind].item.key)) {
                cur_bckt_ind = hs->buckets[cur_bckt_ind].next;
            }
            else {
                fnd_bckt = cur_bckt_ind;
                cur_bckt_ind = INVALID_IND;
            }
        }
    }
    return fnd_bckt;
}

template<typename Val>
void hset_copy_bucket(hset<Val> *hs, sizet dest_ind, sizet src_ind)
{
    auto cur_b = &hs->buckets[src_ind];

    // If we are the tail node, we need to point the head node's prev dest ind.
    if (!is_valid(cur_b->next)) {
        auto cur_bckt = src_ind;
        while (hs->buckets[cur_bckt].prev != src_ind) {
            cur_bckt = hs->buckets[cur_bckt].prev;
        }
        if (cur_bckt != src_ind) {
            hs->buckets[cur_bckt].prev = dest_ind;
        }
    }

    if (hs->buckets[cur_b->prev].next == src_ind) {
        hs->buckets[cur_b->prev].next = dest_ind;
    }
    if (is_valid(cur_b->next) && hs->buckets[cur_b->next].prev == src_ind) {
        hs->buckets[cur_b->next].prev = dest_ind;
    }

    auto previ_b = &hs->buckets[cur_b->item.prev];
    if (previ_b->item.next == src_ind) {
        previ_b->item.next = dest_ind;
    }
    if (is_valid(cur_b->item.next) && hs->buckets[cur_b->item.next].item.prev == src_ind) {
        hs->buckets[cur_b->item.next].item.prev = dest_ind;
    }

    hs->buckets[dest_ind] = hs->buckets[src_ind];
    if (hs->buckets[dest_ind].prev == src_ind) {
        hs->buckets[dest_ind].prev = dest_ind;
    }
    if (hs->buckets[dest_ind].item.prev == src_ind) {
        hs->buckets[dest_ind].item.prev = dest_ind;
    }
    if (hs->buckets[hs->head].item.prev == src_ind) {
        hs->buckets[hs->head].item.prev = dest_ind;
    }
}

template<typename Val>
void hset_clear_bucket(hset<Val> *hs, sizet bckt_ind)
{
    asrt(bckt_ind < hs->buckets.size);

    // If our next index is valid, use it to get the next bucket - set the next bucket's prev index to our prev index
    if (is_valid(hs->buckets[bckt_ind].next)) {
        hs->buckets[hs->buckets[bckt_ind].next].prev = hs->buckets[bckt_ind].prev;
    }

    // If head is pointing to us (we are the last bucket), then make head point to our prev
    if (hs->buckets[hs->head].item.prev == bckt_ind) {
        hs->buckets[hs->head].item.prev = hs->buckets[bckt_ind].item.prev;
    }

    // If head is us, then head should point to our next index
    if (hs->head == bckt_ind && is_valid(hs->buckets[bckt_ind].item.next)) {
        hs->head = hs->buckets[bckt_ind].item.next;
    }

    // If we are the tail node, we need to point the head node's prev to our prev. If we are the tail and the head node,
    // this will just point the head nodes's (us) prev to our prev (us) essentially doing nothing, so we just skip it in
    // that case (where the head node ind ends up as our ind)
    if (!is_valid(hs->buckets[bckt_ind].next)) {
        auto cur_bckt = bckt_ind;
        while (hs->buckets[cur_bckt].prev != bckt_ind) {
            cur_bckt = hs->buckets[cur_bckt].prev;
        }
        if (cur_bckt != bckt_ind) {
            hs->buckets[cur_bckt].prev = hs->buckets[bckt_ind].prev;
        }
    }

    // If the prev bucket's next index is invalid, it means we are the head node of the bucket. We only want to remove
    // the node from the bucket ll if we are NOT the head node - or technically if we are the head node but the only
    // node. The thing is, in that case, our next index will already be invalid anyways.
    if (is_valid(hs->buckets[hs->buckets[bckt_ind].prev].next)) {
        hs->buckets[hs->buckets[bckt_ind].prev].next = hs->buckets[bckt_ind].next;
        hs->buckets[bckt_ind].next = INVALID_IND;
    }

    // Set the previous ind to invalid - we don't set the hashed_v on purpose that is never used
    hs->buckets[bckt_ind].prev = INVALID_IND;

    // Do the same thing we did for the bucket indexes except now with the item indices
    if (is_valid(hs->buckets[bckt_ind].item.next)) {
        hs->buckets[hs->buckets[bckt_ind].item.next].item.prev = hs->buckets[bckt_ind].item.prev;
    }
    // If the previous item has a valid next index (ie if we are not the head node) then set the previous item's next
    // index to our next index to continue the chain
    if (is_valid(hs->buckets[hs->buckets[bckt_ind].item.prev].item.next)) {
        hs->buckets[hs->buckets[bckt_ind].item.prev].item.next = hs->buckets[bckt_ind].item.next;
    }

    // Reset the bucket item
    hs->buckets[bckt_ind].item = {};
}

template<typename Val>
void hset_remove_bucket(hset<Val> *hs, sizet bckt_ind)
{
    asrt(bckt_ind < hs->buckets.size);
    if (!is_valid(hs->buckets[bckt_ind].prev)) {
        return;
    }
    sizet next_bckt = bckt_ind;
    hset_clear_bucket(hs, bckt_ind);
    while (1) {
        next_bckt = (next_bckt + 1) % hs->buckets.size;
        if (next_bckt < bckt_ind || !is_valid(hs->buckets[next_bckt].prev)) {
            break;
        }
        sizet target_bckt = hs->buckets[next_bckt].hashed_v % hs->buckets.size;
        if (next_bckt > bckt_ind && (target_bckt <= bckt_ind || target_bckt > next_bckt)) {
            hset_copy_bucket(hs, bckt_ind, next_bckt);
            bckt_ind = next_bckt;
        }
    }
    hs->buckets[bckt_ind] = {};
}

template<typename Val>
hset<Val>::iterator hset_erase(hset<Val> *hs, typename hset<Val>::iterator item)
{
    hset_item<Val> *ret{};
    if (!item || !is_valid(item->prev)) {
        return ret;
    }
    if (is_valid(item->next)) {
        ret = &hs->buckets[item->next].item;
    }

    // Get our bucket ind from previous item's next.. if the previous item's next is invalid it means our previous item
    // was ourself (ie we are the only item)
    sizet bckt_ind = hs->buckets[item->prev].next;
    if (!is_valid(bckt_ind)) {
        bckt_ind = item->prev;
    }
    hset_remove_bucket(hs, bckt_ind);
    return ret;
}

// Remove the entry for key k from the map. If val is not null, fill it with the value of the item removed.
template<typename Val>
bool hset_remove(hset<Val> *hs, const Val &v)
{
    sizet bckt_ind = hset_find_bucket(hs, v);
    if (bckt_ind != INVALID_IND) {
        hset_remove_bucket(hs, bckt_ind);
        return true;
    }
    return false;
}

template<typename Val>
hset<Val>::iterator hset_find(const hset<Val> *hs, const Val &v)
{
    sizet bucket_ind = hset_find_bucket(hs, v);
    if (bucket_ind != INVALID_IND) {
        return &hs->buckets[bucket_ind].item;
    }
    return nullptr;
}

template<typename Val>
hset<Val>::iterator hset_insert_or_set(hset<Val> *hs, const Val &val, bool set_if_exists)
{
    asrt(hs->hashf);
    if (hs->buckets.size == 0) {
        return nullptr;
    }
    if (hset_should_rehash_on_insert(hs)) {
        hset_rehash(hs, hs->buckets.size * 2);
    }

    u64 hashval = hs->hashf(val, hs->seed0, hs->seed1);
    sizet bckt_ind = hashval % hs->buckets.size;

    // Find an un-occupied bucket - a bucket is unoccupied if the prev bucket index is invalid. Head nodes have the prev
    // bucket index pointing to the last item in the bucket (the last item does not have its next pointing to the first
    // item however).
    auto cur_bckt_ind = bckt_ind;
    auto head_bckt_ind = INVALID_IND;
    sizet i{};
    while (is_valid(hs->buckets[cur_bckt_ind].prev)) {
        // If we found a match (both hashed_v and the actual key) then return null
        if (hs->buckets[cur_bckt_ind].hashed_v == hashval && hs->buckets[cur_bckt_ind].item.val == val) {
            return nullptr;
        }

        // The head bucket will be the first bucket we find with the same hashed bucket as ours
        if (!is_valid(head_bckt_ind) && (hs->buckets[cur_bckt_ind].hashed_v % hs->buckets.size) == bckt_ind) {
            head_bckt_ind = cur_bckt_ind;
        }

        cur_bckt_ind = (hashval + ++i) % hs->buckets.size;
    }

    // New bucket insertion
    if (!is_valid(head_bckt_ind)) {
        head_bckt_ind = cur_bckt_ind;
    }

    // Check the bucket ll for existing items and return null if any
    sizet n = hs->buckets[head_bckt_ind].next;
    while (is_valid(n)) {
        if (hs->buckets[n].hashed_v == hashval && hs->buckets[n].item.val == val) {
            if (set_if_exists) {
                hs->buckets[n].item.val = val;
                return &hs->buckets[n].item;
            }
            else {
                return nullptr;
            }
        }
        n = hs->buckets[n].next;
    }

    // The bucket item's next and prev should both be invalid
    asrt(!is_valid(hs->buckets[cur_bckt_ind].item.next));
    asrt(!is_valid(hs->buckets[cur_bckt_ind].item.prev));

    // Set the key/value/hashed_v
    hs->buckets[cur_bckt_ind].hashed_v = hashval;
    hs->buckets[cur_bckt_ind].item.val = val;

    // If this is the first item, we create head pointing to itself as prev and leave next invalid
    if (!is_valid(hs->head)) {
        hs->head = cur_bckt_ind;
        hs->buckets[cur_bckt_ind].item.prev = cur_bckt_ind;
    }
    else {
        // Our bucket's prev should point to the last item, which is head's prev, our next will remain pointing to
        // invalid to indicate it is the last item. Then head's prev should point to us
        hs->buckets[cur_bckt_ind].item.prev = hs->buckets[hs->head].item.prev;
        hs->buckets[hs->head].item.prev = cur_bckt_ind;

        // And finally the bucket before us (which was previously the last bucket) should now point to us as next
        asrt(!is_valid(hs->buckets[hs->buckets[cur_bckt_ind].item.prev].item.next));
        hs->buckets[hs->buckets[cur_bckt_ind].item.prev].item.next = cur_bckt_ind;
    }

    // And now, we need to insert the item in the bucket item's linked list chain
    asrt(!is_valid(hs->buckets[cur_bckt_ind].prev));

    // If we are appending to a bucket ll rather than inserting the head bucket node
    sizet head_prev_ind = cur_bckt_ind;
    if (cur_bckt_ind != head_bckt_ind) {
        // Make sure the head bucket has a valid prev ind
        asrt(is_valid(hs->buckets[head_bckt_ind].prev));

        // Set our prev to the bucket that was previously at the end
        hs->buckets[cur_bckt_ind].prev = hs->buckets[head_bckt_ind].prev;

        // Asssert the previously end bucket's next index is invalid, and then set it to us
        asrt(!is_valid(hs->buckets[hs->buckets[cur_bckt_ind].prev].next));
        hs->buckets[hs->buckets[cur_bckt_ind].prev].next = cur_bckt_ind;

        // Set the head bucket's prev ind to us
    }
    // If we are the head bucket and our next ind is valid, it means we are inserting this item to a previously deleted
    // head bucket, and need to find the end of the bucket ll to set our prev too
    else if (is_valid(hs->buckets[cur_bckt_ind].next)) {
        while (is_valid(hs->buckets[head_prev_ind].next)) {
            head_prev_ind = hs->buckets[head_prev_ind].next;
        }
    }
    hs->buckets[head_bckt_ind].prev = head_prev_ind;

    ++hs->count;
    return &hs->buckets[cur_bckt_ind].item;
}

// Insert a new item into the map. If the key already exists, return null. If the key does not exist, insert it and
// return the inserted item
template<typename Val>
hset<Val>::iterator hset_insert(hset<Val> *hs, const Val &val)
{
    return hset_insert_or_set(hs, val, false);
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

// Insert a new item into the map. If the key already exists, set the value and return the item. If the key does not
// exist, create it. This may increase the hset capacity and rehash if the new size is greater
template<typename Val>
void hset_set(hset<Val> *hs, const Val &val)
{
    auto result = hset_insert_or_set(hs, val, true);
    asrt(result);
}

// Call hset_set for all items in src on dest.
template<typename Val>
void hset_set(hset<Val> *dest, const hset<Val> *src)
{
    auto iter = hset_begin(src);
    while (iter) {
        auto result = hset_set(dest, iter->key, iter->val);
        asrt(result);
        iter = hset_next(src, iter);
    }
}

template<typename Val>
bool hset_empty(const hset<Val> *hs) {
    return hs->count == 0;
}

template<typename Val>
void hset_clear(hset<Val> *hs)
{
    hs->head = INVALID_IND;
    hs->count = 0;
    arr_clear_to(&hs->buckets, {});
}

template<typename Val>
hset<Val>::iterator hset_begin(const hset<Val> *hs)
{
    if (is_valid(hs->head)) {
        return &hs->buckets[hs->head].item;
    }
    return nullptr;
}

template<typename Val>
hset<Val>::iterator hset_rbegin(const hset<Val> *hs)
{
    if (is_valid(hs->head)) {
        asrt(is_valid(hs->buckets[hs->head].item.prev));
        return &hs->buckets[hs->buckets[hs->head].item.prev].item;
    }
    return nullptr;
}

template<typename Val>
hset<Val>::iterator hset_next(const hset<Val> *hs, typename hset<Val>::iterator item)
{
    if (!item) {
        item = hset_begin(hs);
    }
    if (item && is_valid(item->next)) {
        return &hs->buckets[item->next].item;
    }
    return nullptr;
}

template<typename Val>
hset<Val>::iterator hset_prev(const hset<Val> *hs, typename hset<Val>::iterator item)
{
    if (!item) {
        item = hset_rbegin(hs);
    }
    if (item && item != &hs->buckets[hs->head].item && is_valid(item->prev)) {
        return &hs->buckets[item->prev].item;
    }
    return nullptr;
}

template<typename Val>
void hset_terminate(hset<Val> *hs)
{
    arr_terminate(&hs->buckets);
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
            pup_var(ar, item, {to_cstr("[%d]", i)});
            hset_set(&val, item);
            ++i;
        }
    }
    else {        
        auto iter = hset_begin(&val);
        while (iter) {
            // We know we are packing in to the archive so we can just remove the constness
            pup_var(ar, const_cast<T&>(iter->val), {to_cstr("{%d}", i)});
            iter = hset_next(&val, iter);
            ++i;
        }
    }
}

} // namespace nslib
