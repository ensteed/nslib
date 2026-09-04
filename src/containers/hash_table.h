#pragma once
#include "array.h"
#include "../util.h"

namespace nslib
{
constexpr inline float HASH_TABLE_DEFAULT_LOAD_FACTOR = 0.75f;
constexpr inline sizet HASH_TABLE_MIN_SLOT_COUNT = 4;

// A slot packs the probe distance in the upper 24 bits (1 for an entry sitting in its home slot, 2 one step past it,
// and so on) and an 8 bit fingerprint of the hash in the low byte. A dist_and_fp of zero means the slot is empty.
// Since the distance lives in the high bits, comparing two packed values compares probe distance first, which is what
// robin hood probing orders by - entries further from home sort before entries closer to home along a probe sequence.
constexpr inline u32 HASH_SLOT_DIST_INC = 1u << 8;
constexpr inline u32 HASH_SLOT_FP_MASK = 0xFFu;

struct hash_slot
{
    u32 dist_and_fp{};
    u32 idx{};
};

template<class Key>
using hash_func = u64(const Key &, u64, u64);

// Dense hash table. Items live packed in insertion order in the items array, and a separate power of two sized slot
// array maps hashes to item indices with robin hood linear probing. Lookups touch the 8 byte slot array until they
// find a fingerprint match, then touch the item once to compare the key. Iteration walks the items array directly.
//
// The hash function is a compile time property of the item type (Item::hashf) so it inlines in to lookups rather
// than going through a function pointer.
//
// Pointer validity: inserting can grow the items array and invalidate all item pointers. Erasing moves the last item
// into the erased item's spot, so only pointers to the erased item and the last item are invalidated.
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

    u64 seed0{};
    u64 seed1{};
    float load_factor{0.0f};
    // Item count at which the next insert grows the slot array
    sizet grow_at{0};
    // Right shift applied to a mixed hash to get its home slot index
    u8 shift{64};
    array<Item> items{};
    array<hash_slot> slots{};
};

// Murmur3 style finalizer. The integral hash functions are identity so this is what actually spreads the bits.
inline u64 hash_table_mix(u64 h)
{
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ull;
    h ^= h >> 33;
    return h;
}

inline u32 hash_slot_dist_and_fp(u64 h)
{
    return HASH_SLOT_DIST_INC | ((u32)h & HASH_SLOT_FP_MASK);
}

template<typename Item>
inline u64 hash_table_hash(const hash_table<Item> *hc, const typename hash_table<Item>::hash_key_type &k)
{
    return hash_table_mix(Item::hashf(k, hc->seed0, hc->seed1));
}

template<typename Item>
inline sizet hash_table_home_slot(const hash_table<Item> *hc, u64 h)
{
    return (sizet)(h >> hc->shift);
}

template<typename Item>
inline sizet hash_table_next_slot(const hash_table<Item> *hc, sizet si)
{
    return (si + 1) & (hc->slots.size - 1);
}

template<typename Item>
sizet hash_table_count(const hash_table<Item> *hc)
{
    return hc->items.size;
}

// Returns the index in to the items array of the item with key k, or INVALID_ID if not found
template<typename Item>
sizet hash_table_find_index(const hash_table<Item> *hc, const typename hash_table<Item>::hash_key_type &k)
{
    if (hc->slots.size == 0) {
        return INVALID_ID;
    }
    u64 h = hash_table_hash(hc, k);
    u32 dfp = hash_slot_dist_and_fp(h);
    sizet si = hash_table_home_slot(hc, h);
    const hash_slot *slots = hc->slots.data;
    const Item *items = hc->items.data;
    while (true) {
        u32 sdfp = slots[si].dist_and_fp;
        if (dfp == sdfp && hash_table_item_match(items[slots[si].idx], k)) {
            return slots[si].idx;
        }
        // Robin hood ordering: if we would have displaced this slot on insert, the key is not in the table. This also
        // catches empty slots since their dist_and_fp is zero.
        if (dfp > sdfp) {
            return INVALID_ID;
        }
        dfp += HASH_SLOT_DIST_INC;
        si = hash_table_next_slot(hc, si);
    }
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_find(hash_table<Item> *hc, const typename hash_table<Item>::hash_key_type &k)
{
    sizet ind = hash_table_find_index(hc, k);
    if (ind != INVALID_ID) {
        return &hc->items.data[ind];
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_find(const hash_table<Item> *hc, const typename hash_table<Item>::hash_key_type &k)
{
    sizet ind = hash_table_find_index(hc, k);
    if (ind != INVALID_ID) {
        return &hc->items.data[ind];
    }
    return nullptr;
}

// Place slot s at index si, shifting any richer (closer to home) entries forward until an empty slot absorbs the chain
template<typename Item>
void hash_table_place_slot(hash_table<Item> *hc, hash_slot s, sizet si)
{
    hash_slot *slots = hc->slots.data;
    while (slots[si].dist_and_fp != 0) {
        hash_slot tmp = slots[si];
        slots[si] = s;
        s = tmp;
        s.dist_and_fp += HASH_SLOT_DIST_INC;
        si = hash_table_next_slot(hc, si);
    }
    slots[si] = s;
}

// Rebuild the slot array with at least new_size slots (rounded up to a power of two and never so small that the
// current items exceed the load factor). Items are not moved so item pointers stay valid.
template<typename Item>
void hash_table_rehash(hash_table<Item> *hc, sizet new_size)
{
    sizet needed = (sizet)((float)hc->items.size / hc->load_factor) + 1;
    if (new_size < needed) {
        new_size = needed;
    }
    sizet n = HASH_TABLE_MIN_SLOT_COUNT;
    u8 bits = 2;
    while (n < new_size) {
        n <<= 1;
        ++bits;
    }

    arr_resize(&hc->slots, n);
    arr_clear_to(&hc->slots, hash_slot{});
    hc->shift = (u8)(64 - bits);
    hc->grow_at = (sizet)((float)n * hc->load_factor);
    // Always leave at least one empty slot so probes terminate
    if (hc->grow_at >= n) {
        hc->grow_at = n - 1;
    }

    // Keys are known to be unique here so there is no key compare, but each still has to probe past any entries
    // that sort ahead of it to keep the slot ordering that find relies on for early exit
    for (sizet i = 0; i < hc->items.size; ++i) {
        u64 h = hash_table_hash(hc, hash_table_item_key(hc->items.data[i]));
        u32 dfp = hash_slot_dist_and_fp(h);
        sizet si = hash_table_home_slot(hc, h);
        while (dfp < hc->slots.data[si].dist_and_fp) {
            dfp += HASH_SLOT_DIST_INC;
            si = hash_table_next_slot(hc, si);
        }
        hash_table_place_slot(hc, {dfp, (u32)i}, si);
    }
}

template<typename Item>
bool hash_table_should_rehash_on_insert(const hash_table<Item> *hc)
{
    return hc->items.size >= hc->grow_at;
}

template<typename Item, typename Key, typename Val>
typename hash_table<Item>::iterator hash_table_insert_or_set(hash_table<Item> *hc, const Key &k, const Val &val, bool set_if_exists)
{
    if (hc->slots.size == 0) {
        return nullptr;
    }
    if (hash_table_should_rehash_on_insert(hc)) {
        hash_table_rehash(hc, hc->slots.size * 2);
    }

    u64 h = hash_table_hash(hc, k);
    u32 dfp = hash_slot_dist_and_fp(h);
    sizet si = hash_table_home_slot(hc, h);
    hash_slot *slots = hc->slots.data;

    // Walk the probe sequence while the entries there are at least as far from home as we are - our key can only be
    // among those. Stop at the first slot we would displace.
    while (dfp <= slots[si].dist_and_fp) {
        if (dfp == slots[si].dist_and_fp && hash_table_item_match(hc->items.data[slots[si].idx], k)) {
            Item *existing = &hc->items.data[slots[si].idx];
            if (set_if_exists) {
                set_hash_table_item_value(*existing, k, val);
                return existing;
            }
            return nullptr;
        }
        dfp += HASH_SLOT_DIST_INC;
        si = hash_table_next_slot(hc, si);
    }

    asrt(hc->items.size < 0xFFFFFFFFu);
    u32 idx = (u32)hc->items.size;
    Item *item = arr_emplace_back(&hc->items);
    set_hash_table_item_value(*item, k, val);
    hash_table_place_slot(hc, {dfp, idx}, si);
    return item;
}

template<typename Item>
void hash_table_init(hash_table<Item> *hc, mem_arena *arena, sizet initial_capacity, float load_factor)
{
    hc->seed0 = generate_rand_seed();
    hc->seed1 = generate_rand_seed();
    if (!(load_factor > 0.0f) || load_factor > 1.0f) {
        load_factor = HASH_TABLE_DEFAULT_LOAD_FACTOR;
    }
    hc->load_factor = load_factor;
    arr_init(&hc->items, arena, 0);
    arr_init(&hc->slots, arena, 0);
    hash_table_rehash(hc, initial_capacity);
    arr_reserve(&hc->items, hc->grow_at);
}

template<typename Item>
float hash_table_load_factor(const hash_table<Item> *hc, sizet entry_count)
{
    if (hc->slots.size == 0) {
        return 0.0f;
    }
    return (float)entry_count / (float)hc->slots.size;
}

template<typename Item>
float hash_table_current_load_factor(const hash_table<Item> *hc)
{
    return hash_table_load_factor(hc, hc->items.size);
}

// Remove the item at index idx from the items array. The last item is moved in to idx to keep the array dense.
template<typename Item>
void hash_table_erase_index(hash_table<Item> *hc, sizet idx)
{
    asrt(idx < hc->items.size);
    hash_slot *slots = hc->slots.data;

    // Find the slot pointing at idx - it is somewhere along the probe sequence from the item's home slot
    u64 h = hash_table_hash(hc, hash_table_item_key(hc->items.data[idx]));
    sizet si = hash_table_home_slot(hc, h);
    while (slots[si].idx != idx || slots[si].dist_and_fp == 0) {
        si = hash_table_next_slot(hc, si);
    }

    // Backward shift: pull every following entry that is not in its home slot back one step
    sizet nsi = hash_table_next_slot(hc, si);
    while (slots[nsi].dist_and_fp >= 2 * HASH_SLOT_DIST_INC) {
        slots[si] = {slots[nsi].dist_and_fp - HASH_SLOT_DIST_INC, slots[nsi].idx};
        si = nsi;
        nsi = hash_table_next_slot(hc, nsi);
    }
    slots[si] = {};

    // Swap remove from the items array, then repoint the moved item's slot
    sizet last = hc->items.size - 1;
    if (idx != last) {
        hc->items.data[idx] = hc->items.data[last];
        u64 mh = hash_table_hash(hc, hash_table_item_key(hc->items.data[idx]));
        sizet msi = hash_table_home_slot(hc, mh);
        while (slots[msi].idx != last || slots[msi].dist_and_fp == 0) {
            msi = hash_table_next_slot(hc, msi);
        }
        slots[msi].idx = (u32)idx;
    }
    arr_pop_back(&hc->items);
}

template<typename Item>
bool hash_table_remove(hash_table<Item> *hc, const typename hash_table<Item>::hash_key_type &k)
{
    sizet ind = hash_table_find_index(hc, k);
    if (ind == INVALID_ID) {
        return false;
    }
    hash_table_erase_index(hc, ind);
    return true;
}

// Erase item and return the item now occupying its position (the previously last item), or null if it was the last.
// Iterating forward and erasing with the returned pointer as the new current item visits every item exactly once.
template<typename Item, typename IteratorT>
IteratorT hash_table_erase(hash_table<Item> *hc, IteratorT item)
{
    if (!item) {
        return nullptr;
    }
    sizet idx = (sizet)(item - hc->items.data);
    asrt(idx < hc->items.size);
    if (idx >= hc->items.size) {
        return nullptr;
    }
    hash_table_erase_index(hc, idx);
    if (idx < hc->items.size) {
        return &hc->items.data[idx];
    }
    return nullptr;
}

template<typename Item>
bool hash_table_empty(const hash_table<Item> *hc)
{
    return hc->items.size == 0;
}

template<typename Item>
void hash_table_clear(hash_table<Item> *hc)
{
    arr_clear(&hc->items);
    arr_clear_to(&hc->slots, hash_slot{});
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_begin(hash_table<Item> *hc)
{
    if (hc->items.size > 0) {
        return hc->items.data;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_begin(const hash_table<Item> *hc)
{
    if (hc->items.size > 0) {
        return hc->items.data;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_rbegin(hash_table<Item> *hc)
{
    if (hc->items.size > 0) {
        return &hc->items.data[hc->items.size - 1];
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_rbegin(const hash_table<Item> *hc)
{
    if (hc->items.size > 0) {
        return &hc->items.data[hc->items.size - 1];
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_next(hash_table<Item> *hc, typename hash_table<Item>::iterator item)
{
    if (!item) {
        return hash_table_begin(hc);
    }
    if (item + 1 < hc->items.data + hc->items.size) {
        return item + 1;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_next(const hash_table<Item> *hc, typename hash_table<Item>::const_iterator item)
{
    if (!item) {
        return hash_table_begin(hc);
    }
    if (item + 1 < hc->items.data + hc->items.size) {
        return item + 1;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::iterator hash_table_prev(hash_table<Item> *hc, typename hash_table<Item>::iterator item)
{
    if (!item) {
        return hash_table_rbegin(hc);
    }
    if (item > hc->items.data) {
        return item - 1;
    }
    return nullptr;
}

template<typename Item>
typename hash_table<Item>::const_iterator hash_table_prev(const hash_table<Item> *hc, typename hash_table<Item>::const_iterator item)
{
    if (!item) {
        return hash_table_rbegin(hc);
    }
    if (item > hc->items.data) {
        return item - 1;
    }
    return nullptr;
}

template<typename Item>
void hash_table_terminate(hash_table<Item> *hc)
{
    arr_terminate(&hc->items);
    arr_terminate(&hc->slots);
}

} // namespace nslib
