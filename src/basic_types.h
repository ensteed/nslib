#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <climits>
// #include "osdef.h"

// Check if all of the flags in provided flags
#define test_all_flags(bitmask, flags) (((bitmask) & (flags)) == (flags))

// Check if any of the bits provided in \param flags are set in \param bitmask
#define test_flags(bitmask, flags) (((bitmask) & (flags)) != 0)

// Unset all of the bits provided by \param flags in \param bitmask
#define unset_flags(bitmask, flags) ((bitmask) &= ~(flags))

// Set all of the bits provided by \param flags in \param bitmask
#define set_flags(bitmask, flags) ((bitmask) |= (flags))

#define set_flag_from_bool(bitmask, flag, boolval) ((boolval) ? set_flags(bitmask, flag) : unset_flags(bitmask, flag))

#define intern static

#define SMALL_STR_LEN 24

#define op_eq_func(type) inline bool operator==(const type &lhs, const type &rhs)

#define op_neq_func(type)                                                                                                                  \
    inline bool operator!=(const type &lhs, const type &rhs)                                                                               \
    {                                                                                                                                      \
        return !(lhs == rhs);                                                                                                              \
    }

#define op_eq_func_tt(type)                                                                                                                \
    template<typename T>                                                                                                                   \
    inline bool operator==(const type<T> &lhs, const type<T> &rhs)

#define op_neq_func_tt(type)                                                                                                               \
    template<typename T>                                                                                                                   \
    inline bool operator!=(const type<T> &lhs, const type<T> &rhs)                                                                         \
    {                                                                                                                                      \
        return !(lhs == rhs);                                                                                                              \
    }

#if defined(NDEBUG)
    #define asrt(param) (!(param)) ? elog("Assertion: " #param " failed") : (void)0
    #define asrt_break(msg) elog("Assertion break: " #msg)
#else
    #define asrt assert
    #define asrt_break(msg) assert(!msg)
#endif

#define asrt_log(expr)                                                                                                                     \
    do {                                                                                                                                   \
        b32 asrt_ok = (expr);                                                                                                              \
        asrt(asrt_ok);                                                                                                                     \
        if (asrt_ok) {                                                                                                                     \
            ilog("asrt ok: %s", #expr);                                                                                                    \
        }                                                                                                                                  \
    } while (0)

namespace nslib
{
using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;
using uchar = unsigned char;
using wchar = wchar_t;
using c16 = char16_t;
using c32 = char32_t;
using sizet = std::size_t;
using f32 = float;
using f64 = double;
using f128 = long double;
using b32 = bool;
using cstr = const char *;

const sizet KB_SIZE = 1024;
const sizet MB_SIZE = 1024 * KB_SIZE;

using small_str = char[SMALL_STR_LEN];

inline constexpr const sizet INVALID_IND = ~(0UL);
inline constexpr const u32 INVALID_ID = ~(0U);

inline bool is_valid(sizet v)
{
    return (v != INVALID_IND);
}
inline bool is_valid(u32 v)
{
    return (v != INVALID_ID);
}

} // namespace nslib
