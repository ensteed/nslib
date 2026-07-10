#pragma once
#include "array.h"
#include "../util.h"

namespace nslib
{
constexpr inline float HASH_TABLE_DEFAULT_LOAD_FACTOR = 0.75f;

template<typename Item>
struct hash_bucket
{
    Item item{};
    u64 hashed_v{};
    sizet next{INVALID_ID};
    sizet prev{INVALID_ID};
};

template<typename Item>
struct hash_table
{
    using item_type = Item;
    using value_type = Item;
    using key_type = typename Item::key_type;
    using mapped_type = typename Item::mapped_type;
    using iterator = typename Item::iterator;
    using const_iterator = typename Item::const_iterator;
    using hash_key_type = typename Item::hash_key;
    using hash_func = u64(const hash_key_type &, u64, u64);

    hash_func *hashf{};
    u64 seed0{};
    u64 seed1{};
    float load_factor{0.0f};
    sizet head{INVALID_ID};
    sizet count{0};
    array<hash_bucket<Item>> buckets{};
};

template<typename Item>
sizet hash_table_find_bucket(const hash_table<Item> *hc, const typename hash_table<Item>::hash_key_type &k)
{
    asrt(hc->hashf);
    if (hc->buckets.size == 0) {
        return INVALID_ID;
    }
    u64 hashval = hc->hashf(k, hc->seed0, hc->seed1);
    sizet fnd_bckt = INVALID_ID;
    sizet bckt_ind = hashval % hc->buckets.size;
    sizet cur_bckt_ind = bckt_ind;
    sizet i = 0;

    // Find the correct bucket first - hashed_v mod bucket count should give us bckt_ind if a match
    while (i < hc->buckets.size && is_valid(hc->buckets[cur_bckt_ind].prev) &&
           (bckt_ind != (hc->buckets[cur_bckt_ind].hashed_v % hc->buckets.size))) {
        ++i;
        cur_bckt_ind = (hashval + i) % hc->buckets.size;
    }

    // If we found a matching bucket
    if (i < hc->buckets.size && is_valid(hc->buckets[cur_bckt_ind].prev) &&
        (bckt_ind == (hc->buckets[cur_bckt_ind].hashed_v % hc->buckets.size))) {
        // Follow the bucket linked list to check for matches on any of the items in the bucket
        while (is_valid(cur_bckt_ind)) {
            if ((hashval != hc->buckets[cur_bckt_ind].hashed_v || !hash_table_item_match(hc->buckets[cur_bckt_ind].item, k))) {
                cur_bckt_ind = hc->buckets[cur_bckt_ind].next;
            }
            else {
                fnd_bckt = cur_bckt_ind;
                cur_bckt_ind = INVALID_ID;
            }
        }
    }
    return fnd_bckt;
}

template<typename Item, typename Key, typename Val>
typename hash_table<Item>::iterator hash_table_insert_or_set(hash_table<Item> *hc, const Key &k, const Val &val, bool set_if_exists);

template<typename Item>
void hash_table_rehash(hash_table<Item> *hc, sizet new_size)
{
    // Make tmp copy of buckets and head index, then clear the hash table, then resize the buckets to the new size
    auto tmp = hc->buckets;
    sizet ind = hc->head;
    hash_table_clear(hc);
    arr_resize(&hc->buckets, new_size);

    // Temporarily set the load factor so the hash table will never rehash on insert
    auto cached_lf = hc->load_factor;
    hc->load_factor = 2.0f;

    // And finally insert all of the items in the tmp copy in to the new bucket array
    while (is_valid(ind)) {
        const auto &item = tmp[ind].item;
        hash_table_insert_or_set(hc, hash_table_item_key(item), hash_table_item_value(item), false);
        ind = tmp[ind].item.next;
    }

    // Restore cached load factor
    hc->load_factor = cached_lf;
}

template<typename Item, typename Key, typename Val>
typename hash_table<Item>::iterator hash_table_insert_or_set(hash_table<Item> *hc, const Key &k, const Val &val, bool set_if_exists)
{
    asrt(hc->hashf);
    if (hc->buckets.size == 0) {
        return nullptr;
    }
    if (hash_table_should_rehash_on_insert(hc)) {
        hash_table_rehash(hc, hc->buckets.size * 2);
    }

    u64 hashval = hc->hashf(k, hc->seed0, hc->seed1);
    sizet bckt_ind = hashval % hc->buckets.size;

    // Find an un-occupied bucket - a bucket is unoccupied if the prev bucket index is invalid. Head nodes have the prev
    // bucket index pointing to the last item in the bucket (the last item does not have its next pointing to the first
    // item however).
    auto cur_bckt_ind = bckt_ind;
    auto head_bckt_ind = INVALID_ID;
    sizet i{};
    while (is_valid(hc->buckets[cur_bckt_ind].prev) && i < hc->buckets.size) {
        // If we found a match (both hashed_v and the actual key) then return null
        if (hc->buckets[cur_bckt_ind].hashed_v == hashval && hash_table_item_match(hc->buckets[cur_bckt_ind].item, k)) {
            if (set_if_exists) {
                set_hash_table_item_value(hc->buckets[cur_bckt_ind].item, k, val);
                return &hc->buckets[cur_bckt_ind].item;
            }
            return nullptr;
        }

        // The head bucket will be the first bucket we find with the same hashed bucket as ours
        if (!is_valid(head_bckt_ind) && (hc->buckets[cur_bckt_ind].hashed_v % hc->buckets.size) == bckt_ind) {
            head_bckt_ind = cur_bckt_ind;
        }

        cur_bckt_ind = (hashval + ++i) % hc->buckets.size;
    }
    if (i >= hc->buckets.size) {
        return nullptr;
    }

    // New bucket insertion
    if (!is_valid(head_bckt_ind)) {
        head_bckt_ind = cur_bckt_ind;
    }

    // Check the bucket ll for existing items and return null if any
    sizet n = hc->buckets[head_bckt_ind].next;
    while (is_valid(n)) {
        if (hc->buckets[n].hashed_v == hashval && hash_table_item_match(hc->buckets[n].item, k)) {
            if (set_if_exists) {
                set_hash_table_item_value(hc->buckets[n].item, k, val);
                return &hc->buckets[n].item;
            }
            return nullptr;
        }
        n = hc->buckets[n].next;
    }

    // The bucket item's next and prev should both be invalid
    asrt(!is_valid(hc->buckets[cur_bckt_ind].item.next));
    asrt(!is_valid(hc->buckets[cur_bckt_ind].item.prev));

    // Set the key/value/hashed_v
    hc->buckets[cur_bckt_ind].hashed_v = hashval;
    set_hash_table_item_value(hc->buckets[cur_bckt_ind].item, k, val);

    // If this is the first item, we create head pointing to itself as prev and leave next invalid
    if (!is_valid(hc->head)) {
        hc->head = cur_bckt_ind;
        hc->buckets[cur_bckt_ind].item.prev = cur_bckt_ind;
    }
    else {
        // Our bucket's prev should point to the last item, which is head's prev, our next will remain pointing to
        // invalid to indicate it is the last item. Then head's prev should point to us
        hc->buckets[cur_bckt_ind].item.prev = hc->buckets[hc->head].item.prev;
        hc->buckets[hc->head].item.prev = cur_bckt_ind;

        // And finally the bucket before us (which was previously the last bucket) should now point to us as next
        asrt(!is_valid(hc->buckets[hc->buckets[cur_bckt_ind].item.prev].item.next));
        hc->buckets[hc->buckets[cur_bckt_ind].item.prev].item.next = cur_bckt_ind;
    }

    // And now, we need to insert the item in the bucket item's linked list chain
    asrt(!is_valid(hc->buckets[cur_bckt_ind].prev));

    // If we are appending to a bucket ll rather than inserting the head bucket node
    sizet head_prev_ind = cur_bckt_ind;
    if (cur_bckt_ind != head_bckt_ind) {
        // Make sure the head bucket has a valid prev ind
        asrt(is_valid(hc->buckets[head_bckt_ind].prev));

        // Set our prev to the bucket that was previously at the end
        hc->buckets[cur_bckt_ind].prev = hc->buckets[head_bckt_ind].prev;

        // Asssert the previously end bucket's next index is invalid, and then set it to us
        asrt(!is_valid(hc->buckets[hc->buckets[cur_bckt_ind].prev].next));
        hc->buckets[hc->buckets[cur_bckt_ind].prev].next = cur_bckt_ind;

        // Set the head bucket's prev ind to us
    }
    // If we are the head bucket and our next ind is valid, it means we are inserting this item to a previously deleted
    // head bucket, and need to find the end of the bucket ll to set our prev too
    else if (is_valid(hc->buckets[cur_bckt_ind].next)) {
        while (is_valid(hc->buckets[head_prev_ind].next)) {
            head_prev_ind = hc->buckets[head_prev_ind].next;
        }
    }
    hc->buckets[head_bckt_ind].prev = head_prev_ind;

    ++hc->count;
    return &hc->buckets[cur_bckt_ind].item;
}

template<typename Item>
void hash_table_init(hash_table<Item> *hc, mem_arena *arena, typename hash_table<Item>::hash_func *hashf, sizet initial_capacity, float load_factor)
{
    hc->hashf = hashf;
    hc->seed0 = generate_rand_seed();
    hc->seed1 = generate_rand_seed();
    hc->head = INVALID_ID;
    hc->load_factor = load_factor;
    hc->count = 0;
    arr_init(&hc->buckets, arena, initial_capacity);
    arr_resize(&hc->buckets, initial_capacity);
}

template<typename Item>
float hash_table_load_factor(const hash_table<Item> *hc, sizet entry_count)
{
    if (hc->buckets.size == 0) {
        return 0.0f;
    }
    return (float)entry_count / (float)hc->buckets.size;
}

template<typename Item>
float hash_table_current_load_factor(const hash_table<Item> *hc)
{
    return hash_table_load_factor(hc, hc->count);
}

template<typename Item>
bool hash_table_should_rehash_on_insert(const hash_table<Item> *hc)
{
    if (hc->load_factor >= 0.0f && hc->load_factor <= 1.0f) {
        return hash_table_load_factor(hc, hc->count + 1) > hc->load_factor;
    }
    return false;
}

template<typename Item>
void hash_table_copy_bucket(hash_table<Item> *hc, sizet dest_ind, sizet src_ind)
{
    auto cur_b = &hc->buckets[src_ind];

    // If we are the tail node, we need to point the head node's prev dest ind.
    if (!is_valid(cur_b->next)) {
        auto cur_bckt = src_ind;
        while (hc->buckets[cur_bckt].prev != src_ind) {
            cur_bckt = hc->buckets[cur_bckt].prev;
        }
        if (cur_bckt != src_ind) {
            hc->buckets[cur_bckt].prev = dest_ind;
        }
    }

    if (hc->buckets[cur_b->prev].next == src_ind) {
        hc->buckets[cur_b->prev].next = dest_ind;
    }
    if (is_valid(cur_b->next) && hc->buckets[cur_b->next].prev == src_ind) {
        hc->buckets[cur_b->next].prev = dest_ind;
    }

    auto previ_b = &hc->buckets[cur_b->item.prev];
    if (previ_b->item.next == src_ind) {
        previ_b->item.next = dest_ind;
    }
    if (is_valid(cur_b->item.next) && hc->buckets[cur_b->item.next].item.prev == src_ind) {
        hc->buckets[cur_b->item.next].item.prev = dest_ind;
    }

    hc->buckets[dest_ind] = hc->buckets[src_ind];
    if (hc->buckets[dest_ind].prev == src_ind) {
        hc->buckets[dest_ind].prev = dest_ind;
    }
    if (hc->buckets[dest_ind].item.prev == src_ind) {
        hc->buckets[dest_ind].item.prev = dest_ind;
    }
    if (hc->buckets[hc->head].item.prev == src_ind) {
        hc->buckets[hc->head].item.prev = dest_ind;
    }
    if (hc->head == src_ind) {
        hc->head = dest_ind;
    }
}

// Clear the bucket
template<typename Item>
auto hash_table_clear_bucket(hash_table<Item> *hc, sizet bckt_ind)
{
    asrt(bckt_ind < hc->buckets.size);
    if (!is_valid(hc->buckets[bckt_ind].prev)) {
        return;
    }

    // If our next index is valid, use it to get the next bucket - set the next bucket's prev index to our prev index
    if (is_valid(hc->buckets[bckt_ind].next)) {
        hc->buckets[hc->buckets[bckt_ind].next].prev = hc->buckets[bckt_ind].prev;
    }

    // If head is pointing to us (we are the last bucket), then make head point to our prev
    if (hc->buckets[hc->head].item.prev == bckt_ind) {
        hc->buckets[hc->head].item.prev = hc->buckets[bckt_ind].item.prev;
    }

    // If head is us, then head should point to our next index
    if (hc->head == bckt_ind && is_valid(hc->buckets[bckt_ind].item.next)) {
        hc->head = hc->buckets[bckt_ind].item.next;
    }

    // If we are the tail node, we need to point the head node's prev to our prev. If we are the tail and the head node,
    // this will just point the head nodes's (us) prev to our prev (us) essentially doing nothing, so we just skip it in
    // that case (where the head node ind ends up as our ind)
    if (!is_valid(hc->buckets[bckt_ind].next)) {
        auto cur_bckt = bckt_ind;
        while (hc->buckets[cur_bckt].prev != bckt_ind) {
            cur_bckt = hc->buckets[cur_bckt].prev;
        }
        if (cur_bckt != bckt_ind) {
            hc->buckets[cur_bckt].prev = hc->buckets[bckt_ind].prev;
        }
    }

    // If the prev bucket's next index is invalid, it means we are the head node of the bucket. We only want to remove
    // the node from the bucket ll if we are NOT the head node - or technically if we are the head node but the only
    // node. The thing is, in that case, our next index will already be invalid anyways.
    if (is_valid(hc->buckets[hc->buckets[bckt_ind].prev].next)) { // || hc->buckets[bckt_ind].prev == bckt_ind) {
        hc->buckets[hc->buckets[bckt_ind].prev].next = hc->buckets[bckt_ind].next;
        hc->buckets[bckt_ind].next = INVALID_ID;
    }

    // Set the previous ind to invalid - we don't set the hashed_v on purpose that is never used
    hc->buckets[bckt_ind].prev = INVALID_ID;

    // Do the same thing we did for the bucket indexes except now with the item indices
    if (is_valid(hc->buckets[bckt_ind].item.next)) {
        hc->buckets[hc->buckets[bckt_ind].item.next].item.prev = hc->buckets[bckt_ind].item.prev;
    }
    // If the previous item has a valid next index (ie if we are not the head node) then set the previous item's next
    // index to our next index to continue the chain
    if (is_valid(hc->buckets[hc->buckets[bckt_ind].item.prev].item.next)) {
        hc->buckets[hc->buckets[bckt_ind].item.prev].item.next = hc->buckets[bckt_ind].item.next;
    }

    // Reset the bucket item
    hc->buckets[bckt_ind].item = {};
}

template<typename Item>
void hash_table_remove_bucket(hash_table<Item> *hc, sizet bckt_ind)
{

    asrt(bckt_ind < hc->buckets.size);
    if (!is_valid(hc->buckets[bckt_ind].prev)) {
        return;
    }
    bool was_last = (hc->count == 1);
    sizet hole_ind = bckt_ind;
    sizet bckt_cnt = hc->buckets.size;
    hash_table_clear_bucket(hc, bckt_ind);
    sizet next_bckt = (hole_ind + 1) % bckt_cnt;
    while (next_bckt != hole_ind) {
        if (!is_valid(hc->buckets[next_bckt].prev)) {
            break;
        }
        // Compare circular distance from the bucket's ideal slot to its current location
        // versus the open hole; if the hole is closer, this entry must slide back.
        sizet target_bckt = hc->buckets[next_bckt].hashed_v % bckt_cnt;
        sizet dist_to_next = (next_bckt + bckt_cnt - target_bckt) % bckt_cnt;
        sizet dist_to_hole = (hole_ind + bckt_cnt - target_bckt) % bckt_cnt;
        if (dist_to_hole < dist_to_next) {
            hash_table_copy_bucket(hc, hole_ind, next_bckt);
            hole_ind = next_bckt;
        }
        next_bckt = (next_bckt + 1) % bckt_cnt;
    }
    hc->buckets[hole_ind] = {};
    --hc->count;
    if (was_last) {
        hc->head = INVALID_ID;
    }
}

template<typename Item, typename IteratorT>
IteratorT hash_table_erase(hash_table<Item> *hc, IteratorT item)
{
    IteratorT ret{};
    if (!item || !is_valid(item->prev)) {
        return ret;
    }
    if (is_valid(item->next)) {
        ret = &hc->buckets[item->next].item;
    }

    // Item is the first member of hash_bucket so the pointers align.
    auto bckt = (hash_bucket<Item> *)(const_cast<Item *>(item));
    sizet bckt_ind = (sizet)(bckt - hc->buckets.data);
    asrt(bckt_ind < hc->buckets.size);
    hash_table_remove_bucket(hc, bckt_ind);
    return ret;
}

template<typename Item>
bool hash_table_empty(const hash_table<Item> *hc)
{
    return hc->count == 0;
}

template<typename Item>
void hash_table_clear(hash_table<Item> *hc)
{
    hc->head = INVALID_ID;
    hc->count = 0;
    arr_clear_to(&hc->buckets, {});
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_begin(hash_table<Item> *hc)
{
    if (is_valid(hc->head)) {
        return &hc->buckets[hc->head].item;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_begin(const hash_table<Item> *hc)
{
    if (is_valid(hc->head)) {
        return &hc->buckets[hc->head].item;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_rbegin(hash_table<Item> *hc)
{
    if (is_valid(hc->head)) {
        asrt(is_valid(hc->buckets[hc->head].item.prev));
        return &hc->buckets[hc->buckets[hc->head].item.prev].item;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_rbegin(const hash_table<Item> *hc)
{
    if (is_valid(hc->head)) {
        asrt(is_valid(hc->buckets[hc->head].item.prev));
        return &hc->buckets[hc->buckets[hc->head].item.prev].item;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_next(hash_table<Item> *hc, typename hash_table<Item>::iterator item)
{
    if (!item) {
        return hash_table_begin(hc);
    }
    if (item && is_valid(item->next)) {
        return &hc->buckets[item->next].item;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_next(const hash_table<Item> *hc, typename hash_table<Item>::const_iterator item)
{
    if (!item) {
        return hash_table_begin(hc);
    }
    if (item && is_valid(item->next)) {
        return &hc->buckets[item->next].item;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_prev(hash_table<Item> *hc, typename hash_table<Item>::iterator item)
{
    if (!item) {
        return hash_table_rbegin(hc);
    }
    if (item && item != &hc->buckets[hc->head].item && is_valid(item->prev)) {
        return &hc->buckets[item->prev].item;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_prev(const hash_table<Item> *hc, typename hash_table<Item>::const_iterator item)
{
    if (!item) {
        return hash_table_rbegin(hc);
    }
    if (item && item != &hc->buckets[hc->head].item && is_valid(item->prev)) {
        return &hc->buckets[item->prev].item;
    }
    return nullptr;
}

template<typename Item>
void hash_table_terminate(hash_table<Item> *hc)
{
    arr_terminate(&hc->buckets);
}

} // namespace nslib
