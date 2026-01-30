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
        u64 id;
        struct
        {
            u32 index;
            u32 generation;
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
    return h.generation != 0;
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
struct slot_item_ref {
    slot_handle<T> hndl;
    T *item;
};

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
    u32 used_count{};
};

template<typename T>
void init_slot_pool(slot_pool<T> *pool, u32 elements, mem_arena *arena)
{
    arr_init(&pool->slots, arena, elements);
    arr_init(&pool->free_list, arena, elements);
    pool->used_count = 0;
}

template<typename T>
void terminate_slot_pool(slot_pool<T> *pool)
{
    arr_terminate(&pool->slots);
    arr_terminate(&pool->free_list);
    pool->used_count = 0;
}

template<typename T>
void clear_slot_pool(slot_pool<T> *pool)
{
    arr_clear(&pool->slots);
    arr_clear(&pool->free_list);
    pool->used_count = 0;
}

template<typename T>
bool is_slot_available(const slot_pool<T> *pool)
{
    return (pool->free_list.size > 0) || (pool->slots.size < pool->slots.capacity);
}

template<typename T>
u32 get_slot_used_count(const slot_pool<T> *pool)
{
    asrt(pool);
    return pool->used_count;
}

template<typename T>
bool slot_pool_empty(const slot_pool<T> *pool)
{
    asrt(pool);
    return pool->used_count == 0;
}

template<typename T>
slot_handle<T> get_slot_current_handle(slot_pool<T> *pool, u32 index)
{
    if (index >= pool->slots.size) {
        return {};
    }
    return {.index = index, .generation = pool->slots[index].gen_id};
}

template<typename T>
slot_item_ref<T> acquire_slot(slot_pool<T> *pool, const T &item = {})
{
    slot_item_ref<T> ret{};
    if (!is_slot_available(pool)) {
        return ret;
    }

    // If there is a slot available on the free list use it restoring the gen id. Otherwise, add an item to slots. In
    // either case increment the slot item gen id: for new items it will be 1 and for reused it will be one higher than
    // whatever was placed in the free list
    slot_pool_item<T> *slot_item{};
    auto fl_entry = arr_back(&pool->free_list);
    if (fl_entry) {
        ret.hndl.index = fl_entry->handle.index;
        slot_item = &pool->slots[ret.hndl.index];
        slot_item->gen_id = fl_entry->handle.generation;
        arr_pop_back(&pool->free_list);
    }
    else {
        ret.hndl.index = (u32)pool->slots.size;
        arr_push_back(&pool->slots, {});
        slot_item = arr_back(&pool->slots);
    }
    slot_item->item = item;
    ++slot_item->gen_id;
    ret.item = &slot_item->item;
    ret.hndl.generation = slot_item->gen_id;
    ++pool->used_count;
    return ret;
}

template<typename T>
T *get_slot_item(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle) || handle.index >= pool->slots.size) {
        return nullptr;
    }
    auto *entry = &pool->slots[handle.index];
    if (handle.generation == entry->gen_id) {
        return &entry->item;
    }
    return nullptr;
}

template<typename T>
const T *get_slot_item(const slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle) || handle.index >= pool->slots.size) {
        return nullptr;
    }
    auto *entry = &pool->slots[handle.index];
    if (handle.generation == entry->gen_id) {
        return &entry->item;
    }
    return nullptr;
}

template<typename T>
bool release_slot(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle) || handle.index >= pool->slots.size) {
        return false;
    }
    // Add the handle to our free list
    slot_free_entry<T> free_entry{.handle{handle}};
    arr_push_back(&pool->free_list, free_entry);

    // Set gen id to 0 to indicate this slot isn't used
    auto *entry = &pool->slots.data[handle.index];
    entry->gen_id = 0;
    asrt(pool->used_count > 0);
    --pool->used_count;
    return true;
}

template<typename T>
slot_pool<T>::iterator slot_pool_next(slot_pool<T> *pool, typename slot_pool<T>::iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index + 1;
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
slot_pool<T>::const_iterator slot_pool_next(const slot_pool<T> *pool, typename slot_pool<T>::const_iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index + 1;
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
slot_pool<T>::iterator slot_pool_prev(slot_pool<T> *pool, typename slot_pool<T>::iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index - 1;
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
slot_pool<T>::const_iterator slot_pool_prev(const slot_pool<T> *pool, typename slot_pool<T>::const_iterator iter)
{
    asrt(pool);
    u32 ind = iter.hndl.index - 1;
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
slot_pool<T>::iterator slot_pool_begin(slot_pool<T> *pool)
{
    asrt(pool);
    slot_item_ref<T> tmp_ref{.hndl{.index= (u32)-1}};
    return slot_pool_next(pool, tmp_ref);
}

template<typename T>
slot_pool<T>::const_iterator slot_pool_begin(const slot_pool<T> *pool)
{
    asrt(pool);
    slot_item_ref<T> tmp_ref{.hndl{.index= (u32)-1}};
    return slot_pool_next(pool, tmp_ref);
}

template<typename T>
slot_pool<T>::iterator slot_pool_rbegin(slot_pool<T> *pool)
{
    asrt(pool);
    slot_item_ref<T> tmp_ref{.hndl{.index=(u32)pool->slots.size}};
    return slot_pool_prev(pool, tmp_ref);
}

template<typename T>
slot_pool<T>::const_iterator slot_pool_rbegin(const slot_pool<T> *pool)
{
    asrt(pool);
    slot_item_ref<T> tmp_ref{.hndl{.index=(u32)pool->slots.size}};
    return slot_pool_prev(pool, tmp_ref);
}

} // namespace nslib
