#pragma once
#include "../basic_types.h"
#include "array.h"

namespace nslib
{

template<typename T>
struct slot_handle
{
    union
    {
        // Combined id
        u64 id;
        struct
        {
            // Slot index
            idx_t si;
            // Generation id
            u32 gen_id;
        };
    };
};

op_eq_func_tt(slot_handle)
{
    return lhs.id == rhs.id;
}

op_neq_func_tt(slot_handle);

template<typename T>
bool is_valid(slot_handle<T> h)
{
    return h.gen_id != 0;
}

template<typename T>
struct slot_pool_item
{
    T item{};
    u32 gen_id{};
};

template<typename T>
struct slot_free_entry
{
    slot_handle<T> handle;
};

template<typename T>
struct slot_item_ref
{
    slot_handle<T> hndl;
    T *item;
};

op_eq_func_tt(slot_item_ref)
{
    return lhs.item == rhs.item && lhs.hndl == rhs.hndl;
}

op_neq_func_tt(slot_item_ref);

template<typename T>
bool is_valid(const slot_item_ref<T> &ref)
{
    return is_valid(ref.hndl) && ref.item;
}

// A fixed capacity pool of slots addressed by generation checked handles.
//
// The pool is split in two halves so that handle allocation and item storage can live on different threads with no
// locking. Every field has exactly one writer:
//
//   Allocation half (free_list, reserved_count) - written only by reserve_slot / free_slot. Hands out and takes
//   back handles. Never reads or writes slots[].
//
//   Storage half (slots) - written only by place_slot / clear_slot. Holds each slot's item and the generation that
//   item is valid for. Never reads or writes the free list or reserved_count.
//
// slots is sized to capacity at init and never resized again, so slots.size is immutable and safe to read from
// either thread. Free list entries carry the generation that was live when the slot was freed, which is how
// reserve_slot mints the next generation without looking at slots[].
//
// Two thread use: the handle owning thread (sim) calls reserve_slot / free_slot and sends the handle across an
// ordered channel. The item owning thread (render) calls place_slot / clear_slot when it reads the message. Because
// the channel is ordered, a free followed by a reserve of the same slot always arrives as clear-then-place, which
// is what the gen_id == 0 assertion in place_slot checks.
//
// Single thread use: acquire_slot (reserve + place) and release_slot (clear + free) behave exactly as one would
// expect from a plain slot pool.
template<typename T>
struct slot_pool
{
    using iterator = slot_item_ref<T>;
    using const_iterator = slot_item_ref<const T>;

    // Storage half
    array<slot_pool_item<T>> slots{};

    // Allocation half
    array<slot_free_entry<T>> free_list{};
    // Number of slots that have been reserved at least once - also the index of the next never used slot
    u32 reserved_count{};
};

template<typename T>
void init_slot_pool(slot_pool<T> *pool, u32 elements, mem_arena *arena)
{
    arr_init(&pool->slots, arena, elements);
    // Size to capacity now so slots.size never changes again. Value init leaves every gen_id at 0.
    arr_resize(&pool->slots, elements);
    arr_init(&pool->free_list, arena, elements);
    pool->reserved_count = 0;
}

template<typename T>
void terminate_slot_pool(slot_pool<T> *pool)
{
    arr_terminate(&pool->slots);
    arr_terminate(&pool->free_list);
    pool->reserved_count = 0;
}

// Resets every slot to unused without changing capacity. Touches both halves - single thread only.
template<typename T>
void clear_slot_pool(slot_pool<T> *pool)
{
    for (sizet i = 0; i < pool->slots.size; ++i) {
        pool->slots[i] = {};
    }
    arr_clear(&pool->free_list);
    pool->reserved_count = 0;
}

template<typename T>
u32 get_slot_capacity(const slot_pool<T> &pool)
{
    return (u32)pool.slots.size;
}

// Allocation half
template<typename T>
u32 get_slot_used_count(const slot_pool<T> &pool)
{
    return pool.reserved_count - (u32)pool.free_list.size;
}

// Allocation half
template<typename T>
u32 get_slots_available_count(const slot_pool<T> &pool)
{
    return get_slot_capacity(pool) - get_slot_used_count(pool);
}

// Allocation half
template<typename T>
bool is_slot_available(const slot_pool<T> &pool)
{
    return get_slots_available_count(pool) > 0;
}

// Allocation half
template<typename T>
bool slot_pool_empty(const slot_pool<T> &pool)
{
    return get_slot_used_count(pool) == 0;
}

// Storage half
template<typename T>
slot_handle<T> get_slot_current_handle(slot_pool<T> *pool, u32 index)
{
    if (index >= pool->slots.size) {
        return {};
    }
    return {.si = index, .gen_id = pool->slots[index].gen_id};
}

// Storage half
template<typename T>
slot_handle<const T> get_slot_current_handle(const slot_pool<T> &pool, u32 index)
{
    if (index >= pool.slots.size) {
        return {};
    }
    return {.si = index, .gen_id = pool.slots[index].gen_id};
}

// Allocation half. Mints a handle for an unused slot. Returns an invalid handle if the pool is full. Does not touch
// slots[] - the slot is not usable until place_slot is called with the returned handle.
template<typename T>
slot_handle<T> reserve_slot(slot_pool<T> *pool)
{
    if (!is_slot_available(*pool)) {
        return {};
    }

    // Reuse the most recently freed slot if there is one, restoring the generation it was freed with. Otherwise take
    // the next never used slot at generation 0. Either way the returned generation is one higher.
    slot_handle<T> ret{};
    auto fl_entry = arr_back(&pool->free_list);
    if (fl_entry) {
        ret = fl_entry->handle;
        arr_pop_back(&pool->free_list);
    }
    else {
        asrt(pool->reserved_count < pool->slots.size);
        ret.si = pool->reserved_count++;
    }
    ++ret.gen_id;
    return ret;
}

// Storage half. Stores item at the slot named by handle and stamps it with the handle's generation. The slot must
// currently be unused - in two thread use this means the clear for the previous occupant has already been applied.
template<typename T>
T *place_slot(slot_pool<T> *pool, slot_handle<T> handle, const T &item = {})
{
    asrt(is_valid(handle));
    asrt(handle.si < pool->slots.size);
    auto *entry = &pool->slots[handle.si];
    asrt(entry->gen_id == 0);
    entry->item = item;
    entry->gen_id = handle.gen_id;
    return &entry->item;
}

// Both halves - single thread only. reserve_slot followed by place_slot.
template<typename T>
slot_item_ref<T> acquire_slot(slot_pool<T> *pool, const T &item = {})
{
    slot_item_ref<T> ret{};
    ret.hndl = reserve_slot(pool);
    if (!is_valid(ret.hndl)) {
        return ret;
    }
    ret.item = place_slot(pool, ret.hndl, item);
    return ret;
}

// Storage half
template<typename T>
T *get_slot_item(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle) || handle.si >= pool->slots.size) {
        return nullptr;
    }
    auto *entry = &pool->slots[handle.si];
    if (handle.gen_id == entry->gen_id) {
        return &entry->item;
    }
    return nullptr;
}

// Storage half
template<typename T>
const T *get_slot_item(const slot_pool<T> &pool, slot_handle<T> handle)
{
    if (!is_valid(handle) || handle.si >= pool.slots.size) {
        return nullptr;
    }
    auto *entry = &pool.slots[handle.si];
    if (handle.gen_id == entry->gen_id) {
        return &entry->item;
    }
    return nullptr;
}

// Storage half. Marks the slot unused (gen_id 0) if handle matches the current occupant. Does not return the slot
// to the free list - that is free_slot's job.
template<typename T>
bool clear_slot(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle) || handle.si >= pool->slots.size || handle.gen_id != pool->slots[handle.si].gen_id) {
        return false;
    }
    pool->slots[handle.si].gen_id = 0;
    return true;
}

// Allocation half. Returns the handle's slot to the free list so reserve_slot can hand it out again. Cannot check
// the handle against the slot's current generation (that is the storage half) so it trusts the caller - freeing a
// handle twice, or one that was never reserved, corrupts the free list.
template<typename T>
bool free_slot(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle) || handle.si >= pool->slots.size) {
        return false;
    }
    asrt(pool->free_list.size < pool->free_list.capacity);
    slot_free_entry<T> free_entry{.handle{handle}};
    arr_push_back(&pool->free_list, free_entry);
    return true;
}

// Both halves - single thread only. clear_slot followed by free_slot.
template<typename T>
bool release_slot(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!clear_slot(pool, handle)) {
        return false;
    }
    return free_slot(pool, handle);
}

// Iteration - storage half
template<typename T>
slot_pool<T>::iterator slot_pool_next(slot_pool<T> *pool, typename slot_pool<T>::iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.si + 1;
    while (ind < pool->slots.size) {
        auto hndl = get_slot_current_handle(pool, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool->slots[ind].item};
        }
        ++ind;
    }
    return {};
}

template<typename T>
slot_pool<T>::const_iterator slot_pool_next(const slot_pool<T> &pool, typename slot_pool<T>::const_iterator iter)
{
    u32 ind = iter.hndl.si + 1;
    while (ind < pool.slots.size) {
        auto hndl = get_slot_current_handle(pool, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool.slots[ind].item};
        }
        ++ind;
    }
    return {};
}

template<typename T>
slot_pool<T>::iterator slot_pool_prev(slot_pool<T> *pool, typename slot_pool<T>::iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.si - 1;
    // We utilize u32 wrapping here
    while (ind < pool->slots.size) {
        auto hndl = get_slot_current_handle(pool, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool->slots[ind].item};
        }
        --ind;
    }
    return {};
}

template<typename T>
slot_pool<T>::const_iterator slot_pool_prev(const slot_pool<T> &pool, typename slot_pool<T>::const_iterator iter)
{
    u32 ind = iter.hndl.si - 1;
    // We utilize u32 wrapping here
    while (ind < pool.slots.size) {
        auto hndl = get_slot_current_handle(pool, ind);
        if (is_valid(hndl)) {
            return {.hndl = hndl, .item = &pool.slots[ind].item};
        }
        --ind;
    }
    return {};
}

template<typename T>
slot_pool<T>::iterator slot_pool_begin(slot_pool<T> *pool)
{
    asrt(pool);
    slot_item_ref<T> tmp_ref{.hndl{.si = (u32)-1}};
    return slot_pool_next(pool, tmp_ref);
}

template<typename T>
slot_pool<T>::const_iterator slot_pool_begin(const slot_pool<T> &pool)
{
    slot_item_ref<T> tmp_ref{.hndl{.si = (u32)-1}};
    return slot_pool_next(pool, tmp_ref);
}

template<typename T>
slot_pool<T>::iterator slot_pool_rbegin(slot_pool<T> *pool)
{
    asrt(pool);
    slot_item_ref<T> tmp_ref{.hndl{.si = (u32)pool->slots.size}};
    return slot_pool_prev(pool, tmp_ref);
}

template<typename T>
slot_pool<T>::const_iterator slot_pool_rbegin(const slot_pool<T> &pool)
{
    slot_item_ref<T> tmp_ref{.hndl{.si = (u32)pool.slots.size}};
    return slot_pool_prev(pool, tmp_ref);
}

} // namespace nslib
