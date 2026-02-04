#pragma once
#include "math/mtype_common.h"

namespace nslib
{

enum struct rformat
{
    // RGBA
    RGBA8_SRGB,
    RGBA8_SRGB_COMPRESSED,
    RGBA8_UNORM,
    RGBA8_UNORM_COMPRESSED,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    // BGRA
    BGRA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SNORM,
    BGRA8_UINT,
    BGRA8_SINT,
    // ABGR
    ABGR8_SRGB,
    ABGR8_UNORM,
    ABGR8_SNORM,
    ABGR8_UINT,
    ABGR8_SINT,
    // RGB
    RGB8_SRGB,
    RGB8_SRGB_COMPRESSED,
    RGB8_UNORM,
    RGB8_UNORM_COMPRESSED,
    RGB8_SNORM,
    RGB8_UINT,
    RGB8_SINT,
    // BGR
    BGR8_SRGB,
    BGR8_UNORM,
    BGR8_SNORM,
    BGR8_UINT,
    BGR8_SINT,
    // RG
    RG8_SRGB,
    RG8_UNORM,
    RG8_UNORM_COMPRESSED,
    RG8_SNORM,
    RG8_SNORM_COMPRESSED,
    RG8_UINT,
    RG8_SINT,
    // R
    R8_SRGB,
    R8_UNORM,
    R8_UNORM_COMPRESSED,
    R8_SNORM,
    R8_SNORM_COMPRESSED,
    R8_UINT,
    R8_SINT,
    // RGBA 16 bpp
    RGBA16_SFLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RGBA16_UINT,
    RGBA16_SINT,
    // RGB
    RGB16_SFLOAT,
    RGB16_UNORM,
    RGB16_SNORM,
    RGB16_UINT,
    RGB16_SINT,
    // RG
    RG16_SFLOAT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_UINT,
    RG16_SINT,
    // R
    R16_SFLOAT,
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    // RGBA 32 bpp
    RGBA32_SFLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    // RGB
    RGB32_SFLOAT,
    RGB32_UINT,
    RGB32_SINT,
    // RG
    RG32_SFLOAT,
    RG32_UINT,
    RG32_SINT,
    // R
    R32_SFLOAT,
    R32_UINT,
    R32_SINT,
    // RGBA 64 bpp
    RGBA64_SFLOAT,
    RGBA64_UINT,
    RGBA64_SINT,
    // RGB
    RGB64_SFLOAT,
    RGB64_UINT,
    RGB64_SINT,
    // RG
    RG64_SFLOAT,
    RG64_UINT,
    RG64_SINT,
    // R
    R64_SFLOAT,
    R64_UINT,
    R64_SINT,
    // Depth/Stencil formats
    D16_UNORM, // 2 bytes
    D16_UNORM_S8_UINT, // 3 bytes
    D32_SFLOAT, // 4 bytes
    D24_UNORM_S8_UINT, // 4 bytes
    D32_SFLOAT_S8_UINT, // 8 bytes
    // Whatever format the swapchain is
    INVALID,
};

sizet get_bytes_per_component(rformat format);
bool is_floating_point_type(rformat format);
bool is_uint_type(rformat format);
bool is_sint_type(rformat format);

template<typename T>
struct type_info
{
    using element_type = T;
    static constexpr size_t components = 1;
};

// Specialization for your Vec/Quat types
template<vec_or_quat_type T>
struct type_info<T>
{
    using element_type = typename T::value_type;
    static constexpr size_t components = T::size_;
};

template<typename T>
concept RequiresNormalization = (sizeof(typename type_info<T>::element_type) <= 2) && !floating_pt<typename type_info<T>::element_type>;

template<typename T>
concept FixedFormat = !RequiresNormalization<T>;

// --- Version A: For Floats and 32/64-bit Ints (No bool needed) ---
template<FixedFormat T>
constexpr rformat get_rformat_for_type()
{
    using Info = type_info<T>;
    using V = typename Info::element_type;
    constexpr size_t N = Info::components;
    constexpr size_t S = sizeof(V);

    if constexpr (floating_pt<V>) {
        if constexpr (S == 8) {
            if constexpr (N == 4) return rformat::RGBA64_SFLOAT;
            if constexpr (N == 3) return rformat::RGB64_SFLOAT;
            if constexpr (N == 2) return rformat::RG64_SFLOAT;
            return rformat::R64_SFLOAT;
        }
        else {
            if constexpr (N == 4) return rformat::RGBA32_SFLOAT;
            if constexpr (N == 3) return rformat::RGB32_SFLOAT;
            if constexpr (N == 2) return rformat::RG32_SFLOAT;
            return rformat::R32_SFLOAT;
        }
    }
    else { // 32-bit or 64-bit Integers
        if constexpr (unsigned_integral<V>) {
            if constexpr (S == 8) {
                if constexpr (N == 4) return rformat::RGBA64_UINT;
                if constexpr (N == 2) return rformat::RG64_UINT;
                return rformat::R64_UINT;
            }
            else {
                if constexpr (N == 4) return rformat::RGBA32_UINT;
                if constexpr (N == 2) return rformat::RG32_UINT;
                return rformat::R32_UINT;
            }
        }
        else { // Signed
            if constexpr (S == 8) {
                if constexpr (N == 4) return rformat::RGBA64_SINT;
                if constexpr (N == 2) return rformat::RG64_SINT;
                return rformat::R64_SINT;
            }
            else {
                if constexpr (N == 4) return rformat::RGBA32_SINT;
                if constexpr (N == 2) return rformat::RG32_SINT;
                return rformat::R32_SINT;
            }
        }
    }
}

// --- Version B: For 8-bit and 16-bit Ints (MUST pass bool) ---
template<RequiresNormalization T>
constexpr rformat get_rformat_for_type(bool normalize)
{
    using Info = type_info<T>;
    using V = typename Info::element_type;
    constexpr size_t N = Info::components;
    constexpr size_t S = sizeof(V);

    if constexpr (unsigned_integral<V>) {
        if constexpr (S == 2) { // 16-bit
            if (normalize) {
                if constexpr (N == 4) return rformat::RGBA16_UNORM;
                if constexpr (N == 2) return rformat::RG16_UNORM;
                return rformat::R16_UNORM;
            }
            if constexpr (N == 4) return rformat::RGBA16_UINT;
            if constexpr (N == 2) return rformat::RG16_UINT;
            return rformat::R16_UINT;
        }
        else { // 8-bit
            if (normalize) {
                if constexpr (N == 4) return rformat::RGBA8_UNORM;
                if constexpr (N == 2) return rformat::RG8_UNORM;
                return rformat::R8_UNORM;
            }
            if constexpr (N == 4) return rformat::RGBA8_UINT;
            if constexpr (N == 2) return rformat::RG8_UINT;
            return rformat::R8_UINT;
        }
    }
    else { // Signed 8/16-bit
        if constexpr (S == 2) {
            if (normalize) {
                if constexpr (N == 4) return rformat::RGBA16_SNORM;
                if constexpr (N == 2) return rformat::RG16_SNORM;
                return rformat::R16_SNORM;
            }
            if constexpr (N == 4) return rformat::RGBA16_SINT;
            if constexpr (N == 2) return rformat::RG16_SINT;
            return rformat::R16_SINT;
        }
        else {
            if (normalize) {
                if constexpr (N == 4) return rformat::RGBA8_SNORM;
                if constexpr (N == 2) return rformat::RG8_SNORM;
                return rformat::R8_SNORM;
            }
            if constexpr (N == 4) return rformat::RGBA8_SINT;
            if constexpr (N == 2) return rformat::RG8_SINT;
            return rformat::R8_SINT;
        }
    }
}
} // namespace nslib
