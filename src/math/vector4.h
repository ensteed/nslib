#pragma once

#include "vector3.h"

namespace nslib
{
template<class T>
struct vector4
{
    vector4(T val = 0) : x(val), y(val), z(val), w(val)
    {}

    vector4(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_)
    {}

    vector4(const vector3<T> &xyz_, T w_ = 1) : xyz(xyz_), _wvec3(w_)
    {}

    vector4(T x_, const vector3<T> &yzw_) : _xvec3(x_), yzw(yzw_)
    {}

    vector4(const vector2<T> &xy_, const vector2<T> &zw_) : xy(xy_), zw(zw_)
    {}

    vector4(T x_, const vector2<T> &yz_, T w_) : _xvec2(x_), yz(yz_), _wvec2(w_)
    {}

    vector4(T data_[4]) : data{data_[0], data_[1], data_[2], data_[3]}
    {}

    COMMON_OPERATORS(vector4, 4, T)

#if NOBLE_STEED_SIMD
    using _simd_type = typename simd_traits<T, size_>::_simd_type;
#endif

    union
    {
        T data[size_];

        struct
        {
            T x;
            T y;
            T z;
            T w;
        };

        struct
        {
            T r;
            T g;
            T b;
            T a;
        };

        struct
        {
            T s;
            T t;
            T p;
            T q;
        };

        struct
        {
            vector3<T> xyz;
            T _wvec3;
        };

        struct
        {
            T _xvec3;
            vector3<T> yzw;
        };

        struct
        {
            vector2<T> xy;
            vector2<T> zw;
        };

        struct
        {
            T _xvec2;
            vector2<T> yz;
            T _wvec2;
        };

#if NOBLE_STEED_SIMD
        _simd_type _v4;
#endif
    };
};

pup_func_tt(vector4)
{
    pup_member(x);
    pup_member(y);
    pup_member(z);
    pup_member(w);
}    

// Enable type trait
template<class U>
struct is_vec<vector4<U>>
{
    static constexpr bool value = true;
};

namespace math
{
#if NOBLE_STEED_SIMD

inline __m128 _sse_dp(const __m128 &left, const __m128 &right)
{
#if NOBLE_STEED_SIMD & NOBLE_STEED_SSE41_BIT
    return _mm_dp_ps(left, right, 0xff);
#elif NOBLE_STEED_SIMD & NOBLE_STEED_SSE3_BIT
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

inline float dot(const vector4<float> &lhs, const vector4<float> &rhs)
{
    return _mm_cvtss_f32(_sse_dp(lhs._v4, rhs._v4));
}
#endif

} // namespace math

#if NOBLE_STEED_SIMD

inline vector4<float> operator*(vector4<float> lhs, const vector4<float> &rhs)
{
    lhs._v4 = _mm_mul_ps(lhs._v4, rhs._v4);
    return lhs;
}

inline vector4<float> operator/(vector4<float> lhs, const vector4<float> &rhs)
{
    lhs._v4 = _mm_div_ps(lhs._v4, rhs._v4);
    return lhs;
}

inline vector4<float> operator+(vector4<float> lhs, const vector4<float> &rhs)
{
    lhs._v4 = _mm_add_ps(lhs._v4, rhs._v4);
    return lhs;
}

inline vector4<float> operator-(vector4<float> lhs, const vector4<float> &rhs)
{
    lhs._v4 = _mm_sub_ps(lhs._v4, rhs._v4);
    return lhs;
}

template<arithmetic_type T>
inline vector4<float> operator*(vector4<float> lhs, T rhs)
{
    __m128 s = _mm_set1_ps(rhs);
    lhs._v4 = _mm_mul_ps(lhs._v4, s);
    return lhs;
}

template<arithmetic_type T>
inline vector4<float> operator/(vector4<float> lhs, T rhs)
{
    __m128 s = _mm_set1_ps(1.0/rhs);
    lhs._v4 = _mm_mul_ps(lhs._v4, s);
    return lhs;
}

#endif


using s8vec4 = vector4<s8>;
using s16vec4 = vector4<s16>;
using svec4 = vector4<s32>;
using s64vec4 = vector4<s64>;
using u8vec4 = vector4<u8>;
using u16vec4 = vector4<u16>;
using uvec4 = vector4<u32>;
using u64vec4 = vector4<u64>;
using vec4 = vector4<f32>;
using f64vec4 = vector4<f64>;
} // namespace nslib
