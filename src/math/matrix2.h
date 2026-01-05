#pragma once

#include "vector2.h"

namespace nslib
{

template<class T>
struct matrix2
{
    matrix2() : data{{1, 0}, {0, 1}}
    {}

    matrix2(const T &val) : data{{val}, {val}}
    {}

    matrix2(const vector2<T> &row1_, const vector2<T> &row2_) : row1(row1_), row2(row2_)
    {}

    matrix2(T data_[4]): elements{data_[0], data_[1], data_[2], data_[3]}
    {}

    matrix2(T data_[2][2]): data{data_[0], data_[1]}
    {}

    COMMON_OPERATORS(matrix2, 2, vector2<T>)

#if NOBLE_STEED_SIMD
    using _simd_type = __m128;
#endif

    vector2<T> operator()(sizet val_) const
    {
        return vector2<T>(data[0][val_], data[1][val_]);
    }

    union
    {
        T elements[size_*vector2<T>::size_];
        vector2<T> data[size_];
        struct
        {
            vector2<T> row1;
            vector2<T> row2;
        };
#if NOBLE_STEED_SIMD
        _simd_type _packed;
#endif
    };
};

pup_func_tt(matrix2)
{
    pup_member(row1);
    pup_member(row2);
}    

// Enable type trait
template<class U>
struct is_mat<matrix2<U>>
{
    static constexpr bool value = true;
};

namespace math
{

template<class T>
T determinant(const matrix2<T> &mat)
{
    return mat.data[0].x * mat.data[1].y - mat.data[1].x * mat.data[0].y;
}

template<floating_pt T>
matrix2<T> inverse(const matrix2<T> &mat)
{
    matrix2<T> ret;
    T inv_det = (T)1 / determinant(mat);
    ret.data[0][0] = mat.data[1][1] * inv_det;
    ret.data[0][1] = -mat.data[0][1] * inv_det;
    ret.data[1][0] = -mat.data[1][0] * inv_det;
    ret.data[1][1] = mat.data[0][0] * inv_det;
    return ret;
}

template<class T>
void transpose(matrix2<T> *mat)
{
    T tmp = mat->data[1][0];
    mat->data[1][0] = mat->data[0][1];
    mat->data[0][1] = tmp;
}

template<class T>
matrix2<T> transpose(matrix2<T> mat)
{
    transpose(&mat);
    return mat;
}

template<floating_pt T>
matrix2<T> rotation_mat(T radian_angle)
{
    matrix2<T> ret;
    ret.data[0][0] = math::cos(radian_angle);
    ret.data[0][1] = math::sin(radian_angle);
    ret.data[1][0] = -math::sin(radian_angle);
    ret.data[1][1] = math::cos(radian_angle);
    return ret;
}

template<class T>
matrix2<T> scaling_mat(const vector2<T> &scale_)
{
    matrix2<T> ret;
    ret.data[0][0] = scale_.x;
    ret.data[1][1] = scale_.y;
    return ret;
}

template<class T>
matrix2<T> scaling_mat(const matrix2<T> &transform)
{
    matrix2<T> ret;
    ret.data[0][0] = length(transform[0]);
    ret.data[1][1] = length(transform[1]);
    return ret;
}

template<class T>
vector2<T> scaling_vec(const matrix2<T> &tform)
{
    return {length(tform[0]), length(tform[1])};
}

template<class T>
vector2<T> translation_vec(const matrix2<T> &tform)
{
    return tform(2);
}

#if NOBLE_STEED_SIMD
template<>
inline void transpose(matrix2<float> *mat)
{
    mat->_packed = _mm_shuffle_ps(mat->_packed, mat->_packed, 0b11011000);
}

template<>
inline void compwise_mult(matrix2<float> *lhs, const matrix2<float> &rhs)
{
    lhs->_packed = _mm_mul_ps(lhs->_packed, rhs._packed);
}

template<>
inline void compwise_div(matrix2<float> *lhs, const matrix2<float> &rhs)
{
    lhs->_packed = _mm_div_ps(lhs->_packed, rhs._packed);
}

template<>
inline void compwise_mult_rows(matrix2<float> *lhs, const vector2<float> &row_vec)
{
    __m128 rv = _mm_set_ps(row_vec.y, row_vec.x, row_vec.y, row_vec.x);
    lhs->_packed = _mm_mul_ps(lhs->_packed, rv);
}

template<>
inline void compwise_div_rows(matrix2<float> *lhs, const vector2<float> &row_vec)
{
    __m128 rv = _mm_set_ps(row_vec.y, row_vec.x, row_vec.y, row_vec.x);
    lhs->_packed = _mm_div_ps(lhs->_packed, rv);
}

template<>
inline void compwise_div_rows(const vector2<float> &row_vec, matrix2<float> *lhs)
{
    __m128 rv = _mm_set_ps(row_vec.y, row_vec.x, row_vec.y, row_vec.x);
    lhs->_packed = _mm_div_ps(rv, lhs->_packed);
}

template<>
inline void compwise_mult_columns(matrix2<float> *lhs, const vector2<float> &col_vec)
{
    __m128 rv = _mm_set_ps(col_vec.y, col_vec.y, col_vec.x, col_vec.x);
    lhs->_packed = _mm_mul_ps(lhs->_packed, rv);
}

template<>
inline void compwise_div_columns(matrix2<float> *lhs, const vector2<float> &col_vec)
{
    __m128 rv = _mm_set_ps(col_vec.y, col_vec.y, col_vec.x, col_vec.x);
    lhs->_packed = _mm_div_ps(lhs->_packed, rv);
}

template<>
inline void compwise_div_columns(const vector2<float> &col_vec, matrix2<float> *lhs)
{
    __m128 rv = _mm_set_ps(col_vec.y, col_vec.y, col_vec.x, col_vec.x);
    lhs->_packed = _mm_div_ps(rv, lhs->_packed);
}

#endif

} // namespace math


// Overloaded operators
template<class T>
matrix2<T> operator*(const matrix2<T> &lhs, const matrix2<T> &rhs)
{
    matrix2<T> ret;
    ret.data[0][0] = lhs.data[0][0] * rhs.data[0][0] + lhs.data[0][1] * rhs.data[1][0];
    ret.data[0][1] = lhs.data[0][0] * rhs.data[0][1] + lhs.data[0][1] * rhs.data[1][1];
    ret.data[1][0] = lhs.data[1][0] * rhs.data[0][0] + lhs.data[1][1] * rhs.data[1][0];
    ret.data[1][1] = lhs.data[1][0] * rhs.data[0][1] + lhs.data[1][1] * rhs.data[1][1];
    return ret;
}

template<class T>
matrix2<T> operator/(const matrix2<T> &lhs, const matrix2<T> &rhs)
{
    return lhs * math::inverse(rhs);
}

template<class T>
vector2<T> operator*(const matrix2<T> &lhs, const vector2<T> &rhs)
{
    return {math::dot(lhs.row1, rhs), math::dot(lhs.row2, rhs)};
}

template<floating_pt T>
vector2<T> operator/(const matrix2<T> &lhs, const vector2<T> &rhs)
{
    T mult = 1 / math::dot(rhs, rhs);
    return {math::dot(lhs[0], rhs) * mult, math::dot(lhs[1], rhs) * mult};
}

template<integral T>
vector2<T> operator/(const matrix2<T> &lhs, const vector2<T> &rhs)
{
    T lensq = math::dot(rhs, rhs);
    return {math::dot(lhs[0], rhs) / lensq, math::dot(lhs[1], rhs) / lensq};
}

template<class T>
vector2<T> operator*(const vector2<T> &lhs, const matrix2<T> &rhs)
{
    vector2<T> ret;
    ret[0] = lhs[0] * rhs[0][0] + lhs[1] * rhs[1][0];
    ret[1] = lhs[0] * rhs[0][1] + lhs[1] * rhs[1][1];
    return ret;
}

template<class T>
vector2<T> operator/(const vector2<T> &lhs, const matrix2<T> &rhs)
{
    return lhs * math::inverse(rhs);
}

#if NOBLE_STEED_SIMD

inline matrix2<float> operator+(matrix2<float> lhs, const matrix2<float> &rhs)
{
    lhs._packed = _mm_add_ps(lhs._packed, rhs._packed);
    return lhs;
}

inline matrix2<float> operator-(matrix2<float> lhs, const matrix2<float> &rhs)
{
    lhs._packed = _mm_sub_ps(lhs._packed, rhs._packed);
    return lhs;
}

template<arithmetic_type T>
inline matrix2<float> operator*(matrix2<float> lhs, T rhs)
{
    __m128 s = _mm_set1_ps(rhs);
    lhs._packed = _mm_mul_ps(lhs._packed, s);
    return lhs;
}

template<arithmetic_type T>
inline matrix2<float> operator/(matrix2<float> lhs, T rhs)
{
    __m128 s = _mm_set1_ps(1.0 / rhs);
    lhs._packed = _mm_mul_ps(lhs._packed, s);
    return lhs;
}

inline vector2<float> operator*(const vector2<float> &lhs, const matrix2<float> &rhs)
{
    return math::transpose(rhs) * lhs;
}

#endif

using s8mat2 = matrix2<s8>;
using s16mat2 = matrix2<s16>;
using smat2 = matrix2<s32>;
using s64mat2 = matrix2<s64>;
using u8mat2 = matrix2<u8>;
using u16mat2 = matrix2<u16>;
using umat2 = matrix2<u32>;
using u64mat2 = matrix2<u64>;
using mat2 = matrix2<f32>;
using f64mat2 = matrix2<f64>;
} // namespace nslib
