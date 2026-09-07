#pragma once

#include "../atomic_types.h"

namespace nslib
{

template<typename T, sizet N>
struct spsc_queue
{
    T entries[N];
    atomic_sizet write_index = 0;
    atomic_sizet read_index = 0;
};

template<typename T, sizet N>
b8 spsc_push(spsc_queue<T, N> *q, const T &value)
{
    size_t write = q->write_index.load(std::memory_order_relaxed);
    size_t next = (write + 1) % N;

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
    q->read_index.store((read + 1) % N, std::memory_order_release);
    return true;
}
} // namespace nslib
