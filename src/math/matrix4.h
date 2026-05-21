#pragma once

#include "matrix3.h"

namespace nslib
{

template<class T>
struct matrix4
{
    // Identity
    matrix4() : data{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}
    {}

    matrix4(const T &val_) : data{{val_}, {val_}, {val_}, {val_}}
    {}

    matrix4(const vector4<T> &row1_, const vector4<T> &row2_, const vector4<T> &row3_, const vector4<T> &row4_)
        : row1(row1_), row2(row2_), row3(row3_), row4(row4_)
    {}

    matrix4(const matrix3<T> &basis, const vector3<T> &col4 = {}, const vector4<T> &row4_ = {0, 0, 0, 1})
        : row1(basis[0], col4[0]), row2(basis[1], col4[1]), row3(basis[2], col4[2]), row4(row4_)
    {}

    matrix4(T data_[16])
        : elements{data_[0],
                   data_[1],
                   data_[2],
                   data_[3],
                   data_[4],
                   data_[5],
                   data_[6],
                   data_[7],
                   data_[8],
                   data_[9],
                   data_[10],
                   data_[11],
                   data_[12],
                   data_[13],
                   data_[14],
                   data_[15]}
    {}

    matrix4(T data_[4][4]) : data{data_[0], data_[1], data_[2], data_[3]}
    {}

    COMMON_OPERATORS(matrix4, 4, vector4<T>)
#if NOBLE_STEED_SIMD
    using _simd_type = typename simd_traits<T, size_>::_simd_type;
#endif

    vector4<T> operator()(s8 ind) const
    {
        return {data[0][ind], data[1][ind], data[2][ind], data[3][ind]};
    }

    union
    {
        T elements[size_ * vector4<T>::size_];
        vector4<T> data[size_];
        struct
        {
            vector4<T> row1;
            vector4<T> row2;
            vector4<T> row3;
            vector4<T> row4;
        };
#if NOBLE_STEED_SIMD
        _simd_type _data[size_];
#endif
    };
};

pup_func_tt(matrix4)
{
    pup_member(row1);
    pup_member(row2);
    pup_member(row3);
    pup_member(row4);
}

// Enable type trait
template<class U>
struct is_mat<matrix4<U>>
{
    static constexpr bool value = true;
};

namespace math
{

template<class T>
matrix3<T> basis(const matrix4<T> &mat)
{
    return {mat.row1.xyz, mat.row2.xyz, mat.row3.xyz};
}

template<class T>
void transpose(matrix4<T> *mat)
{
    T tmp = (*mat)[1][0];
    (*mat)[1][0] = (*mat)[0][1];
    (*mat)[0][1] = tmp;
    tmp = (*mat)[2][0];
    (*mat)[2][0] = (*mat)[0][2];
    (*mat)[0][2] = tmp;
    tmp = (*mat)[2][1];
    (*mat)[2][1] = (*mat)[1][2];
    (*mat)[1][2] = tmp;

    tmp = (*mat)[3][0];
    (*mat)[3][0] = (*mat)[0][3];
    (*mat)[0][3] = tmp;
    tmp = (*mat)[3][1];
    (*mat)[3][1] = (*mat)[1][3];
    (*mat)[1][3] = tmp;
    tmp = (*mat)[3][2];
    (*mat)[3][2] = (*mat)[2][3];
    (*mat)[2][3] = tmp;
}

template<class T>
inline matrix4<T> transpose(matrix4<T> mat)
{
    transpose(&mat);
    return mat;
}

template<class T>
T determinant(const matrix4<T> &mat)
{
    // Thanks to Urho3D Matrix4 Source Code
    T v0 = mat.data[2][0] * mat.data[3][1] - mat.data[2][1] * mat.data[3][0];
    T v1 = mat.data[2][0] * mat.data[3][2] - mat.data[2][2] * mat.data[3][0];
    T v2 = mat.data[2][0] * mat.data[3][3] - mat.data[2][3] * mat.data[3][0];
    T v3 = mat.data[2][1] * mat.data[3][2] - mat.data[2][2] * mat.data[3][1];
    T v4 = mat.data[2][1] * mat.data[3][3] - mat.data[2][3] * mat.data[3][1];
    T v5 = mat.data[2][2] * mat.data[3][3] - mat.data[2][3] * mat.data[3][2];

    T i00 = (v5 * mat.data[1][1] - v4 * mat.data[1][2] + v3 * mat.data[1][3]);
    T i10 = -(v5 * mat.data[1][0] - v2 * mat.data[1][2] + v1 * mat.data[1][3]);
    T i20 = (v4 * mat.data[1][0] - v2 * mat.data[1][1] + v0 * mat.data[1][3]);
    T i30 = -(v3 * mat.data[1][0] - v1 * mat.data[1][1] + v0 * mat.data[1][2]);
    return (i00 * mat.data[0][0] + i10 * mat.data[0][1] + i20 * mat.data[0][2] + i30 * mat.data[0][3]);
}

template<floating_pt T>
matrix4<T> inverse(const matrix4<T> &mat)
{
    // Thanks to Urho3D Matrix4 Source Code
    matrix4<T> ret;
    T v0 = mat.data[2][0] * mat.data[3][1] - mat.data[2][1] * mat.data[3][0];
    T v1 = mat.data[2][0] * mat.data[3][2] - mat.data[2][2] * mat.data[3][0];
    T v2 = mat.data[2][0] * mat.data[3][3] - mat.data[2][3] * mat.data[3][0];
    T v3 = mat.data[2][1] * mat.data[3][2] - mat.data[2][2] * mat.data[3][1];
    T v4 = mat.data[2][1] * mat.data[3][3] - mat.data[2][3] * mat.data[3][1];
    T v5 = mat.data[2][2] * mat.data[3][3] - mat.data[2][3] * mat.data[3][2];

    ret.data[0][0] = (v5 * mat.data[1][1] - v4 * mat.data[1][2] + v3 * mat.data[1][3]);
    ret.data[1][0] = -(v5 * mat.data[1][0] - v2 * mat.data[1][2] + v1 * mat.data[1][3]);
    ret.data[2][0] = (v4 * mat.data[1][0] - v2 * mat.data[1][1] + v0 * mat.data[1][3]);
    ret.data[3][0] = -(v3 * mat.data[1][0] - v1 * mat.data[1][1] + v0 * mat.data[1][2]);
    T det = (ret.data[0][0] * mat.data[0][0] + ret.data[1][0] * mat.data[0][1] + ret.data[2][0] * mat.data[0][2] +
             ret.data[3][0] * mat.data[0][3]);

    T inv_det = (T)1 / det;

    ret.data[0][0] *= inv_det;
    ret.data[1][0] *= inv_det;
    ret.data[2][0] *= inv_det;
    ret.data[3][0] *= inv_det;

    ret.data[0][1] = -(v5 * mat.data[0][1] - v4 * mat.data[0][2] + v3 * mat.data[0][3]) * inv_det;
    ret.data[1][1] = (v5 * mat.data[0][0] - v2 * mat.data[0][2] + v1 * mat.data[0][3]) * inv_det;
    ret.data[2][1] = -(v4 * mat.data[0][0] - v2 * mat.data[0][1] + v0 * mat.data[0][3]) * inv_det;
    ret.data[3][1] = (v3 * mat.data[0][0] - v1 * mat.data[0][1] + v0 * mat.data[0][2]) * inv_det;

    v0 = mat.data[1][0] * mat.data[3][1] - mat.data[1][1] * mat.data[3][0];
    v1 = mat.data[1][0] * mat.data[3][2] - mat.data[1][2] * mat.data[3][0];
    v2 = mat.data[1][0] * mat.data[3][3] - mat.data[1][3] * mat.data[3][0];
    v3 = mat.data[1][1] * mat.data[3][2] - mat.data[1][2] * mat.data[3][1];
    v4 = mat.data[1][1] * mat.data[3][3] - mat.data[1][3] * mat.data[3][1];
    v5 = mat.data[1][2] * mat.data[3][3] - mat.data[1][3] * mat.data[3][2];

    ret.data[0][2] = (v5 * mat.data[0][1] - v4 * mat.data[0][2] + v3 * mat.data[0][3]) * inv_det;
    ret.data[1][2] = -(v5 * mat.data[0][0] - v2 * mat.data[0][2] + v1 * mat.data[0][3]) * inv_det;
    ret.data[2][2] = (v4 * mat.data[0][0] - v2 * mat.data[0][1] + v0 * mat.data[0][3]) * inv_det;
    ret.data[3][2] = -(v3 * mat.data[0][0] - v1 * mat.data[0][1] + v0 * mat.data[0][2]) * inv_det;

    v0 = mat.data[2][1] * mat.data[1][0] - mat.data[2][0] * mat.data[1][1];
    v1 = mat.data[2][2] * mat.data[1][0] - mat.data[2][0] * mat.data[1][2];
    v2 = mat.data[2][3] * mat.data[1][0] - mat.data[2][0] * mat.data[1][3];
    v3 = mat.data[2][2] * mat.data[1][1] - mat.data[2][1] * mat.data[1][2];
    v4 = mat.data[2][3] * mat.data[1][1] - mat.data[2][1] * mat.data[1][3];
    v5 = mat.data[2][3] * mat.data[1][2] - mat.data[2][2] * mat.data[1][3];

    ret.data[0][3] = -(v5 * mat.data[0][1] - v4 * mat.data[0][2] + v3 * mat.data[0][3]) * inv_det;
    ret.data[1][3] = (v5 * mat.data[0][0] - v2 * mat.data[0][2] + v1 * mat.data[0][3]) * inv_det;
    ret.data[2][3] = -(v4 * mat.data[0][0] - v2 * mat.data[0][1] + v0 * mat.data[0][3]) * inv_det;
    ret.data[3][3] = (v3 * mat.data[0][0] - v1 * mat.data[0][1] + v0 * mat.data[0][2]) * inv_det;
    return ret;
}

template<floating_pt T>
matrix4<T> ortho(T left, T right, T top, T bottom, T nearz, T farz)
{
    T w = right - left;
    T h = top - bottom;
    T p = farz - nearz;

    T x = (right + left) / w;
    T y = (top + bottom) / h;

    matrix4<T> ret;
    set_mat_column(&ret, 3, vector4<T>{-x, -y, -nearz / p, 1});
    ret[0][0] = 2 / w;
    ret[1][1] = 2 / h;
    ret[2][2] = 1 / p;
    return ret;
}

template<floating_pt T>
matrix4<T> perspective_vfov(T fov, T aspect_ratio, T z_near, T z_far)
{
    T z_range = z_far - z_near;
    T height = T(1) / math::tan((fov * T(0.5)) * TO_RADS);
    matrix4<T> ret{};
    ret[0][0] = height * (T(1) / aspect_ratio);
    ret[1][1] = height;
    ret[2][2] = z_far / z_range;
    ret[2][3] = -z_far * z_near / z_range;
    ret[3][2] = T(1);
    ret[3][3] = T(0);
    return ret;
}

template<floating_pt T>
matrix4<T> perspective_hfov(T fov_h_deg, T aspect_ratio, T z_near, T z_far)
{
    const T z_range = z_far - z_near;
    const T fx = T(1) / math::tan((fov_h_deg * T(0.5)) * TO_RADS);
    matrix4<T> ret{};
    ret[0][0] = fx;
    ret[1][1] = fx * aspect_ratio; // fy = fx * (w/h)
    ret[2][2] = z_far / z_range;
    ret[2][3] = -z_far * z_near / z_range;
    ret[3][2] = T(1);
    ret[3][3] = T(0);
    return ret;
}


template<floating_pt T>
matrix4<T> look_at(const vector3<T> &eye_pos, const vector3<T> &target_pos, const vector3<T> &up_dir = {0, 1, 0})
{
    matrix4<T> trans;
    // target
    trans[2] = {normalize(target_pos - eye_pos), 0};
    // right - cross global up direction with target
    trans[0] = {normalize(cross(up_dir, trans[2].xyz)), 0};
    // up - cross target with right
    trans[1] = {cross(trans[2].xyz, trans[0].xyz), 0};

    // This is the same as doing the matrix multiplication rot * trans (normally would do trans * rot but camera is reverse that)
    trans[0][3] = -dot(trans[0].xyz, eye_pos);
    trans[1][3] = -dot(trans[1].xyz, eye_pos);
    trans[2][3] = -dot(trans[2].xyz, eye_pos);
    return trans;
}

template<floating_pt T>
matrix4<T> model_tform(const vector3<T> &pos={}, const quaternion<T> & orientation={}, const vector3<T> &scale={1})
{
    matrix4<T> ret{math::rotation_mat(orientation), pos};
    compwise_mult_rows(&ret, vec4{scale, 1});
    return ret;
}

template<class T>
vector3<T> right_vec(const matrix4<T> &mat)
{
    return normalize(mat(0).xyz);
}

template<class T>
vector3<T> target_vec(const matrix4<T> &mat)
{
    return normalize(mat(2).xyz);
}

template<class T>
vector3<T> up_vec(const matrix4<T> &mat)
{
    return normalize(mat(1).xyz);
}

template<class T>
matrix3<T> rotation_mat(const matrix4<T> &transform)
{
    matrix3<T> ret(basis(transform));
    return rotation_mat(ret);
}

template<class T>
quaternion<T> orientation(const matrix4<T> &transform)
{
    return orientation(rotation_mat(transform));
}

template<class T>
matrix3<T> scaling_mat(const matrix4<T> &transform)
{
    matrix3<T> ret(basis(transform));
    return scaling_mat(ret);
}

template<class T>
vector3<T> scaling_vec(const matrix4<T> &transform)
{
    return {length(transform[0].xyz), length(transform[1].xyz), length(transform[2].xyz)};
}

template<class T>
vector3<T> translation_vec(const matrix4<T> &transform)
{
    return transform(3).xyz;
}

#if NOBLE_STEED_SIMD

float determinant(const matrix4<float> &mat);

matrix4<float> inverse(const matrix4<float> &mat);

template<>
inline void transpose(matrix4<float> *mat)
{
    _MM_TRANSPOSE4_PS(mat->_data[0], mat->_data[1], mat->_data[2], mat->_data[3]);
}

template<>
inline void compwise_mult(matrix4<float> *lhs, const matrix4<float> &rhs)
{
    lhs->_data[0] = _mm_mul_ps(lhs->_data[0], rhs._data[0]);
    lhs->_data[1] = _mm_mul_ps(lhs->_data[1], rhs._data[1]);
    lhs->_data[2] = _mm_mul_ps(lhs->_data[2], rhs._data[2]);
    lhs->_data[3] = _mm_mul_ps(lhs->_data[3], rhs._data[3]);
}

template<>
inline void compwise_mult(matrix3<float> *lhs, const matrix3<float> &rhs)
{
    matrix4<float> m4(*lhs);
    matrix4<float> m4_rhs(rhs);
    m4._data[0] = _mm_mul_ps(m4._data[0], m4_rhs._data[0]);
    m4._data[1] = _mm_mul_ps(m4._data[1], m4_rhs._data[1]);
    m4._data[2] = _mm_mul_ps(m4._data[2], m4_rhs._data[2]);
    *lhs = math::basis(m4);
}

template<>
inline void compwise_div(matrix4<float> *lhs, const matrix4<float> &rhs)
{
    lhs->_data[0] = _mm_div_ps(lhs->_data[0], rhs._data[0]);
    lhs->_data[1] = _mm_div_ps(lhs->_data[1], rhs._data[1]);
    lhs->_data[2] = _mm_div_ps(lhs->_data[2], rhs._data[2]);
    lhs->_data[3] = _mm_div_ps(lhs->_data[3], rhs._data[3]);
}

template<>
inline void compwise_div(matrix3<float> *lhs, const matrix3<float> &rhs)
{
    matrix4<float> m4(*lhs);
    matrix4<float> m4_rhs(rhs);
    m4._data[0] = _mm_div_ps(m4._data[0], m4_rhs._data[0]);
    m4._data[1] = _mm_div_ps(m4._data[1], m4_rhs._data[1]);
    m4._data[2] = _mm_div_ps(m4._data[2], m4_rhs._data[2]);
    *lhs = math::basis(m4);
}

template<>
inline void compwise_mult_rows(matrix4<float> *lhs, const vector4<float> &row_vec)
{
    lhs->_data[0] = _mm_mul_ps(lhs->_data[0], row_vec._v4);
    lhs->_data[1] = _mm_mul_ps(lhs->_data[1], row_vec._v4);
    lhs->_data[2] = _mm_mul_ps(lhs->_data[2], row_vec._v4);
    lhs->_data[3] = _mm_mul_ps(lhs->_data[3], row_vec._v4);
}

template<>
inline void compwise_mult_rows(matrix3<float> *lhs, const vector3<float> &row_vec)
{
    matrix4<float> m4(*lhs);
    vector4<float> v4(row_vec);
    m4._data[0] = _mm_mul_ps(m4._data[0], v4._v4);
    m4._data[1] = _mm_mul_ps(m4._data[1], v4._v4);
    m4._data[2] = _mm_mul_ps(m4._data[2], v4._v4);
    *lhs = math::basis(m4);
}

template<>
inline void compwise_div_rows(matrix4<float> *lhs, const vector4<float> &row_vec)
{
    lhs->_data[0] = _mm_div_ps(lhs->_data[0], row_vec._v4);
    lhs->_data[1] = _mm_div_ps(lhs->_data[1], row_vec._v4);
    lhs->_data[2] = _mm_div_ps(lhs->_data[2], row_vec._v4);
    lhs->_data[3] = _mm_div_ps(lhs->_data[3], row_vec._v4);
}

template<>
inline void compwise_div_rows(matrix3<float> *lhs, const vector3<float> &row_vec)
{
    matrix4<float> m4(*lhs);
    vector4<float> v4(row_vec);
    m4._data[0] = _mm_div_ps(m4._data[0], v4._v4);
    m4._data[1] = _mm_div_ps(m4._data[1], v4._v4);
    m4._data[2] = _mm_div_ps(m4._data[2], v4._v4);
    *lhs = math::basis(m4);
}

template<>
inline void compwise_div_rows(const vector4<float> &row_vec, matrix4<float> *rhs)
{
    rhs->_data[0] = _mm_div_ps(row_vec._v4, rhs->_data[0]);
    rhs->_data[1] = _mm_div_ps(row_vec._v4, rhs->_data[1]);
    rhs->_data[2] = _mm_div_ps(row_vec._v4, rhs->_data[2]);
    rhs->_data[3] = _mm_div_ps(row_vec._v4, rhs->_data[3]);
}

template<>
inline void compwise_div_rows(const vector3<float> &row_vec, matrix3<float> *rhs)
{
    matrix4<float> m4(*rhs);
    vector4<float> v4(row_vec);
    m4._data[0] = _mm_div_ps(v4._v4, m4._data[0]);
    m4._data[1] = _mm_div_ps(v4._v4, m4._data[1]);
    m4._data[2] = _mm_div_ps(v4._v4, m4._data[2]);
    *rhs = math::basis(m4);
}

template<>
inline void compwise_mult_columns(matrix4<float> *lhs, const vector4<float> &col_vec)
{
    transpose(lhs);
    lhs->_data[0] = _mm_mul_ps(lhs->_data[0], col_vec._v4);
    lhs->_data[1] = _mm_mul_ps(lhs->_data[1], col_vec._v4);
    lhs->_data[2] = _mm_mul_ps(lhs->_data[2], col_vec._v4);
    lhs->_data[3] = _mm_mul_ps(lhs->_data[3], col_vec._v4);
    transpose(lhs);
}

template<>
inline void compwise_div_columns(matrix4<float> *lhs, const vector4<float> &col_vec)
{
    transpose(lhs);
    lhs->_data[0] = _mm_div_ps(lhs->_data[0], col_vec._v4);
    lhs->_data[1] = _mm_div_ps(lhs->_data[1], col_vec._v4);
    lhs->_data[2] = _mm_div_ps(lhs->_data[2], col_vec._v4);
    lhs->_data[3] = _mm_div_ps(lhs->_data[3], col_vec._v4);
    transpose(lhs);
}

template<>
inline void compwise_div_columns(const vector4<float> &col_vec, matrix4<float> *rhs)
{
    transpose(rhs);
    rhs->_data[0] = _mm_div_ps(col_vec._v4, rhs->_data[0]);
    rhs->_data[1] = _mm_div_ps(col_vec._v4, rhs->_data[1]);
    rhs->_data[2] = _mm_div_ps(col_vec._v4, rhs->_data[2]);
    rhs->_data[3] = _mm_div_ps(col_vec._v4, rhs->_data[3]);
    transpose(rhs);
}

#endif

} // namespace math

template<class T>
matrix4<T> operator*(const matrix4<T> &lhs, const matrix4<T> &rhs)
{
    matrix4<T> ret;
    ret[0][0] = lhs[0][0] * rhs[0][0] + lhs[0][1] * rhs[1][0] + lhs[0][2] * rhs[2][0] + lhs[0][3] * rhs[3][0];
    ret[0][1] = lhs[0][0] * rhs[0][1] + lhs[0][1] * rhs[1][1] + lhs[0][2] * rhs[2][1] + lhs[0][3] * rhs[3][1];
    ret[0][2] = lhs[0][0] * rhs[0][2] + lhs[0][1] * rhs[1][2] + lhs[0][2] * rhs[2][2] + lhs[0][3] * rhs[3][2];
    ret[0][3] = lhs[0][0] * rhs[0][3] + lhs[0][1] * rhs[1][3] + lhs[0][2] * rhs[2][3] + lhs[0][3] * rhs[3][3];

    ret[1][0] = lhs[1][0] * rhs[0][0] + lhs[1][1] * rhs[1][0] + lhs[1][2] * rhs[2][0] + lhs[1][3] * rhs[3][0];
    ret[1][1] = lhs[1][0] * rhs[0][1] + lhs[1][1] * rhs[1][1] + lhs[1][2] * rhs[2][1] + lhs[1][3] * rhs[3][1];
    ret[1][2] = lhs[1][0] * rhs[0][2] + lhs[1][1] * rhs[1][2] + lhs[1][2] * rhs[2][2] + lhs[1][3] * rhs[3][2];
    ret[1][3] = lhs[1][0] * rhs[0][3] + lhs[1][1] * rhs[1][3] + lhs[1][2] * rhs[2][3] + lhs[1][3] * rhs[3][3];

    ret[2][0] = lhs[2][0] * rhs[0][0] + lhs[2][1] * rhs[1][0] + lhs[2][2] * rhs[2][0] + lhs[2][3] * rhs[3][0];
    ret[2][1] = lhs[2][0] * rhs[0][1] + lhs[2][1] * rhs[1][1] + lhs[2][2] * rhs[2][1] + lhs[2][3] * rhs[3][1];
    ret[2][2] = lhs[2][0] * rhs[0][2] + lhs[2][1] * rhs[1][2] + lhs[2][2] * rhs[2][2] + lhs[2][3] * rhs[3][2];
    ret[2][3] = lhs[2][0] * rhs[0][3] + lhs[2][1] * rhs[1][3] + lhs[2][2] * rhs[2][3] + lhs[2][3] * rhs[3][3];

    ret[3][0] = lhs[3][0] * rhs[0][0] + lhs[3][1] * rhs[1][0] + lhs[3][2] * rhs[2][0] + lhs[3][3] * rhs[3][0];
    ret[3][1] = lhs[3][0] * rhs[0][1] + lhs[3][1] * rhs[1][1] + lhs[3][2] * rhs[2][1] + lhs[3][3] * rhs[3][1];
    ret[3][2] = lhs[3][0] * rhs[0][2] + lhs[3][1] * rhs[1][2] + lhs[3][2] * rhs[2][2] + lhs[3][3] * rhs[3][2];
    ret[3][3] = lhs[3][0] * rhs[0][3] + lhs[3][1] * rhs[1][3] + lhs[3][2] * rhs[2][3] + lhs[3][3] * rhs[3][3];
    return ret;
}

template<class T>
matrix4<T> operator/(const matrix4<T> &lhs, const matrix4<T> &rhs)
{
    return lhs * math::inverse(rhs);
}

template<class T>
inline vector4<T> operator*(const matrix4<T> &lhs, const vector4<T> &rhs)
{
    using namespace math;
    return {dot(lhs[0], rhs), dot(lhs[1], rhs), dot(lhs[2], rhs), dot(lhs[3], rhs)};
}

template<floating_pt T>
inline vector4<T> operator/(const matrix4<T> &lhs, const vector4<T> &rhs)
{
    using namespace math;
    T mult = 1 / dot(rhs, rhs);
    return {dot(lhs[0], rhs) * mult, dot(lhs[1], rhs) * mult, dot(lhs[2], rhs) * mult, dot(lhs[3], rhs) * mult};
}

template<integral T>
inline vector4<T> operator/(const matrix4<T> &lhs, const vector4<T> &rhs)
{
    using namespace math;
    T lensq = dot(rhs, rhs);
    return {dot(lhs[0], rhs) / lensq, dot(lhs[1], rhs) / lensq, dot(lhs[2], rhs) / lensq, dot(lhs[3] * rhs) / lensq};
}

template<class T>
vector4<T> operator*(const vector4<T> &lhs, const matrix4<T> &rhs)
{
    vector4<T> ret;
    ret[0] = lhs[0] * rhs[0][0] + lhs[1] * rhs[1][0] + lhs[2] * rhs[2][0] + lhs[3] * rhs[3][0];
    ret[1] = lhs[0] * rhs[0][1] + lhs[1] * rhs[1][1] + lhs[2] * rhs[2][1] + lhs[3] * rhs[3][1];
    ret[2] = lhs[0] * rhs[0][2] + lhs[1] * rhs[1][2] + lhs[2] * rhs[2][2] + lhs[3] * rhs[3][2];
    ret[3] = lhs[0] * rhs[0][3] + lhs[1] * rhs[1][3] + lhs[2] * rhs[2][3] + lhs[3] * rhs[3][3];
    return ret;
}

template<class T>
vector4<T> operator/(const vector4<T> &lhs, const matrix4<T> &rhs)
{
    return lhs * math::inverse(rhs);
}

#if NOBLE_STEED_SIMD

inline __m128 _linear_combine_sse(const __m128 &left, const matrix4<float> &right)
{
    __m128 res;
    res = _mm_mul_ps(_mm_shuffle_ps(left, left, 0x00), right._data[0]);
    res = _mm_add_ps(res, _mm_mul_ps(_mm_shuffle_ps(left, left, 0x55), right._data[1]));
    res = _mm_add_ps(res, _mm_mul_ps(_mm_shuffle_ps(left, left, 0xaa), right._data[2]));
    res = _mm_add_ps(res, _mm_mul_ps(_mm_shuffle_ps(left, left, 0xff), right._data[3]));
    return (res);
}

inline __m128 _linear_combine_v4_sse(const matrix4<float> &left, const __m128 &right)
{
    __m128 res;
    res = _mm_mul_ps(left._data[0], right);
    res = _mm_add_ps(res, _mm_mul_ps(left._data[1], right));
    res = _mm_add_ps(res, _mm_mul_ps(left._data[2], right));
    res = _mm_add_ps(res, _mm_mul_ps(left._data[3], right));
    return (res);
}

inline matrix4<float> operator*(matrix4<float> lhs, const matrix4<float> &rhs)
{
    lhs._data[0] = _linear_combine_sse(lhs._data[0], rhs);
    lhs._data[1] = _linear_combine_sse(lhs._data[1], rhs);
    lhs._data[2] = _linear_combine_sse(lhs._data[2], rhs);
    lhs._data[3] = _linear_combine_sse(lhs._data[3], rhs);
    return lhs;
}

inline matrix3<float> operator*(const matrix3<float> &lhs, const matrix3<float> &rhs)
{
    matrix4<float> m4(lhs);
    m4._data[0] = _linear_combine_sse(m4._data[0], rhs);
    m4._data[1] = _linear_combine_sse(m4._data[1], rhs);
    m4._data[2] = _linear_combine_sse(m4._data[2], rhs);
    return math::basis(m4);
}

inline vector4<float> operator*(const matrix4<float> &lhs, const vector4<float> &rhs)
{
    vector4<float> ret;
    ret.data[0] = _mm_cvtss_f32(math::_sse_dp(lhs._data[0], rhs._v4));
    ret.data[1] = _mm_cvtss_f32(math::_sse_dp(lhs._data[1], rhs._v4));
    ret.data[2] = _mm_cvtss_f32(math::_sse_dp(lhs._data[2], rhs._v4));
    ret.data[3] = _mm_cvtss_f32(math::_sse_dp(lhs._data[3], rhs._v4));
    return ret;
}

inline vector3<float> operator*(const matrix3<float> &lhs, const vector3<float> &rhs)
{
    matrix4<float> m4(lhs);
    vector4<float> v4(rhs);
    vector3<float> ret;
    ret.data[0] = _mm_cvtss_f32(math::_sse_dp(m4._data[0], v4._v4));
    ret.data[1] = _mm_cvtss_f32(math::_sse_dp(m4._data[1], v4._v4));
    ret.data[2] = _mm_cvtss_f32(math::_sse_dp(m4._data[2], v4._v4));
    return ret;
}

inline vector4<float> operator*(const vector4<float> &lhs, const matrix4<float> &rhs)
{
    return math::transpose(rhs) * lhs;
}

template<arithmetic_type T>
inline matrix4<float> operator*(matrix4<float> lhs, T rhs)
{
    __m128 r = _mm_set_ss(rhs);
    lhs._data[0] = _mm_mul_ps(lhs._data[0], r);
    lhs._data[1] = _mm_mul_ps(lhs._data[1], r);
    lhs._data[2] = _mm_mul_ps(lhs._data[2], r);
    lhs._data[3] = _mm_mul_ps(lhs._data[3], r);
    return lhs;
}

template<arithmetic_type T>
inline matrix3<float> operator*(const matrix3<float> &lhs, T rhs)
{
    __m128 r = _mm_set_ss(rhs);
    matrix4<T> m4(lhs);
    m4._data[0] = _mm_mul_ps(m4._data[0], r);
    m4._data[1] = _mm_mul_ps(m4._data[1], r);
    m4._data[2] = _mm_mul_ps(m4._data[2], r);
    return math::basis(m4);
}

template<arithmetic_type T>
inline matrix4<float> operator/(matrix4<float> lhs, T rhs)
{
    __m128 r = _mm_set_ss(1.0f / rhs);
    lhs._data[0] = _mm_mul_ps(lhs._data[0], r);
    lhs._data[1] = _mm_mul_ps(lhs._data[1], r);
    lhs._data[2] = _mm_mul_ps(lhs._data[2], r);
    lhs._data[3] = _mm_mul_ps(lhs._data[3], r);
    return lhs;
}

template<arithmetic_type T>
inline matrix3<float> operator/(const matrix3<float> &lhs, T rhs)
{
    __m128 r = _mm_set_ss(1.0f / rhs);
    matrix4<T> m4(lhs);
    m4._data[0] = _mm_mul_ps(m4._data[0], r);
    m4._data[1] = _mm_mul_ps(m4._data[1], r);
    m4._data[2] = _mm_mul_ps(m4._data[2], r);
    return math::basis(m4);
}

inline matrix4<float> operator+(matrix4<float> lhs, const matrix4<float> &rhs)
{
    lhs._data[0] = _mm_add_ps(lhs._data[0], rhs._data[0]);
    lhs._data[1] = _mm_add_ps(lhs._data[1], rhs._data[1]);
    lhs._data[2] = _mm_add_ps(lhs._data[2], rhs._data[2]);
    lhs._data[3] = _mm_add_ps(lhs._data[3], rhs._data[3]);
    return lhs;
}

inline matrix3<float> operator+(const matrix3<float> &lhs, const matrix3<float> &rhs)
{
    matrix4<float> m4(lhs), m4_rhs(rhs);
    m4._data[0] = _mm_add_ps(m4._data[0], m4_rhs._data[0]);
    m4._data[1] = _mm_add_ps(m4._data[1], m4_rhs._data[1]);
    m4._data[2] = _mm_add_ps(m4._data[2], m4_rhs._data[2]);
    return math::basis(m4);
}

inline matrix4<float> operator-(matrix4<float> lhs, const matrix4<float> &rhs)
{
    lhs._data[0] = _mm_sub_ps(lhs._data[0], rhs._data[0]);
    lhs._data[1] = _mm_sub_ps(lhs._data[1], rhs._data[1]);
    lhs._data[2] = _mm_sub_ps(lhs._data[2], rhs._data[2]);
    lhs._data[3] = _mm_sub_ps(lhs._data[3], rhs._data[3]);
    return lhs;
}

inline matrix3<float> operator-(const matrix3<float> &lhs, const matrix3<float> &rhs)
{
    matrix4<float> m4(lhs), m4_rhs(rhs);
    m4._data[0] = _mm_sub_ps(m4._data[0], m4_rhs._data[0]);
    m4._data[1] = _mm_sub_ps(m4._data[1], m4_rhs._data[1]);
    m4._data[2] = _mm_sub_ps(m4._data[2], m4_rhs._data[2]);
    return math::basis(m4);
}

#endif

using s8mat4 = matrix4<s8>;
using s16mat4 = matrix4<s16>;
using smat4 = matrix4<s32>;
using s64mat4 = matrix4<s64>;
using u8mat4 = matrix4<u8>;
using u16mat4 = matrix4<u16>;
using umat4 = matrix4<u32>;
using u64mat4 = matrix4<u64>;
using mat4 = matrix4<f32>;
using f64mat4 = matrix4<f64>;

} // namespace nslib
