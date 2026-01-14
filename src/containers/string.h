#pragma once
#include <stdio.h>
#include "array.h"
#include "../basic_type_traits.h"

namespace nslib
{

#define ls(...) str_cstr(to_str(__VA_ARGS__))
struct string
{
    using iterator = char *;
    using const_iterator = const char *;

    static constexpr sizet SMALL_STR_SIZE = 24;
    char sos[SMALL_STR_SIZE]{};
    array<char> buf{};

    string();
    string(const string &copy);
    string(const char *copy, mem_arena *arena = nullptr);
    ~string();

    string &operator=(string rhs);
    string &operator+=(const string &rhs);
    const char &operator[](sizet ind) const;
    char &operator[](sizet ind);
};

using string_array = array<string>;

string operator+(const string &lhs, const string &rhs);

bool operator==(const string &lhs, const string &rhs);

inline bool operator!=(const string &lhs, const string &rhs)
{
    return !(lhs == rhs);
}

inline const char *str_cstr(const string *str)
{
    return (str->buf.capacity > string::SMALL_STR_SIZE) ? str->buf.data : str->sos;
}

inline const char *str_cstr(const string &str)
{
    return (str.buf.capacity > string::SMALL_STR_SIZE) ? str.buf.data : str.sos;
}

inline char *str_data(string *str)
{
    return (str->buf.capacity > string::SMALL_STR_SIZE) ? str->buf.data : str->sos;
}

inline sizet str_len(const string *str)
{
    return str->buf.size;
}

inline sizet str_len(const string &str)
{
    return str.buf.size;
}

inline sizet str_capacity(const string &str)
{
    return str.buf.capacity;
}

inline sizet str_capacity(const string *str)
{
    return str->buf.capacity;
}

void swap(string *lhs, string *rhs);

void str_init(string *str, mem_arena *arena = mem_global_arena());

void str_terminate(string *str);

void str_set_capacity(string *str, sizet new_cap);

string::iterator str_begin(string *str);

string::const_iterator str_begin(const string &str);

string::iterator str_end(string *str);

string::const_iterator str_end(const string &str);

bool str_empty(const string &str);

string *str_copy(string *dest, const string &src);

string *str_copy(string *dest, const char *src);

string *str_copy(string *dest, const char *src, sizet src_len);

string *str_resize(string *str, sizet new_size, char c);

string *str_resize(string *str, sizet new_size);

string *str_clear(string *str);

string *str_reserve(string *str, sizet new_cal);

string *str_shrink_to_fit(string *str);

string *str_push_back(string *str, char c);

string *str_pop_back(string *str);

string::iterator str_erase(string *str, string::iterator iter);

string::iterator str_erase(string *str, string::iterator first, string::iterator last);

bool str_remove(string *str, sizet ind);

sizet str_remove(string *str, char c);

string *str_append(string *str, const string &to_append);

string *str_append(string *str, const char *to_append);

string *str_append(string *str, const char *to_append, sizet to_append_len);

template<class... Args>
string *str_printf(string *dest, const char *format_txt, Args &&...args)
{
    sizet needed_space = snprintf(nullptr, 0, format_txt, static_cast<Args &&>(args)...);
    sizet sz = str_len(*dest);
    str_resize(dest, sz + needed_space);
    snprintf(str_data(dest) + sz, needed_space + 1, format_txt, args...);
    return dest;
}

template<class... Args>
void str_scanf(const char* src, const char *format_txt, Args &&...args)
{
    sscanf(src, format_txt, static_cast<Args &&>(args)...);
}

template<class... Args>
void str_scanf(const string &src, const char *format_txt, Args &&...args)
{
    str_scanf(str_cstr(src), format_txt, static_cast<Args &&>(args)...);
}

template<class... Args>
string to_str(const char *format_txt, Args &&...args)
{
    string ret{};
    sizet needed_space = snprintf(nullptr, 0, format_txt, static_cast<Args &&>(args)...);
    sizet sz = str_len(ret);
    str_resize(&ret, sz + needed_space);
    snprintf(str_data(&ret) + sz, needed_space + 1, format_txt, args...);
    return ret;
}

u64 hash_type(const string &key, u64 seed0, u64 seed1);

template<signed_integral T>
string to_str(const T &n)
{
    string ret;
    str_printf(&ret, "%d", n);
    return ret;
}

template<unsigned_integral T>
string to_str(const T &n)
{
    string ret;
    str_printf(&ret, "%u", n);
    return ret;
}

template<floating_pt T>
string to_str(const T &n)
{
    string ret;
    str_printf(&ret, "%f", n);
    return ret;
}

template<signed_integral T>
void from_str(T *n, const string &str)
{
    str_scanf(str, "%d", n);
}

template<unsigned_integral T>
void from_str(T *n, const string &str)
{
    str_scanf(str, "%u", n);
}

template<floating_pt T>
void from_str(T *n, const string &str)
{
    str_scanf(str, "%f", n);
}

inline const string &to_str(const string &str)
{
    return str;
}

string to_str(void *i);
string to_str(s64 i);
string to_str(u64 i);
string to_str(char c);

void from_str(void **i, const string &str);
void from_str(s64 *i, const string &str);
void from_str(u64 *i, const string &str);
void from_str(s16 *i, const string &str);
void from_str(u16 *i, const string &str);
void from_str(s8 *i, const string &str);
void from_str(u8 *i, const string &str);
void from_str(char *c, const string &str);

void from_str(void **i, const char *str);
void from_str(s64 *i, const char *str);
void from_str(u64 *i, const char *str);
void from_str(s16 *i, const char *str);
void from_str(u16 *i, const char *str);
void from_str(s8 *i, const char *str);
void from_str(u8 *i, const char *str);
void from_str(char *c, const char *str);

template<class ArchiveT>
void pack_unpack(ArchiveT *ar, string &val, const pack_var_info &vinfo)
{
    sizet size = val.buf.size;
    pup_var(ar, size, {"size"});
    str_resize(&val, size);
    for (sizet i = 0; i < size; ++i) {
        pup_var(ar, val[i], {ls("[%d]", i)});
    }
}

// We have to put our array func here rather than in array because of the ls call
template<class ArchiveT, class T>
void pack_unpack(ArchiveT *ar, array<T> &val, const pack_var_info &vinfo)
{
    pup_var(ar, val.size, {"size"});
    arr_resize(&val, val.size);
    for (int i = 0; i < val.size; ++i) {
        pup_var(ar, val[i], {ls("[%d]", i)});
    }
}

} // namespace nslib
