#pragma once
#include "../basic_types.h"
#include "../basic_type_traits.h"
#include "../archive_common.h" // IWYU pragma: export


#define NOBLE_STEED_SSE_BIT (0x00000001)
#define NOBLE_STEED_SSE2_BIT (0x00000002)
#define NOBLE_STEED_SSE3_BIT (0x00000004)
#define NOBLE_STEED_SSSE3_BIT (0x0000008)
#define NOBLE_STEED_SSE41_BIT (0x00000010)
#define NOBLE_STEED_SSE42_BIT (0x00000020)
#define NOBLE_STEED_AVX_BIT (0x00000040)
#define NOBLE_STEED_AVX2_BIT (0x00000080)
#define NOBLE_STEED_SSE_SQRT_BIT (0x00000100)

#define NOBLE_STEED_USE_SSE (NOBLE_STEED_SSE_BIT)
#define NOBLE_STEED_USE_SSE2 (NOBLE_STEED_SSE2_BIT | NOBLE_STEED_USE_SSE)
#define NOBLE_STEED_USE_SSE3 (NOBLE_STEED_SSE3_BIT | NOBLE_STEED_USE_SSE2)
#define NOBLE_STEED_USE_SSSE3 (NOBLE_STEED_SSSE3_BIT | NOBLE_STEED_USE_SSE3)
#define NOBLE_STEED_USE_SSE41 (NOBLE_STEED_SSE41_BIT | NOBLE_STEED_USE_SSSE3)
#define NOBLE_STEED_USE_SSE42 (NOBLE_STEED_SSE42_BIT | NOBLE_STEED_USE_SSE41)
#define NOBLE_STEED_USE_AVX (NOBLE_STEED_AVX_BIT | NOBLE_STEED_SSE42)
#define NOBLE_STEED_USE_AVX2 (NOBLE_STEED_AVX2_BIT | NOBLE_STEED_AVX)

#define NOBLE_STEED_SIMD (NSLIB_ENABLE_SIMD & NOBLE_STEED_USE_SSE42) // | NOBLE_STEED_SSE_SQRT_BIT)

#if NOBLE_STEED_SIMD
#include <immintrin.h>
namespace nslib
{
template<typename T, s8 SZ>
struct simd_traits
{
    using _simd_type = T[SZ];
};

template<>
struct simd_traits<f32, 4>
{
    using _simd_type = __m128;
    // etc
};

inline __m128 _sse_dp(const __m128 &left, const __m128 &right)
{
#if (NOBLE_STEED_SIMD & NOBLE_STEED_SSE41_BIT)
    return _mm_dp_ps(left, right, 0xff);
#elif (NOBLE_STEED_SIMD & NOBLE_STEED_SSE3_BIT)
    __m128 mul0 = _mm_mul_ps(left, right);
    __m128 hadd0 = _mm_hadd_ps(mul0, mul0);
    __m128 hadd1 = _mm_hadd_ps(hadd0, hadd0);
    return hadd1;
#else
    __m128 mul0 = _mm_mul_ps(left, right);
    __m128 swp0 = _mm_shuffle_ps(mul0, mul0, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 add0 = _mm_add_ps(mul0, swp0);
    mul0 = _mm_shuffle_ps(add0, add0, _MM_SHUFFLE(0, 1, 2, 3));
    add0 = _mm_add_ps(add0, mul0);
    return add0;
#endif
}
} // namespace nslib
#endif

namespace nslib
{
inline constexpr f32 FLOAT_EPS = 0.001f;

template<class T>
struct is_vec
{
    static constexpr bool value = false;
};

template<class T>
struct is_quat
{
    static constexpr bool value = false;
};

template<class T>
struct is_mat
{
    static constexpr bool value = false;
};

template<class T>
concept vec_type = is_vec<T>::value;

template<class T>
concept mat_type = is_mat<T>::value;

template<class T>
concept quat_type = is_quat<T>::value;

template<class T>
concept vec_or_quat_type = vec_type<T> || quat_type<T>;

template<class T>
concept holds_floating_pt = floating_pt<typename T::value_type>;

template<class T>
concept holds_integral = integral<typename T::value_type>;

template<class T>
concept holds_signed_number = signed_number<typename T::value_type>;

template<class T>
concept holds_arithmetic_type = arithmetic_type<typename T::value_type>;

template<class T>
struct vector2;

template<class T>
struct vector3;

template<class T>
struct bounding_box;

template<class T>
struct cube_base;

template<class T>
concept holds_basic_comparable_type = (vec_type<T> || mat_type<T> || quat_type<T>);

template<class T>
concept holds_basic_arithmetic_type = (vec_type<T> || mat_type<T> || quat_type<T>);

template<floating_pt T>
bool fequals(T left, T right, T eps = FLOAT_EPS)
{
    return (left < (right + FLOAT_EPS)) && (left > (right - FLOAT_EPS));
}

template<holds_floating_pt T>
bool fequals(T left, T right)
{
    return (left == right);
}


template<holds_basic_comparable_type T>
requires holds_floating_pt<T>
bool operator==(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i) {
        if (!fequals(lhs_[i], rhs_[i]))
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
requires holds_integral<T>
bool operator==(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i) {
        if (!(lhs_[i] == rhs_[i]))
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
bool operator!=(const T &lhs_, const T &rhs_)
{
    return !(lhs_ == rhs_);
}

template<holds_basic_comparable_type T>
bool operator<(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i) {
        if (lhs_[i] >= rhs_[i])
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
bool operator>(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i) {
        if (lhs_[i] <= rhs_[i])
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
bool operator<=(const T &lhs_, const T &rhs_)
{
    return (lhs_ < rhs_) || (lhs_ == rhs_);
}

template<holds_basic_comparable_type T>
bool operator>=(const T &lhs_, const T &rhs_)
{
    return (lhs_ > rhs_) || (lhs_ == rhs_);
}

template<holds_basic_arithmetic_type T>
T operator+(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] += rhs_[i];
    return lhs_;
}

template<holds_basic_arithmetic_type T>
T operator-(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] -= rhs_[i];
    return lhs_;
}

template<holds_basic_arithmetic_type T>
T operator*(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] *= rhs_[i];
    return lhs_;
}

template<arithmetic_type T, holds_basic_arithmetic_type U>
U operator*(U lhs_, T rhs_)
{
    for (auto &&element : lhs_)
        element *= typename U::value_type(rhs_);
    return lhs_;
}

template<arithmetic_type T, holds_basic_arithmetic_type U>
U operator*(T lhs_, const U &rhs_)
{
    return rhs_ * lhs_;
}

template<holds_basic_arithmetic_type T>
T operator/(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] /= rhs_[i];
    return lhs_;
}

template<arithmetic_type T, holds_basic_arithmetic_type U>
U operator/(const U lhs_, T rhs_)
{
    T tmp((T)1.0 / rhs_);
    return lhs_ * tmp;
}

template<integral T, holds_basic_arithmetic_type U>
U operator/(U lhs_, T rhs_)
{
    for (auto &&element : lhs_)
        element /= rhs_;
    return lhs_;
}

template<floating_pt T, holds_basic_arithmetic_type U>
U operator/(T lhs_, U rhs_)
{
    for (auto &&element : rhs_)
        element = lhs_ / element;
    return rhs_;
}

#define COMMON_OPERATORS(type, element_count, ind_ret_type)                                                                                \
    template<class U>                                                                                                                      \
    using container_type = vector2<U>;                                                                                                     \
    using value_type = T;                                                                                                                  \
    using iterator = ind_ret_type *;                                                                                                       \
    using const_iterator = const ind_ret_type *;                                                                                           \
    static constexpr sizet size_ = element_count;                                                                                          \
    template<class U>                                                                                                                      \
    type(const type<U> &conv)                                                                                                              \
    {                                                                                                                                      \
        for (u8 i{0}; i < size_; ++i)                                                                                                      \
            data[i] = (T)conv[i];                                                                                                          \
    }                                                                                                                                      \
    inline type<T> &operator+=(const type<T> &rhs_)                                                                                        \
    {                                                                                                                                      \
        return *this = *this + rhs_;                                                                                                       \
    }                                                                                                                                      \
    inline type<T> &operator-=(const type<T> &rhs_)                                                                                        \
    {                                                                                                                                      \
        return *this = *this - rhs_;                                                                                                       \
    }                                                                                                                                      \
    inline type<T> &operator*=(const type<T> &rhs_)                                                                                        \
    {                                                                                                                                      \
        return *this = *this * rhs_;                                                                                                       \
    }                                                                                                                                      \
    inline type<T> &operator/=(const type<T> &rhs_)                                                                                        \
    {                                                                                                                                      \
        return *this = *this / rhs_;                                                                                                       \
    }                                                                                                                                      \
    inline type<T> &operator*=(const T &rhs_)                                                                                              \
    {                                                                                                                                      \
        return *this = *this * rhs_;                                                                                                       \
    }                                                                                                                                      \
    inline type<T> &operator/=(const T &rhs_)                                                                                              \
    {                                                                                                                                      \
        return *this = *this / rhs_;                                                                                                       \
    }                                                                                                                                      \
    inline const ind_ret_type &operator[](sizet val_) const                                                                                \
    {                                                                                                                                      \
        return data[val_];                                                                                                                 \
    }                                                                                                                                      \
    inline ind_ret_type &operator[](sizet val_)                                                                                            \
    {                                                                                                                                      \
        return data[val_];                                                                                                                 \
    }                                                                                                                                      \
    inline constexpr sizet size() const                                                                                                    \
    {                                                                                                                                      \
        return size_;                                                                                                                      \
    }                                                                                                                                      \
    inline constexpr iterator begin()                                                                                                      \
    {                                                                                                                                      \
        return &data[0];                                                                                                                   \
    }                                                                                                                                      \
    inline constexpr iterator end()                                                                                                        \
    {                                                                                                                                      \
        return begin() + size_;                                                                                                            \
    }                                                                                                                                      \
    inline constexpr const_iterator begin() const                                                                                          \
    {                                                                                                                                      \
        return &data[0];                                                                                                                   \
    }                                                                                                                                      \
    inline constexpr const_iterator end() const                                                                                            \
    {                                                                                                                                      \
        return begin() + size_;                                                                                                            \
    }

} // namespace nslib
