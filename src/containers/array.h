#pragma once

#include <new> // IWYU pragma: keep
#include "../archive_common.h"
#include "../memory.h"

namespace nslib
{
template<typename T, sizet N>
struct static_array
{
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;
    static inline constexpr sizet capacity = N;

    sizet size{0};
    T data[N];

    inline const T &operator[](sizet ind) const
    {
        return data[ind];
    }
    inline T &operator[](sizet ind)
    {
        return data[ind];
    }
};

template<typename T>
struct array
{
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;

    sizet size{};
    sizet capacity{};
    T *data{};
    mem_arena *arena{};

    array(mem_arena *arena=nullptr, sizet initial_capacity = 0)
    {
        arr_init(this, arena, initial_capacity);
    }

    array(mem_arena *arena, const array &copy)
    {
        arr_init(this, arena, copy.capacity);
        arr_copy(this, &copy);
    }

    // Must be declared - without it the compiler emits a memberwise copy that shallow copies data,
    // and both arrays then free it. Adopts the source's arena, same as string(const string&).
    array(const array &copy)
    {
        arr_init(this, copy.arena, copy.capacity);
        arr_copy(this, &copy);
    }

    ~array()
    {
        arr_terminate(this);
    }

    array &operator=(const array &rhs)
    {
        // arr_resize value-initializes elements with no arena (nested containers, hmap items), and
        // those elements are then assigned into. Adopt the source's arena when we have none of our
        // own. If we already have one, we keep it - the copy lives where the destination lives.
        if (!arena) arena = rhs.arena;
        arr_copy(this, &rhs);
        return *this;
    }

    inline const T &operator[](sizet ind) const
    {
        return data[ind];
    }
    inline T &operator[](sizet ind)
    {
        return data[ind];
    }
};

template<typename T>
void swap(array<T> *lhs, array<T> *rhs)
{
    auto tmp_arena = lhs->arena;
    lhs->arena = rhs->arena;
    rhs->arena = tmp_arena;

    auto tmp_size = lhs->size;
    lhs->size = rhs->size;
    rhs->size = tmp_size;

    auto tmp_capacity = lhs->capacity;
    lhs->capacity = rhs->capacity;
    rhs->capacity = tmp_capacity;

    auto tmp_data = lhs->data;
    lhs->data = rhs->data;
    rhs->data = tmp_data;
}

template<typename T>
void arr_init(array<T> *arr, mem_arena *arena, sizet initial_capacity = 0)
{
    arr->arena = arena;
    arr_set_capacity(arr, initial_capacity);
}

template<typename T>
void arr_terminate(array<T> *arr)
{
    arr_set_capacity(arr, 0);
}

template<typename T>
typename T::iterator arr_begin(T *arrobj)
{
    return arrobj->data;
}

template<typename T>
sizet arr_len(const array<T> *arr)
{
    return arr->size;
}

template<typename T>
sizet arr_len(const array<T> &arr)
{
    return arr.size;
}

template<typename T>
sizet arr_sizeof(const array<T> *arr)
{
    return sizeof(T) * arr->size;
}

template<typename T>
sizet arr_sizeof(const array<T> &arr)
{
    return sizeof(T) * arr.size;
}

// Get the used byte size of the static array (the capacity is just N)
template<typename T, sizet N>
sizet arr_sizeof(const static_array<T, N> *arr)
{
    return sizeof(T) * arr->size;
}

// Get the used byte size of the static array (the capacity is just N)
template<typename T, sizet N>
sizet arr_sizeof(const static_array<T, N> &arr)
{
    return sizeof(T) * arr.size;
}

template<typename T>
typename T::iterator arr_end(T *arrobj)
{
    // NOTE: When data is null (no storage), this returns null. Callers must not do
    // pointer arithmetic or dereference on a null iterator. This is a deliberate
    // contract to avoid allocating sentinel storage for empty arrays.
    return arrobj->data + arrobj->size;
}

template<typename T>
typename T::const_iterator arr_begin(const T *arrobj)
{
    // NOTE: When data is null (no storage), this returns null. Callers must not do
    // pointer arithmetic or dereference on a null iterator. This is a deliberate
    // contract to avoid allocating sentinel storage for empty arrays.
    return arrobj->data;
}

template<typename T>
typename T::const_iterator arr_end(const T *arrobj)
{
    // NOTE: When data is null (no storage), this returns null. Callers must not do
    // pointer arithmetic or dereference on a null iterator. This is a deliberate
    // contract to avoid allocating sentinel storage for empty arrays.
    return arrobj->data + arrobj->size;
}

template<typename Arr1, typename Arr2>
void arr_copy(Arr1 *dest, const Arr2 *source)
{
    arr_copy(dest, source->data, source->size);
}

template<typename T, sizet N>
void arr_copy(static_array<T, N> *dest, const T *src, sizet src_size)
{
    asrt(src_size <= N);
    dest->size = src_size;
    for (sizet i = 0; i < dest->size; ++i) {
        (*dest)[i] = src[i];
    }
}

template<typename T>
void arr_copy(array<T> *dest, const T *src, sizet src_size)
{
    arr_resize(dest, src_size);
    for (sizet i = 0; i < dest->size; ++i) {
        (*dest)[i] = src[i];
    }
}

template<typename T>
void arr_append(array<T> *arr, const T *src, sizet src_size)
{
    sizet offset = arr->size;
    arr_resize(arr, offset + src_size);
    for (sizet i = 0; i < src_size; ++i) {
        (*arr)[offset + i] = src[i];
    }
}

template<typename T>
void arr_append(array<T> *arr, const array<T> *source)
{
    arr_append(arr, source->data, source->size);
}

template<typename T>
void arr_set_capacity(array<T> *arr, sizet new_cap)
{
    if (new_cap == arr->capacity) {
        return;
    }

    if (new_cap > 0) {
        // New cap can't be any smaller than mem_nod since we are using free list allocator
        while (new_cap * sizeof(T) < sizeof(mem_node)) {
            ++new_cap;
        }
    }

    sizet new_size = arr->size;
    if (new_cap < new_size) {
        new_size = new_cap;
    }

    T *new_data{};
    // NOTE: This always move-constructs into new storage. Types must be move-constructible.
    // Copy-only types will not compile; we intentionally do not provide a copy fallback here.
    if (new_cap > 0) {
        auto alignment = alignof(T);
        new_data = (T *)mem_alloc(new_cap * sizeof(T), arr->arena, alignment > DEFAULT_MIN_ALIGNMENT ? alignment : DEFAULT_MIN_ALIGNMENT);
        for (sizet i = 0; i < new_size; ++i) {
            new (&new_data[i]) T(static_cast<T &&>(arr->data[i]));
        }
    }

    if (arr->data) {
        for (sizet i = 0; i < arr->size; ++i) {
            arr->data[i].~T();
        }
        mem_free(arr->data, arr->arena);
    }

    arr->data = new_data;
    arr->capacity = new_cap;
    arr->size = new_size;
}

template<typename T>
void arr_reserve(array<T> *arr, sizet capacity)
{
    if (arr->capacity < capacity) {
        arr_set_capacity(arr, capacity);
    }
}

template<typename T>
void arr_shrink_to_fit(array<T> *arr)
{
    asrt(arr->size <= arr->capacity);
    if (arr->size < arr->capacity) {
        arr_set_capacity(arr, arr->size);
    }
}

template<typename T>
T *arr_push_back(array<T> *arr, const T &item)
{
    sizet sz = arr->size;
    arr_resize(arr, sz + 1);
    (*arr)[sz] = item;
    return &(*arr)[sz];
}

template<typename T, sizet N>
T *arr_push_back(static_array<T, N> *arr, const T &item)
{
    asrt(arr->size < arr->capacity);
    sizet sz = arr->size;
    ++arr->size;
    arr->data[sz] = item;
    return &arr->data[sz];
}

template<typename T, typename... Args>
T *arr_emplace_back(array<T> *arr, Args &&...args)
{
    sizet sz = arr->size;
    if (sz + 1 > arr->capacity) {
        sizet cap = arr->capacity;
        if (cap < 1) {
            cap = 1;
        }
        while (cap < sz + 1) {
            cap *= 2;
        }
        arr_set_capacity(arr, cap);
    }
    T *ret = &arr->data[sz];
    new (ret) T(static_cast<Args &&>(args)...);
    arr->size = sz + 1;
    return ret;
}

// Assigns each element in the array to item - does not change array size or capacity.
template<typename T>
void arr_clear_to(T *bufobj, const typename T::value_type &item)
{
    for (int i = 0; i < bufobj->size; ++i) {
        bufobj->data[i] = item;
    }
}

// Call destructor on all items in the array and set size to 0, does not affect the capacity.
template<typename T>
void arr_clear(array<T> *arr)
{
    for (int i = 0; i < arr->size; ++i) {
        arr->data[i].~T();
    }
    arr->size = 0;
}

template<typename T, sizet N>
void arr_clear(static_array<T, N> *arr)
{
    for (sizet i = 0; i < arr->size; ++i) {
        arr->data[i].~T();
    }
    arr->size = 0;
}

template<typename T>
void arr_pop_back(array<T> *arr)
{
    if (arr->size == 0) return;
    arr->data[arr->size - 1].~T();
    --arr->size;
}

template<typename T, sizet N>
void arr_pop_back(static_array<T, N> *arr)
{
    if (arr->size == 0) return;
    arr->data[arr->size - 1].~T();
    --arr->size;
}

template<typename T>
typename T::value_type *arr_back(T *bufobj)
{
    if (bufobj->size > 0) {
        return &bufobj->data[bufobj->size - 1];
    }
    return nullptr;
}

template<typename T>
typename T::value_type *arr_front(T *bufobj)
{
    if (bufobj->size > 0) {
        return &bufobj->data[0];
    }
    return nullptr;
}

template<typename T>
typename T::iterator arr_find(T *bufobj, const typename T::value_type &item)
{
    auto iter = arr_begin(bufobj);
    auto end = arr_end(bufobj);
    while (iter && iter != end) {
        if (*iter == item) {
            return iter;
        }
        ++iter;
    }
    return end;
}

template<typename T, typename... Args>
array<T> *arr_resize(array<T> *arr, sizet new_size, Args &&...args)
{
    if (arr->size == new_size) return arr;

    if (new_size < arr->size) {
        for (sizet i = new_size; i < arr->size; ++i) {
            arr->data[i].~T();
        }
        arr->size = new_size;
        return arr;
    }

    // Make sure our current size doesn't exceed the capacity - it shouldnt that would definitely be a bug if it did.
    asrt(arr->size <= arr->capacity);
    sizet cap = arr->capacity;
    if (new_size > cap) {
        if (cap < 1) {
            cap = 1;
        }
        while (cap < new_size)
            cap *= 2;
        arr_set_capacity(arr, cap);
    }
    for (sizet i = arr->size; i < new_size; ++i) {
        new (&arr->data[i]) T(static_cast<Args &&>(args)...);
    }
    arr->size = new_size;
    return arr;
}

template<typename T, sizet N, typename... Args>
static_array<T, N> *arr_resize(static_array<T, N> *arr, sizet new_size, Args &&...args)
{
    asrt(new_size <= N);
    if (new_size < arr->size) {
        for (sizet i = new_size; i < arr->size; ++i) {
            arr->data[i].~T();
        }
        arr->size = new_size;
        return arr;
    }
    for (sizet i = arr->size; i < new_size; ++i) {
        new (&arr->data[i]) T(static_cast<Args &&>(args)...);
    }
    arr->size = new_size;
    return arr;
}

template<typename T>
typename T::iterator arr_erase(T *bufobj, typename T::iterator iter)
{
    if (iter == arr_end(bufobj)) {
        return iter;
    }
    auto copy_iter = iter + 1;
    while (copy_iter != arr_end(bufobj)) {
        *(copy_iter - 1) = *copy_iter;
        ++copy_iter;
    }

    arr_pop_back(bufobj);
    return iter;
}

template<typename T>
typename T::iterator arr_erase(T *bufobj, typename T::iterator first, typename T::iterator last)
{
    sizet reduce_size = (last - first);
    if (reduce_size > bufobj->size || reduce_size == 0) {
        return last;
    }

    // Shift all items after the range over to the first item in the range, until we reach to end of the data
    while (last != arr_end(bufobj)) {
        *first = *last;
        ++first;
        ++last;
    }

    arr_resize(bufobj, bufobj->size - reduce_size);
    return first;
}

// Remove the item at index by copying the last item in the array to its spot and popping the last item. This does not
// preserve the order of the array.
template<typename T>
bool arr_swap_remove(T *bufobj, sizet index)
{
    if (index >= bufobj->size) return false;
    // Only copy the last item if we are not the last item
    if (index != bufobj->size - 1) bufobj->data[index] = *arr_back(bufobj);
    arr_pop_back(bufobj);
    return true;
}

// Remove the item at index by copying all items > index to their previous element, and then popping the last item.
template<typename T>
bool arr_remove(T *bufobj, sizet index)
{
    if (index >= bufobj->size) return false;

    // Copy the items back one spot
    for (sizet i = index + 1; i < bufobj->size; ++i) {
        bufobj->data[i - 1] = bufobj->data[i];
    }

    // Pop the last item
    arr_pop_back(bufobj);
    return true;
}

// Remove the item at index by copying all items > index to their previous element, and then popping the last item.
template<typename T>
sizet arr_remove(T *bufobj, const typename T::value_type &val)
{
    sizet removed{};
    sizet write_ind{};
    for (sizet read_ind = 0; read_ind < bufobj->size; ++read_ind) {
        if (bufobj->data[read_ind] == val) {
            bufobj->data[read_ind].~T();
            ++removed;
        }
        else {
            if (write_ind != read_ind) {
                new (&bufobj->data[write_ind]) T(static_cast<T &&>(bufobj->data[read_ind]));
                bufobj->data[read_ind].~T();
            }
            ++write_ind;
        }
    }
    bufobj->size = write_ind;
    return removed;
}

template<typename T>
sizet arr_index_of(T *bufobj, typename T::value_type *item)
{
    sizet offset = (item - bufobj->data);
    if (offset < bufobj->size) {
        return offset;
    }
    return INVALID_ID;
}

using byte_array = array<u8>;

template<typename ArchiveT, typename T, sizet N>
void pack_unpack(ArchiveT *ar, static_array<T, N> &val, const pack_var_info &vinfo)
{
    pup_var(ar, val.size, {"size"});
    pup_var(ar, val.data, {"data", {pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE, &val.size}});
}

} // namespace nslib
