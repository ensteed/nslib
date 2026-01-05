#pragma once

#include <cmath>
#include <algorithm>
#include "mtype_common.h"

namespace nslib::math
{
inline constexpr f32 PI = 3.14159265359f;
inline constexpr f32 FLOAT_EPS = 0.001f;
inline constexpr char PRINT_MAT_DELIMITER = '\n';
inline constexpr char PRINT_MAT_START = '\n';
inline constexpr char PRINT_MAT_END = '\n';
inline constexpr char PRINT_VEC_DELIMITER = ' ';
inline constexpr char PRINT_START_VEC = '[';
inline constexpr char PRINT_END_VEC = ']';
inline constexpr s8 ROUND_TO_DEC = 4;

inline constexpr f32 TO_DEGREES = (180.0f / PI);
inline constexpr f32 TO_RADS = (PI / 180.0f);

s8 count_digits(s32 number);

template<floating_pt T>
T sin(T val)
{
    return std::sin(val);
}

template<floating_pt T>
T cos(T val)
{
    return std::cos(val);
}

template<floating_pt T>
T tan(T val)
{
    return std::tan(val);
}

template<floating_pt T>
T asin(T val)
{
    return std::asin(val);
}

template<floating_pt T>
T acos(T val)
{
    return std::acos(val);
}

template<floating_pt T>
T atan(T val)
{
    return std::atan2(val);
}

template<floating_pt T>
inline T sqrt(T val)
{
    return std::sqrt(val);
}

template<floating_pt T>
inline T rsqrt(T val)
{
    return T(1) / std::sqrt(val);
}

#if NOBLE_STEED_SIMD & NOBLE_STEED_USE_SSE
#if NOBLE_STEED_SIMD & NOBLE_STEED_SSE_SQRT_BIT
inline f32 sqrt(f32 val)
{
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(val)));
}
#endif
inline f32 rsqrt(f32 val)
{
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(val)));
}
#endif

f32 random_float(f32 high_ = 1.0f, f32 low_ = 0.0f);

double round_decimal(double to_round, s8 decimal_places);

template<class T>
auto sum_elements(const T &veca)
{
    auto ret = decltype(veca.data[0]){0};
    for (auto &&item : veca)
        ret += item;
    return ret;
}

template<class U, class T>
auto convert_elements(const T &veca)
{
    using other_type = typename T::template container_type<U>;
    other_type ret;
    for (u8 i{0}; i < T::size_; ++i)
        ret[i] = (U)veca[i];
    return ret;
}

template<class T>
inline typename T::value_type dot(const T &veca, const T &vecb)
{
    auto ret = decltype(veca.data[0]){0};
    for (s8 i{0}; i < veca.size(); ++i)
        ret += veca[i] * vecb[i];
    return ret;
}

template<class T>
inline auto length_sq(const T &veca)
{
    return dot(veca, veca);
}

template<holds_floating_pt T>
inline auto length(const T &veca)
{
    return math::sqrt(dot(veca, veca));
}

template<holds_integral T>
inline auto length(const T &veca)
{
    return math::sqrt((f32)length_sq(veca));
}

template<vec_or_quat_type T>
inline void set_length(T *vec, const typename T::value_type &new_len)
{
    auto len = math::rsqrt(dot(*vec, *vec));
    *vec *= (new_len * len);
}

template<vec_or_quat_type T>
inline T set_length(T vec, const typename T::value_type &new_len)
{
    set_length(&vec, new_len);
    return vec;
}

template<vec_type T>
auto angle(const T &veca, const T &vecb)
{
    auto dot_p = dot(veca, vecb);
    auto l = math::sqrt(length_sq(veca) * length_sq(vecb));
    if (l < FLOAT_EPS)
        return l;
    return math::acos(dot_p / l);
}

template<vec_type T>
requires holds_integral<T>
auto angle(const T &veca, const T &vecb)
{
    auto dot_p = dot(veca, vecb);
    f32 l = math::sqrt((f32)(length_sq(veca) * length_sq(vecb)));
    if (l < FLOAT_EPS)
        return l;
    return math::acos(dot_p / l);
}

template<vec_type T>
requires holds_floating_pt<T>
void project(T *a, const T &b)
{
    using Type = typename T::value_type;
    Type denom = math::dot(b, b);
    if (fequals(denom, (Type)0.0, (Type)FLOAT_EPS))
        return;
    *a = (math::dot(*a, b) / denom) * b;
}

template<vec_type T>
void project(T *a, const T &b)
{
    using Type = typename T::value_type;
    Type denom = math::dot(b, b);
    if (denom == 0)
        return;
    *a = (math::dot(*a, b) / (f32)denom) * b;
}

template<vec_type T>
T project(T a, const T &b)
{
    project(&a, b);
    return a;
}

template<vec_type T>
void project_plane(T *vec, const T &normal)
{
    *vec -= project(*vec, normal);
}

template<vec_type T>
T project_plane(T vec, const T &normal)
{
    project_plane(&vec, normal);
    return vec;
}

template<vec_type T>
void reflect(T *vec, const T &normal)
{
    using Type = typename T::value_type;
    *vec -= (Type)2 * math::dot(*vec, normal) * normal;
}

template<vec_type T>
T reflect(T vec, const T &normal)
{
    reflect(&vec, normal);
    return vec;
}

template<vec_or_quat_type T>
void normalize(T *vec_or_quat)
{
    auto len = math::rsqrt(dot(*vec_or_quat, *vec_or_quat));
    *vec_or_quat *= len;
}

template<vec_or_quat_type T>
T normalize(T vec_or_quat)
{
    normalize(&vec_or_quat);
    return vec_or_quat;
}

template<class T>
auto min_element(const T &cont)
{
    return *std::min_element(cont.begin(), cont.end());
}

template<class T>
auto max_element(const T &cont)
{
    return *std::max_element(cont.begin(), cont.end());
}

template<class T>
T minimums(T lhs_, const T &rhs_)
{
    for (s8 i{0}; i < lhs_.size(); ++i)
    {
        if (rhs_[i] < lhs_[i])
            lhs_[i] = rhs_[i];
    }
    return lhs_;
}

template<class T>
T maximums(T lhs_, const T &rhs_)
{
    for (s8 i{0}; i < lhs_.size(); ++i)
    {
        if (rhs_[i] > lhs_[i])
            lhs_[i] = rhs_[i];
    }
    return lhs_;
}

template<signed_number T>
T abs(T item)
{
    return std::abs(item);
}

template<signed_number T>
void abs(T *item)
{
    *item = std::abs(*item);
}

template<holds_signed_number T>
void abs(T *item)
{
    for (auto &&element : *item)
        abs(&element);
}

template<holds_signed_number T>
T abs(T item)
{
    abs(&item);
    return item;
}

template<floating_pt T>
T ceil(T item)
{
    return std::ceil(item);
}

template<floating_pt T>
void ceil(T *item)
{
    *item = std::ceil(*item);
}

// This changes in place
template<holds_floating_pt T>
void ceil(T *item)
{
    for (auto &&element : *item)
        ceil(&element);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>

T ceil(T item)
{
    ceil(&item);
    return item;
}

template<floating_pt T>
T floor(T item)
{
    return std::floor(item);
}

template<floating_pt T>
void floor(T *item)
{
    *item = std::floor(*item);
}

// This changes in place
template<holds_floating_pt T>
void floor(T *item)
{
    for (auto &&element : *item)
        floor(&element);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>

T floor(T item)
{
    floor(&item);
    return item;
}

template<floating_pt T>
T round(T item)
{
    return std::round(item);
}

template<floating_pt T>
void round(T *item)
{
    *item = std::round(*item);
}

// This changes in place
template<holds_floating_pt T>
void round(T *item)
{
    for (auto &&element : *item)
        round(&element);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>
T round(T item)
{
    round(&item);
    return item;
}

template<floating_pt T>
T round(T item, s8 decimal_places)
{
    return round_decimal(item, decimal_places);
}

template<floating_pt T>
void round(T *item, s8 decimal_places)
{
    *item = round_decimal(*item, decimal_places);
}

// This changes in place
template<holds_floating_pt T>
void round(T *item, s8 decimal_places)
{
    for (auto &&element : *item)
        round(&element, decimal_places);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>
T round(T item, s8 decimal_places)
{
    round(&item, decimal_places);
    return item;
}

template<mat_type T, vec_type V>
void set_mat_column(T *mat, sizet ind, const V &col)
{
    static_assert(V::size_ <= T::size_);
    for (s8 i{0}; i < V::size_; ++i)
        (*mat)[i][ind] = col[i];
}

template<mat_type T>
void compwise_mult(T *lhs, const T &rhs)
{
    for (s8 i{0}; i < T::size_; ++i)
        (*lhs)[i] *= rhs[i];
}

template<mat_type T>
T compwise_mult(T lhs, const T &rhs)
{
    compwise_mult(&lhs, rhs);
    return lhs;
}

template<mat_type T>
void compwise_div(T *lhs, const T &rhs)
{
    for (s8 i{0}; i < T::size_; ++i)
        (*lhs)[i] /= rhs[i];
}

template<mat_type T>
T compwise_div(T lhs, const T &rhs)
{
    compwise_div(&lhs, rhs);
    return lhs;
}

template<mat_type T, vec_type V>
void compwise_mult_rows(T *lhs, const V &row_vec)
{
    static_assert(T::size_ == V::size_);
    for (s8 i{0}; i < T::size_; ++i)
        (*lhs)[i] *= row_vec;
}

template<mat_type T, vec_type V>
T compwise_mult_rows(T lhs, const V &row_vec)
{
    compwise_mult_rows(&lhs, row_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_mult_rows(const V &row_vec, const T &rhs)
{
    return compwise_mult_rows(rhs, row_vec);
}

template<vec_type V, mat_type T>
void compwise_mult_rows(const V &row_vec, T *rhs)
{
    compwise_mult_rows(rhs, row_vec);
}

template<mat_type T, vec_type V>
void compwise_div_rows(T *lhs, const V &row_vec)
{
    static_assert(T::size_ == V::size_);
    for (s8 i{0}; i < T::size_; ++i)
        (*lhs)[i] /= row_vec;
}

template<vec_type V, mat_type T>
void compwise_div_rows(const V &row_vec, T *rhs)
{
    static_assert(T::size_ == V::size_);
    for (s8 i{0}; i < T::size_; ++i)
        (*rhs)[i] = row_vec / (*rhs)[i];
}

template<mat_type T, vec_type V>
T compwise_div_rows(T lhs, const V &row_vec)
{
    compwise_div_rows(&lhs, row_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_div_rows(const V &row_vec, T rhs)
{
    compwise_div_rows(row_vec, &rhs);
    return rhs;
}

template<mat_type T, vec_type V>
void compwise_mult_columns(T *lhs, const V &column_vec)
{
    static_assert(T::size_ == V::size_);
    for (s8 rowi{0}; rowi < T::size_; ++rowi)
    {
        for (s8 coli{0}; coli < T::size_; ++coli)
            (*lhs)[rowi][coli] *= column_vec[rowi];
    }
}

template<mat_type T, vec_type V>
T compwise_mult_columns(T lhs, const V &column_vec)
{
    compwise_mult_columns(&lhs, column_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_mult_columns(const V &column_vec, const T &rhs)
{
    return compwise_mult_columns(column_vec, &rhs);
}

template<mat_type T, vec_type V>
void compwise_div_columns(T *lhs, const V &column_vec)
{
    static_assert(T::size_ == V::size_);
    for (s8 rowi{0}; rowi < T::size_; ++rowi)
    {
        for (s8 coli{0}; coli < T::size_; ++coli)
            (*lhs)[rowi][coli] /= column_vec[rowi];
    }
}

template<vec_type V, mat_type T>
void compwise_div_columns(const V &column_vec, T *rhs)
{
    static_assert(T::size_ == V::size_);
    for (s8 rowi{0}; rowi < T::size_; ++rowi)
    {
        for (s8 coli{0}; coli < T::size_; ++coli)
            (*rhs)[rowi][coli] = column_vec[rowi] / (*rhs)[rowi][coli];
    }
}

template<mat_type T, vec_type V>
T compwise_div_columns(T lhs, const V &column_vec)
{
    compwise_div_columns(&lhs, column_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_div_columns(const V &column_vec, T rhs)
{
    compwise_div_columns(column_vec, &rhs);
    return rhs;
}
} // namespace math
