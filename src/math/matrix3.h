#pragma once

#include "quaternion.h"
#include "matrix2.h"

namespace nslib
{
enum view_matrix_ind
{
    VIEW_MATRIX_ROW_RIGHT,
    VIEW_MATRIX_ROW_UP,
    VIEW_MATRIX_ROW_TARGET,
    VIEW_MATRIX_COL_POS
};

template<class T>
struct matrix3
{
    matrix3() : data{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}
    {}

    matrix3(const T &val_) : data{{val_}, {val_}, {val_}}
    {}

    matrix3(const vector3<T> &row1_, const vector3<T> &row2_, const vector3<T> &row3_): row1(row1_), row2(row2_), row3(row3_)
    {}

    matrix3(const matrix2<T> &basis_)
    {
        data[0][0] = basis_[0][0];
        data[0][1] = basis_[0][1];
        data[1][0] = basis_[1][0];
        data[1][1] = basis_[1][1];
        data[0][2] = 0;
        data[1][2] = 0;
        data[2][0] = 0;
        data[2][1] = 0;
        data[2][2] = 1;
    }

    matrix3(T data_[9]): elements{data_[0], data_[1], data_[2], data_[3], data_[4], data_[5], data_[6], data_[7], data_[8]}
    {}

    matrix3(T data_[3][3]): data{data_[0], data_[1], data_[2]}
    {}

    COMMON_OPERATORS(matrix3, 3, vector3<T>)

    vector3<T> operator()(s8 ind) const
    {
        return {data[0][ind], data[1][ind], data[2][ind]};
    }

    union
    {
        T elements[size_*vector3<T>::size_];
        vector3<T> data[size_];
        struct
        {
            vector3<T> row1;
            vector3<T> row2;
            vector3<T> row3;
        };
    };
};

pup_func_tt(matrix3)
{
    pup_member(row1);
    pup_member(row2);
    pup_member(row3);
}    

// Enable type trait
template<class U>
struct is_mat<matrix3<U>>
{
    static constexpr bool value = true;
};

namespace math
{

template<class T>
T determinant(const matrix3<T> &mat)
{
    return mat.data[0][0] * mat.data[1][1] * mat.data[2][2] - mat.data[0][0] * mat.data[1][2] * mat.data[2][1] +
           mat.data[0][1] * mat.data[1][2] * mat.data[2][0] - mat.data[0][1] * mat.data[1][0] * mat.data[2][2] +
           mat.data[0][2] * mat.data[1][0] * mat.data[2][1] - mat.data[0][2] * mat.data[1][1] * mat.data[2][0];
}

template<floating_pt T>
matrix3<T> inverse(const matrix3<T> &mat)
{
    T inv_det = (T)1 / determinant(mat);
    return {{(mat.data[1][1] * mat.data[2][2] - mat.data[1][2] * mat.data[2][1]) * inv_det,
             (mat.data[0][2] * mat.data[2][1] - mat.data[0][1] * mat.data[2][2]) * inv_det,
             (mat.data[0][1] * mat.data[1][2] - mat.data[0][2] * mat.data[1][1]) * inv_det},
            {(mat.data[1][2] * mat.data[2][0] - mat.data[1][0] * mat.data[2][2]) * inv_det,
             (mat.data[0][0] * mat.data[2][2] - mat.data[0][2] * mat.data[2][0]) * inv_det,
             (mat.data[0][2] * mat.data[1][0] - mat.data[0][0] * mat.data[1][2]) * inv_det},
            {(mat.data[1][0] * mat.data[2][1] - mat.data[1][1] * mat.data[2][0]) * inv_det,
             (mat.data[0][1] * mat.data[2][0] - mat.data[0][0] * mat.data[2][1]) * inv_det,
             (mat.data[0][0] * mat.data[1][1] - mat.data[0][1] * mat.data[1][0]) * inv_det}};
}

template<class T>
void transpose(matrix3<T> *mat)
{
    T tmp;
    tmp = mat->data[1][0];
    mat->data[1][0] = mat->data[0][1];
    mat->data[0][1] = tmp;
    tmp = mat->data[2][0];
    mat->data[2][0] = mat->data[0][2];
    mat->data[0][2] = tmp;
    tmp = mat->data[2][1];
    mat->data[2][1] = mat->data[1][2];
    mat->data[1][2] = tmp;
}

template<class T>
matrix3<T> transpose(matrix3<T> mat)
{
    transpose(&mat);
    return mat;
}

template<class T>
matrix2<T> basis(const matrix3<T> &mat)
{
    return {{mat.data[0][0], mat.data[0][1]}, {mat.data[1][0], mat.data[1][1]}};
}

template<class T>
vector3<T> up_vec(const matrix3<T> &mat)
{
    return normalize(mat(1));
}

template<class T>
vector3<T> right_vec(const matrix3<T> &mat)
{
    return normalize(mat(0));
}

template<class T>
vector3<T> target_vec(const matrix3<T> &mat)
{
    return normalize(mat(2));
}

template<floating_pt T>
matrix3<T> rotation_mat(const vector4<T> &axis_angle)
{
    matrix3<T> ret;

    // http://www.euclideanspace.com/maths/geometry/rotations/conversions/angleToMatrix/index.htm
    T angle = axis_angle.a;

    T c = math::cos(angle);
    T s = math::sin(angle);
    T t = 1 - c;

    ret.data[0][0] = c + axis_angle.x * axis_angle.x * t;
    ret.data[1][1] = c + axis_angle.y * axis_angle.y * t;
    ret.data[2][2] = c + axis_angle.z * axis_angle.z * t;

    T tmp1 = axis_angle.x * axis_angle.y * t;
    T tmp2 = axis_angle.z * s;
    ret.data[1][0] = tmp1 + tmp2;
    ret.data[0][1] = tmp1 - tmp2;

    tmp1 = axis_angle.x * axis_angle.z * t;
    tmp2 = axis_angle.y * s;
    ret.data[2][0] = tmp1 - tmp2;
    ret.data[0][2] = tmp1 + tmp2;

    tmp1 = axis_angle.y * axis_angle.z * t;
    tmp2 = axis_angle.x * s;
    ret.data[2][1] = tmp1 + tmp2;
    ret.data[1][2] = tmp1 - tmp2;
    return ret;
}

template<floating_pt T>
matrix3<T> rotation_mat(const vector3<T> &euler, typename vector3<T>::euler_order order)
{
    matrix3<T> ret;
    T cb = math::cos(euler.x);
    T sb = math::sin(euler.x);
    T ch = math::cos(euler.y);
    T sh = math::sin(euler.y);
    T ca = math::cos(euler.z);
    T sa = math::sin(euler.z);

    switch (order)
    {
    case (vector3<T>::XYZ):
        ret.data[0][0] = ch * ca;
        ret.data[0][1] = -ch * sa;
        ret.data[0][2] = sh;

        ret.data[1][0] = cb * sa + sb * ca * sh;
        ret.data[1][1] = cb * ca - sb * sa * sh;
        ret.data[1][2] = -sb * ch;

        ret.data[2][0] = sb * sa - cb * ca * sh;
        ret.data[2][1] = sb * ca + cb * sa * sh;
        ret.data[2][2] = cb * ch;
        break;
    case (vector3<T>::XZY):
        ret.data[0][0] = ch * ca;
        ret.data[0][1] = -sa;
        ret.data[0][2] = sh * ca;

        ret.data[1][0] = cb * ch * sa + sb * sh;
        ret.data[1][1] = cb * ca;
        ret.data[1][2] = cb * sh * sa - sb * ch;

        ret.data[2][0] = sb * ch * sa - cb * sh;
        ret.data[2][1] = sb * ca;
        ret.data[2][2] = sb * sh * sa + cb * ch;
        break;
    case (vector3<T>::YXZ):
        ret.data[0][0] = ch * ca + sh * sa * sb;
        ret.data[0][1] = sh * ca * sb - ch * sa;
        ret.data[0][2] = cb * sh;

        ret.data[1][0] = cb * sa;
        ret.data[1][1] = cb * ca;
        ret.data[1][2] = -sb;

        ret.data[2][0] = ch * sa * sb - sh * ca;
        ret.data[2][1] = sh * sa + ch * ca * sb;
        ret.data[2][2] = cb * ch;
        break;
    case (vector3<T>::YZX):
        ret.data[0][0] = ch * ca;
        ret.data[0][1] = sb * sh - cb * ch * sa;
        ret.data[0][2] = sb * ch * sa + cb * sh;

        ret.data[1][0] = sa;
        ret.data[1][1] = cb * ca;
        ret.data[1][2] = -sb * ca;

        ret.data[2][0] = -sh * ca;
        ret.data[2][1] = cb * sh * sa + sb * ch;
        ret.data[2][2] = cb * ch - sb * sh * sa;
        break;
    case (vector3<T>::ZXY):
        ret.data[0][0] = ch * ca - sh * sa * sb;
        ret.data[0][1] = -cb * sa;
        ret.data[0][2] = sh * ca + ch * sa * sb;

        ret.data[1][0] = ch * sa + sh * ca * sb;
        ret.data[1][1] = cb * ca;
        ret.data[1][2] = sh * sa - ch * ca * sb;

        ret.data[2][0] = -cb * sh;
        ret.data[2][1] = sb;
        ret.data[2][2] = cb * ch;
        break;
    case (vector3<T>::ZYX):
        ret.data[0][0] = ch * ca;
        ret.data[0][1] = sb * ca * sh - cb * sa;
        ret.data[0][2] = cb * ca * sh + sb * sa;

        ret.data[1][0] = ch * sa;
        ret.data[1][1] = sb * sa * sh + cb * ca;
        ret.data[1][2] = cb * sa * sh - sb * ca;

        ret.data[2][0] = -sh;
        ret.data[2][1] = sb * ch;
        ret.data[2][2] = cb * ch;
        break;
    }
    return ret;
}

template<floating_pt T>
matrix3<T> rotation_mat(const quaternion<T> &orient)
{
    matrix3<T> ret;
    ret.data[0][0] = 1 - 2 * (orient.y * orient.y + orient.z * orient.z);
    ret.data[0][1] = 2 * (orient.x * orient.y - orient.z * orient.w);
    ret.data[0][2] = 2 * (orient.x * orient.z + orient.y * orient.w);

    ret.data[1][0] = 2 * (orient.x * orient.y + orient.z * orient.w);
    ret.data[1][1] = 1 - 2 * (orient.x * orient.x + orient.z * orient.z);
    ret.data[1][2] = 2 * (orient.y * orient.z - orient.x * orient.w);

    ret.data[2][0] = 2 * (orient.x * orient.z - orient.y * orient.w);
    ret.data[2][1] = 2 * (orient.y * orient.z + orient.x * orient.w);
    ret.data[2][2] = 1 - 2 * (orient.x * orient.x + orient.y * orient.y);
    return ret;
}

template<floating_pt T>
matrix3<T> rotation_mat(const vector3<T> &from_vec, const vector3<T> &to_vec)
{
    /* http://www.euclideanspace.com/maths/algebra/vectors/angleBetween/index.htm */
    matrix3<T> ret;

    vector3<T> vs = cross(from_vec, to_vec);
    T ca = math::dot(from_vec, to_vec);
    vector3<T> v = normalize(vs);
    vector3<T> vt = v * ((T)1 - ca);

    ret.data[0][0] = vt.x * v.x + ca;
    ret.data[1][1] = vt.y * v.y + ca;
    ret.data[2][2] = vt.z * v.z + ca;
    vt.x *= v.y;
    vt.z *= v.x;
    vt.y *= v.z;
    ret.data[0][1] = vt.x - vs.z;
    ret.data[0][2] = vt.z + vs.y;
    ret.data[1][0] = vt.x + vs.z;
    ret.data[1][2] = vt.y - vs.x;
    ret.data[2][0] = vt.z - vs.y;
    ret.data[2][1] = vt.y + vs.x;
    return ret;
}

template<class T>
matrix3<T> rotation_mat(const matrix3<T> &transform)
{
    matrix3<T> ret(transform);
    normalize(&ret.data[0]);
    normalize(&ret.data[1]);
    normalize(&ret.data[2]);
    return ret;
}

template<floating_pt T>
vector3<T> euler(const matrix3<T> &rotation, typename vector3<T>::euler_order order = vector3<T>::XYZ)
{
    // https://github.com/mrdoob/three.js/blob/master/src/math/Euler.js
    vector3<T> ret;
    T ep = 1 - math::FLOAT_EPS;
    switch (order)
    {
    case (vector3<T>::XYZ):
        ret.y = math::asin(rotation[0][2]);
        if (std::abs(rotation[0][2]) < ep)
        {
            ret.x = math::atan(-rotation[1][2], rotation[2][2]);
            ret.z = math::atan(-rotation[0][1], rotation[0][0]);
        }
        else
        {
            ret.x = math::atan(rotation[2][1], rotation[1][1]);
            ret.z = 0;
        }
        break;
    case (vector3<T>::XZY):
        ret.z = math::asin(rotation[0][1]);
        if (std::abs(rotation[0][1]) < ep)
        {
            ret.x = math::atan(rotation[2][1], rotation[1][1]);
            ret.y = math::atan(rotation[0][2], rotation[0][0]);
        }
        else
        {
            ret.x = math::atan(-rotation[1][2], rotation[2][2]);
            ret.y = 0;
        }
        break;
    case (vector3<T>::YXZ):
        ret.x = math::asin(rotation[1][2]);
        if (std::abs(rotation[1][2]) < ep)
        {
            ret.y = math::atan(rotation[0][2], rotation[2][2]);
            ret.z = math::atan(rotation[1][0], rotation[1][1]);
        }
        else
        {
            ret.y = math::atan(-rotation[2][0], rotation[0][0]);
            ret.z = 0;
        }
        break;
    case (vector3<T>::YZX):
        ret.z = math::asin(rotation[1][0]);
        if (std::abs(rotation[1][0]) < ep)
        {
            ret.x = math::atan(-rotation[1][2], rotation[1][1]);
            ret.y = math::atan(-rotation[2][0], rotation[0][0]);
        }
        else
        {
            ret.x = 0;
            ret.y = math::atan(rotation[0][2], rotation[2][2]);
        }
        break;
    case (vector3<T>::ZXY):
        ret.x = math::asin(rotation[2][1]);
        if (std::abs(rotation[2][1]) < ep)
        {
            ret.y = math::atan(-rotation[2][0], rotation[2][2]);
            ret.z = math::atan(-rotation[0][1], rotation[1][1]);
        }
        else
        {
            ret.y = 0;
            ret.z = math::atan(rotation[1][0], rotation[0][0]);
        }
        break;
    case (vector3<T>::ZYX):
        ret.y = math::asin(rotation[2][0]);
        if (std::abs(rotation[2][0]) < ep)
        {
            ret.x = math::atan(rotation[2][1], rotation[2][2]);
            ret.z = math::atan(rotation[1][0], rotation[0][0]);
        }
        else
        {
            ret.x = 0;
            ret.z = math::atan(-rotation[0][1], rotation[1][1]);
        }
        break;
    }
    return ret;
}

template<floating_pt T>
quaternion<T> orientation(const matrix3<T> &rotation)
{
    quaternion<T> ret;
    T tr = rotation[0][0] + rotation[1][1] + rotation[2][2], s;

    if (tr > 0)
    {
        s = math::sqrt(tr + T(1.0)) * 2;
        ret.w = T(0.25) * s;
        ret.x = (rotation[2][1] - rotation[1][2]) / s;
        ret.y = (rotation[0][2] - rotation[2][0]) / s;
        ret.z = (rotation[1][0] - rotation[0][1]) / s;
    }
    else if ((rotation[0][0] > rotation[1][1]) & (rotation[0][0] > rotation[2][2]))
    {
        s = math::sqrt(T(1.0) + rotation[0][0] - rotation[1][1] - rotation[2][2]) * 2;
        ret.w = (rotation[2][1] - rotation[1][2]) / s;
        ret.x = T(0.25) * s;
        ret.y = (rotation[0][1] + rotation[1][0]) / s;
        ret.z = (rotation[0][2] + rotation[2][0]) / s;
    }
    else if (rotation[1][1] > rotation[2][2])
    {
        s = math::sqrt(T(1.0) + rotation[1][1] - rotation[0][0] - rotation[2][2]) * 2;
        ret.w = (rotation[0][2] - rotation[2][0]) / s;
        ret.x = (rotation[0][1] + rotation[1][0]) / s;
        ret.y = T(0.25) * s;
        ret.z = (rotation[1][2] + rotation[2][1]) / s;
    }
    else
    {
        s = math::sqrt(T(1.0) + rotation[2][2] - rotation[0][0] - rotation[1][1]) * 2;
        ret.w = (rotation[1][0] - rotation[0][1]) / s;
        ret.x = (rotation[0][2] + rotation[2][0]) / s;
        ret.y = (rotation[1][2] + rotation[2][1]) / s;
        ret.z = T(0.25) * s;
    }
    return ret;
}

template<class T>
matrix3<T> scaling_mat(const vector3<T> &scale)
{
    matrix3<T> ret;
    ret.data[0][0] = scale.x;
    ret.data[1][1] = scale.y;
    ret.data[2][2] = scale.z;
    return ret;
}

template<class T>
matrix3<T> scaling_mat(const matrix3<T> &transform)
{
    matrix3<T> ret;
    ret.data[0][0] = length(transform[0]);
    ret.data[1][1] = length(transform[1]);
    ret.data[2][2] = length(transform[2]);
    return ret;
}

template<class T>
vector3<T> scaling_vec(const matrix3<T> &transform)
{
    vector3<T> ret;
    ret.x = length(transform[0]);
    ret.y = length(transform[1]);
    ret.z = length(transform[2]);
    return ret;
}

template<class T>
vector3<T> translation_vec(const matrix3<T> &transform)
{
    return transform(3);
}


} // namespace math

// Overloaded operators
template<class T>
matrix3<T> operator*(const matrix3<T> &lhs, const matrix3<T> &rhs)
{
    matrix3<T> ret;
    ret.data[0][0] = lhs.data[0][0] * rhs.data[0][0] + lhs.data[0][1] * rhs.data[1][0] + lhs.data[0][2] * rhs.data[2][0];
    ret.data[0][1] = lhs.data[0][0] * rhs.data[0][1] + lhs.data[0][1] * rhs.data[1][1] + lhs.data[0][2] * rhs.data[2][1];
    ret.data[0][2] = lhs.data[0][0] * rhs.data[0][2] + lhs.data[0][1] * rhs.data[1][2] + lhs.data[0][2] * rhs.data[2][2];

    ret.data[1][0] = lhs.data[1][0] * rhs.data[0][0] + lhs.data[1][1] * rhs.data[1][0] + lhs.data[1][2] * rhs.data[2][0];
    ret.data[1][1] = lhs.data[1][0] * rhs.data[0][1] + lhs.data[1][1] * rhs.data[1][1] + lhs.data[1][2] * rhs.data[2][1];
    ret.data[1][2] = lhs.data[1][0] * rhs.data[0][2] + lhs.data[1][1] * rhs.data[1][2] + lhs.data[1][2] * rhs.data[2][2];

    ret.data[2][0] = lhs.data[2][0] * rhs.data[0][0] + lhs.data[2][1] * rhs.data[1][0] + lhs.data[2][2] * rhs.data[2][0];
    ret.data[2][1] = lhs.data[2][0] * rhs.data[0][1] + lhs.data[2][1] * rhs.data[1][1] + lhs.data[2][2] * rhs.data[2][1];
    ret.data[2][2] = lhs.data[2][0] * rhs.data[0][2] + lhs.data[2][1] * rhs.data[1][2] + lhs.data[2][2] * rhs.data[2][2];
    return ret;
}

template<class T>
matrix3<T> operator/(const matrix3<T> &lhs, const matrix3<T> &rhs)
{
    return lhs * math::inverse(rhs);
}

template<class T>
vector3<T> operator*(const matrix3<T> &lhs, const vector3<T> &rhs)
{
    using namespace math;
    return {dot(lhs.data[0], rhs), dot(lhs.data[1], rhs), dot(lhs.data[2], rhs)};
}

template<floating_pt T>
inline vector3<T> operator/(const matrix3<T> &lhs, const vector3<T> &rhs)
{
    using namespace math;
    T mult = 1 / dot(rhs, rhs);
    return {dot(lhs[0], rhs) * mult, dot(lhs[1], rhs) * mult, dot(lhs[2], rhs) * mult};
}

template<integral T>
inline vector3<T> operator/(const matrix3<T> &lhs, const vector3<T> &rhs)
{
    using namespace math;
    T lensq = dot(rhs, rhs);
    return {dot(lhs[0], rhs) / lensq, dot(lhs[1], rhs) / lensq, dot(lhs[2], rhs) / lensq};
}

template<class T>
vector3<T> operator*(const vector3<T> &lhs, const matrix3<T> &rhs)
{
    vector3<T> ret;
    ret[0] = lhs[0] * rhs[0][0] + lhs[1] * rhs[1][0] + lhs[2] * rhs[2][0];
    ret[1] = lhs[0] * rhs[0][1] + lhs[1] * rhs[1][1] + lhs[2] * rhs[2][1];
    ret[2] = lhs[0] * rhs[0][2] + lhs[1] * rhs[1][2] + lhs[2] * rhs[2][2];
    return ret;
}

template<class T>
vector3<T> operator/(const vector3<T> &lhs, const matrix3<T> &rhs)
{
    return lhs * math::inverse(rhs);
}

#if NOBLE_STEED_SIMD
inline vector3<float> operator*(const vector3<float> &lhs, const matrix3<float> &rhs)
{
    auto mat = math::transpose(rhs);
    return mat * lhs;
}
#endif

using s8mat3 = matrix3<s8>;
using s16mat3 = matrix3<s16>;
using smat3 = matrix3<s32>;
using s64mat3 = matrix3<s64>;
using u8mat3 = matrix3<u8>;
using u16mat3 = matrix3<u16>;
using umat3 = matrix3<u32>;
using u64mat3 = matrix3<u64>;
using mat3 = matrix3<f32>;
using f64mat3 = matrix3<f64>;
} // namespace nslib
