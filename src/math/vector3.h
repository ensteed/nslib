#pragma once

#include "vector2.h"

namespace nslib
{

template<class T>
struct vector3
{
    enum coord_sys
    {
        Cartesian,
        Cylindrical,
        Spherical
    };

    enum euler_order
    {
        XYZ,
        XZY,
        YXZ,
        YZX,
        ZXY,
        ZYX
    };

    vector3(T val_ = 0) : x(val_), y(val_), z(val_)
    {}

    vector3(const vector2<T> &xy_, T z_) : xy(xy_), _z(z_)
    {}

    vector3(T x_, const vector2<T> &yz_) : _x(x_), yz(yz_)
    {}

    vector3(T x_, T y_, T z_ = 0) : x(x_), y(y_), z(z_)
    {}

    vector3(T data_[3]) : data{data_[0], data_[1], data_[2]}
    {}

    COMMON_OPERATORS(vector3, 3, T)

    union
    {
        T data[size_];

        struct
        {
            T x;
            T y;
            T z;
        };

        struct
        {
            T r;
            T g;
            T b;
        };

        struct
        {
            T s;
            T t;
            T p;
        };

        struct
        {
            T rad;
            T theta;
            T phi;
        };

        struct
        {
            vector2<T> xy;
            T _z;
        };

        struct
        {
            T _x;
            vector2<T> yz;
        };
    };
};

pup_func_tt(vector3)
{
    pup_member(x);
    pup_member(y);
    pup_member(z);
}    

// Enable type trait
template<class U>
struct is_vec<vector3<U>>
{
    static constexpr bool value = true;
};

namespace math
{

#if NOBLE_STEED_SIMD
inline float dot(const vector3<float> &lhs, const vector3<float> &rhs)
{
    __m128 l = _mm_set_ps(0.0f, lhs.z, lhs.y, lhs.x);
    __m128 r = _mm_set_ps(0.0f, rhs.z, rhs.y, rhs.x);
    return _mm_cvtss_f32(_sse_dp(l, r));
}
#endif

template<arithmetic_type T>
void cross(vector3<T> *srcvec, const vector3<T> &cross_with_)
{
    T tmpx = srcvec->x, tmpy = srcvec->y;
    srcvec->x = srcvec->y * cross_with_.z - srcvec->z * cross_with_.y;
    srcvec->y = srcvec->z * cross_with_.x - tmpx * cross_with_.z;
    srcvec->z = tmpx * cross_with_.y - tmpy * cross_with_.x;
}

template<arithmetic_type T>
vector3<T> cross(vector3<T> lhs, const vector3<T> &rhs)
{
    cross(&lhs, rhs);
    return lhs;
}

template<floating_pt T>
vector3<T> spherical_to_cartesian(const vector3<T> &spherical)
{
    vector3<T> ret;
    ret.x = spherical.rad * math::cos(spherical.theta) * math::sin(spherical.phi);
    ret.y = spherical.rad * math::sin(spherical.theta) * math::sin(spherical.phi);
    ret.z = spherical.rad * math::cos(spherical.phi);
    return ret;
}

template<floating_pt T>
vector3<T> cylindrical_to_cartesian(const vector3<T> &cylindrical)
{
    vector3<T> ret;
    ret.x = cylindrical.rad * math::cos(cylindrical.theta);
    ret.y = cylindrical.rad * math::sin(cylindrical.theta);
    ret.z = cylindrical.z;
    return ret;
}

template<floating_pt T>
vector3<T> cartesian_to_cylindrical(const vector3<T> &cartesian)
{
    vector3<T> ret;
    ret.x = math::sqrt(cartesian.x * cartesian.x + cartesian.y * cartesian.y);

    if (fequals(cartesian.x, 0))
    {
        ret.y = math::PI / 2;
        if (fequals(cartesian.y, 0))
            ret.y = 0;
    }
    else
        ret.y = math::atan(cartesian.y, cartesian.x);

    ret.z = cartesian.z;
    return ret;
}

template<floating_pt T>
vector3<T> cartesian_to_spherical(const vector3<T> &cartesian)
{
    vector3<T> ret;
    ret.x = length(cartesian);

    if (fequals(cartesian.x, 0))
    {
        ret.y = math::PI / 2;
        if (fequals(cartesian.y, 0))
            ret.y = 0;
    }
    else
        ret.y = math::atan(cartesian.y, cartesian.x);

    if (ret.x == 0)
        ret.z = 0;
    else
        ret.z = math::acos(cartesian.z / ret.x);
    return ret;
}
} // namespace math

using s8vec3 = vector3<s8>;
using s16vec3 = vector3<s16>;
using svec3 = vector3<s32>;
using s64vec3 = vector3<s64>;
using u8vec3 = vector3<u8>;
using u16vec3 = vector3<u16>;
using uvec3 = vector3<u32>;
using u64vec3 = vector3<u64>;
using vec3 = vector3<f32>;
using f64vec3 = vector3<f64>;
} // namespace nslib
