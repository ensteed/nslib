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

template<typename T>
struct slot_pool
{
    using iterator = slot_item_ref<T>;
    using const_iterator = slot_item_ref<const T>;

    // Slots store user data, generation tracking, and usage info alongside an explicit free list.
    array<slot_pool_item<T>> slots{};
    array<slot_free_entry<T>> free_list{};
};

template<typename T>
void init_slot_pool(slot_pool<T> *pool, u32 elements, mem_arena *arena)
{
    arr_init(&pool->slots, arena, elements);
    arr_init(&pool->free_list, arena, elements);
}

template<typename T>
void terminate_slot_pool(slot_pool<T> *pool)
{
    arr_terminate(&pool->slots);
    arr_terminate(&pool->free_list);
}

template<typename T>
void clear_slot_pool(slot_pool<T> *pool)
{
    arr_clear(&pool->slots);
    arr_clear(&pool->free_list);
}

template<typename T>
u32 get_slot_used_count(const slot_pool<T> &pool)
{
    return (u32)pool.slots.size - (u32)pool.free_list.size;
}

template<typename T>
u32 get_slots_available_count(const slot_pool<T> &pool)
{
    return (u32)pool.slots.capacity - get_slot_used_count(pool);
}

template<typename T>
bool is_slot_available(const slot_pool<T> &pool)
{
    return get_slots_available_count(pool) > 0;
}

template<typename T>
bool slot_pool_empty(const slot_pool<T> &pool)
{
    return pool.slots.size == pool.free_list.size;
}

template<typename T>
slot_handle<T> get_slot_current_handle(slot_pool<T> *pool, u32 index)
{
    if (index >= pool->slots.size) {
        return {};
    }
    return {.si = index, .gen_id = pool->slots[index].gen_id};
}

template<typename T>
slot_handle<const T> get_slot_current_handle(const slot_pool<T> &pool, u32 index)
{
    if (index >= pool.slots.size) {
        return {};
    }
    return {.si = index, .gen_id = pool.slots[index].gen_id};
}

template<typename T>
slot_item_ref<T> acquire_slot(slot_pool<T> *pool, const T &item = {})
{
    slot_item_ref<T> ret{};
    if (!is_slot_available(*pool)) {
        return ret;
    }

    // If there is a slot available on the free list use it restoring the gen id. Otherwise, add an item to slots. In
    // either case increment the slot item gen id: for new items it will be 1 and for reused it will be one higher than
    // whatever was placed in the free list
    slot_pool_item<T> *slot_item{};
    auto fl_entry = arr_back(&pool->free_list);
    if (fl_entry) {
        ret.hndl.si = fl_entry->handle.si;
        slot_item = &pool->slots[ret.hndl.si];
        slot_item->gen_id = fl_entry->handle.gen_id;
        arr_pop_back(&pool->free_list);
    }
    else {
        ret.hndl.si = (u32)pool->slots.size;
        arr_push_back(&pool->slots, {});
        slot_item = arr_back(&pool->slots);
    }
    slot_item->item = item;
    ++slot_item->gen_id;
    ret.item = &slot_item->item;
    ret.hndl.gen_id = slot_item->gen_id;
    return ret;
}

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

template<typename T>
bool release_slot(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (handle.si >= pool->slots.size || handle.gen_id != pool->slots[handle.si].gen_id) {
        return false;
    }
    // Add the handle to our free list
    slot_free_entry<T> free_entry{.handle{handle}};
    arr_push_back(&pool->free_list, free_entry);

    // Set gen id to 0 to indicate this slot isn't used
    auto *entry = &pool->slots.data[handle.si];
    entry->gen_id = 0;
    return true;
}

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
    asrt(pool);
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
