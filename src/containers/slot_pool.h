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
    return h.id != 0;
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
struct slot_pool
{
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
bool is_slot_available(const slot_pool<T> *pool)
{
    return (pool->free_list.size > 0) || (pool->slots.size < pool->slots.capacity);
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
slot_handle<T> acquire_slot(slot_pool<T> *pool, const T &item = {})
{
    slot_handle<T> ret{};
    if (!is_slot_available(pool)) {
        return ret;
    }

    // If there is a slot available on the free list use it restoring the gen id. Otherwise, add an item to slots. In
    // either case increment the slot item gen id: for new items it will be 1 and for reused it will be one higher than
    // whatever was placed in the free list
    slot_pool_item<T> *slot_item{};
    auto fl_entry = arr_back(&pool->free_list);
    if (fl_entry) {
        ret.index = fl_entry->handle.index;
        slot_item = &pool->slots[ret.index];
        slot_item->gen_id = fl_entry->handle.generation;
        arr_pop_back(&pool->free_list);
    }
    else {
        ret.index = (u32)pool->slots.size;
        arr_push_back(&pool->slots, {});
        slot_item = arr_back(&pool->slots);
    }
    slot_item->item = item;
    ++slot_item->gen_id;
    ret.generation = slot_item->gen_id;
    return ret;
}

template<typename T>
T *get_slot_item(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle)) {
        return nullptr;
    }
    asrt(handle.index < pool->slots.size);
    auto *entry = &pool->slots[handle.index];
    if (handle.generation == entry->gen_id) {
        return &entry->item;
    }
    return nullptr;
}

template<typename T>
const T *get_slot_item(const slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle)) {
        return nullptr;
    }
    asrt(handle.index < pool->slots.size);
    auto *entry = &pool->slots[handle.index];
    if (handle.generation == entry->gen_id) {
        return &entry->item;
    }
    return nullptr;
}

template<typename T>
bool release_slot(slot_pool<T> *pool, slot_handle<T> handle)
{
    if (!is_valid(handle)) {
        return false;
    }
    asrt(handle.index < pool->slots.size);

    // Add the handle to our free list
    slot_free_entry<T> free_entry{.handle{handle}};
    arr_push_back(&pool->free_list, free_entry);

    // Set gen id to 0 to indicate this slot isn't used
    auto *entry = &pool->slots.data[handle.index];
    entry->gen_id = 0;
    return true;
}

} // namespace nslib
