#pragma once

#include "../atomic_types.h"

namespace nslib
{

// Holds N entries. The ring has to keep one slot open to tell full apart from empty, so the backing storage is
// N + 1 - that way the usable capacity matches the N the caller asked for.
template<typename T, sizet N>
struct spsc_queue
{
    static inline constexpr sizet capacity = N;
    static inline constexpr sizet slot_count = N + 1;

    T entries[slot_count];
    atomic_sizet write_index = 0;
    atomic_sizet read_index = 0;
};

template<typename T, sizet N>
b8 spsc_push(spsc_queue<T, N> *q, const T &value)
{
    size_t write = q->write_index.load(std::memory_order_relaxed);
    size_t next = (write + 1) % q->slot_count;

    if (next == q->read_index.load(std::memory_order_acquire)) {
        return false; // full
    }
    q->entries[write] = value;
    q->write_index.store(next, std::memory_order_release);
    return true;
}

template<typename T, sizet N>
b8 spsc_pop(spsc_queue<T, N> *q, T *value = nullptr)
{
    size_t read = q->read_index.load(std::memory_order_relaxed);
    if (read == q->write_index.load(std::memory_order_acquire)) return false; // empty
    if (value) *value = q->entries[read];
    q->read_index.store((read + 1) % q->slot_count, std::memory_order_release);
    return true;
}
} // namespace nslib
