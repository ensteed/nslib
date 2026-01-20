#pragma once
#include <type_traits>

namespace nslib
{
template<class T>
concept floating_pt = std::is_floating_point_v<T>;

template<class T>
concept integral = std::is_integral_v<T>;

template<class T>
concept signed_number = std::is_signed_v<T>;

template<class T>
concept unsigned_number = std::is_unsigned_v<T>;

template<class T>
concept signed_integral = integral<T> && signed_number<T>;

template<class T>
concept unsigned_integral = integral<T> && unsigned_number<T>;

template<class T>
concept arithmetic_type = std::is_arithmetic_v<T>;

} // namespace nslib
