#pragma once
#include "math/mtype_common.h"

namespace nslib
{

enum rformat
{
    // RGBA
    RFMT_RGBA8_SRGB,
    RFMT_RGBA8_SRGB_COMPRESSED,
    RFMT_RGBA8_UNORM,
    RFMT_RGBA8_UNORM_COMPRESSED,
    RFMT_RGBA8_SNORM,
    RFMT_RGBA8_UINT,
    RFMT_RGBA8_SINT,
    // BGRA
    RFMT_BGRA8_SRGB,
    RFMT_BGRA8_UNORM,
    RFMT_BGRA8_SNORM,
    RFMT_BGRA8_UINT,
    RFMT_BGRA8_SINT,
    // ABGR
    RFMT_ABGR8_SRGB,
    RFMT_ABGR8_UNORM,
    RFMT_ABGR8_SNORM,
    RFMT_ABGR8_UINT,
    RFMT_ABGR8_SINT,
    // RGB
    RFMT_RGB8_SRGB,
    RFMT_RGB8_SRGB_COMPRESSED,
    RFMT_RGB8_UNORM,
    RFMT_RGB8_UNORM_COMPRESSED,
    RFMT_RGB8_SNORM,
    RFMT_RGB8_UINT,
    RFMT_RGB8_SINT,
    // BGR
    RFMT_BGR8_SRGB,
    RFMT_BGR8_UNORM,
    RFMT_BGR8_SNORM,
    RFMT_BGR8_UINT,
    RFMT_BGR8_SINT,
    // RG
    RFMT_RG8_SRGB,
    RFMT_RG8_UNORM,
    RFMT_RG8_UNORM_COMPRESSED,
    RFMT_RG8_SNORM,
    RFMT_RG8_SNORM_COMPRESSED,
    RFMT_RG8_UINT,
    RFMT_RG8_SINT,
    // R
    RFMT_R8_SRGB,
    RFMT_R8_UNORM,
    RFMT_R8_UNORM_COMPRESSED,
    RFMT_R8_SNORM,
    RFMT_R8_SNORM_COMPRESSED,
    RFMT_R8_UINT,
    RFMT_R8_SINT,
    // RGBA 16 bpp
    RFMT_RGBA16_SFLOAT,
    RFMT_RGBA16_UNORM,
    RFMT_RGBA16_SNORM,
    RFMT_RGBA16_UINT,
    RFMT_RGBA16_SINT,
    // RGB
    RFMT_RGB16_SFLOAT,
    RFMT_RGB16_UNORM,
    RFMT_RGB16_SNORM,
    RFMT_RGB16_UINT,
    RFMT_RGB16_SINT,
    // RG
    RFMT_RG16_SFLOAT,
    RFMT_RG16_UNORM,
    RFMT_RG16_SNORM,
    RFMT_RG16_UINT,
    RFMT_RG16_SINT,
    // R
    RFMT_R16_SFLOAT,
    RFMT_R16_UNORM,
    RFMT_R16_SNORM,
    RFMT_R16_UINT,
    RFMT_R16_SINT,
    // RGBA 32 bpp
    RFMT_RGBA32_SFLOAT,
    RFMT_RGBA32_UINT,
    RFMT_RGBA32_SINT,
    // RGB
    RFMT_RGB32_SFLOAT,
    RFMT_RGB32_UINT,
    RFMT_RGB32_SINT,
    // RG
    RFMT_RG32_SFLOAT,
    RFMT_RG32_UINT,
    RFMT_RG32_SINT,
    // R
    RFMT_R32_SFLOAT,
    RFMT_R32_UINT,
    RFMT_R32_SINT,
    // RGBA 64 bpp
    RFMT_RGBA64_SFLOAT,
    RFMT_RGBA64_UINT,
    RFMT_RGBA64_SINT,
    // RGB
    RFMT_RGB64_SFLOAT,
    RFMT_RGB64_UINT,
    RFMT_RGB64_SINT,
    // RG
    RFMT_RG64_SFLOAT,
    RFMT_RG64_UINT,
    RFMT_RG64_SINT,
    // R
    RFMT_R64_SFLOAT,
    RFMT_R64_UINT,
    RFMT_R64_SINT,
    // Depth/Stencil formats
    RFMT_D16_UNORM,          // 2 bytes
    RFMT_D16_UNORM_S8_UINT,  // 3 bytes
    RFMT_D32_SFLOAT,         // 4 bytes
    RFMT_D24_UNORM_S8_UINT,  // 4 bytes
    RFMT_D32_SFLOAT_S8_UINT, // 8 bytes
                             // Whatever format the swapchain is
    RFMT_INVALID,
};

inline constexpr const char *RFORMAT_STR_NAMES[]{
    "RGBA8_SRGB",
    "RGBA8_SRGB_COMPRESSED",
    "RGBA8_UNORM",
    "RGBA8_UNORM_COMPRESSED",
    "RGBA8_SNORM",
    "RGBA8_UINT",
    "RGBA8_SINT",
    "BGRA8_SRGB",
    "BGRA8_UNORM",
    "BGRA8_SNORM",
    "BGRA8_UINT",
    "BGRA8_SINT",
    "ABGR8_SRGB",
    "ABGR8_UNORM",
    "ABGR8_SNORM",
    "ABGR8_UINT",
    "ABGR8_SINT",
    "RGB8_SRGB",
    "RGB8_SRGB_COMPRESSED",
    "RGB8_UNORM",
    "RGB8_UNORM_COMPRESSED",
    "RGB8_SNORM",
    "RGB8_UINT",
    "RGB8_SINT",
    "BGR8_SRGB",
    "BGR8_UNORM",
    "BGR8_SNORM",
    "BGR8_UINT",
    "BGR8_SINT",
    "RG8_SRGB",
    "RG8_UNORM",
    "RG8_UNORM_COMPRESSED",
    "RG8_SNORM",
    "RG8_SNORM_COMPRESSED",
    "RG8_UINT",
    "RG8_SINT",
    "R8_SRGB",
    "R8_UNORM",
    "R8_UNORM_COMPRESSED",
    "R8_SNORM",
    "R8_SNORM_COMPRESSED",
    "R8_UINT",
    "R8_SINT",
    "RGBA16_SFLOAT",
    "RGBA16_UNORM",
    "RGBA16_SNORM",
    "RGBA16_UINT",
    "RGBA16_SINT",
    "RGB16_SFLOAT",
    "RGB16_UNORM",
    "RGB16_SNORM",
    "RGB16_UINT",
    "RGB16_SINT",
    "RG16_SFLOAT",
    "RG16_UNORM",
    "RG16_SNORM",
    "RG16_UINT",
    "RG16_SINT",
    "R16_SFLOAT",
    "R16_UNORM",
    "R16_SNORM",
    "R16_UINT",
    "R16_SINT",
    "RGBA32_SFLOAT",
    "RGBA32_UINT",
    "RGBA32_SINT",
    "RGB32_SFLOAT",
    "RGB32_UINT",
    "RGB32_SINT",
    "RG32_SFLOAT",
    "RG32_UINT",
    "RG32_SINT",
    "R32_SFLOAT",
    "R32_UINT",
    "R32_SINT",
    "RGBA64_SFLOAT",
    "RGBA64_UINT",
    "RGBA64_SINT",
    "RGB64_SFLOAT",
    "RGB64_UINT",
    "RGB64_SINT",
    "RG64_SFLOAT",
    "RG64_UINT",
    "RG64_SINT",
    "R64_SFLOAT",
    "R64_UINT",
    "R64_SINT",
    "D16_UNORM",
    "D16_UNORM_S8_UINT",
    "D32_SFLOAT",
    "D24_UNORM_S8_UINT",
    "D32_SFLOAT_S8_UINT",
    "INVALID",
};

constexpr const char *get_rformat_str(rformat f)
{
    return RFORMAT_STR_NAMES[(u32)f];
}

struct rformat_info
{
    u8 block_width;
    u8 block_height;
    u8 bytes_per_block;
    u8 components;
};

u8 get_bytes_per_component(rformat format);
u8 get_component_count(rformat format);

sizet calculate_image_size(const rformat_info finfo, u32 width, u32 height, u32 mip_levels, u32 layer_count);
sizet calculate_image_buffer_size(const rformat_info finfo, u32 width, u32 height, u32 mip_levels, u32 layer_count);

rformat_info get_rformat_info(rformat format);
b32 is_floating_point_type(rformat format);

b32 has_stencil_component(rformat format);
b32 has_depth_component(rformat format);
b32 is_depth_only(rformat format);
b32 is_stencil_only(rformat format);
b32 is_depth_stencil(rformat format);

b32 is_uint_type(rformat format);
b32 is_sint_type(rformat format);

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

// --- Version A: For Floats and 32/64-bit Ints (No b32 needed) ---
template<FixedFormat T>
constexpr rformat get_rformat_for_type()
{
    using Info = type_info<T>;
    using V = typename Info::element_type;
    constexpr size_t N = Info::components;
    constexpr size_t S = sizeof(V);

    if constexpr (floating_pt<V>) {
        if constexpr (S == 8) {
            if constexpr (N == 4) return RFMT_RGBA64_SFLOAT;
            if constexpr (N == 3) return RFMT_RGB64_SFLOAT;
            if constexpr (N == 2) return RFMT_RG64_SFLOAT;
            return RFMT_R64_SFLOAT;
        }
        else {
            if constexpr (N == 4) return RFMT_RGBA32_SFLOAT;
            if constexpr (N == 3) return RFMT_RGB32_SFLOAT;
            if constexpr (N == 2) return RFMT_RG32_SFLOAT;
            return RFMT_R32_SFLOAT;
        }
    }
    else { // 32-bit or 64-bit Integers
        if constexpr (unsigned_integral<V>) {
            if constexpr (S == 8) {
                if constexpr (N == 4) return RFMT_RGBA64_UINT;
                if constexpr (N == 2) return RFMT_RG64_UINT;
                return RFMT_R64_UINT;
            }
            else {
                if constexpr (N == 4) return RFMT_RGBA32_UINT;
                if constexpr (N == 2) return RFMT_RG32_UINT;
                return RFMT_R32_UINT;
            }
        }
        else { // Signed
            if constexpr (S == 8) {
                if constexpr (N == 4) return RFMT_RGBA64_SINT;
                if constexpr (N == 2) return RFMT_RG64_SINT;
                return RFMT_R64_SINT;
            }
            else {
                if constexpr (N == 4) return RFMT_RGBA32_SINT;
                if constexpr (N == 2) return RFMT_RG32_SINT;
                return RFMT_R32_SINT;
            }
        }
    }
}

// --- Version B: For 8-bit and 16-bit Ints (MUST pass b32) ---
template<RequiresNormalization T>
constexpr rformat get_rformat_for_type(b32 normalize)
{
    using Info = type_info<T>;
    using V = typename Info::element_type;
    constexpr size_t N = Info::components;
    constexpr size_t S = sizeof(V);

    if constexpr (unsigned_integral<V>) {
        if constexpr (S == 2) { // 16-bit
            if (normalize) {
                if constexpr (N == 4) return RFMT_RGBA16_UNORM;
                if constexpr (N == 2) return RFMT_RG16_UNORM;
                return RFMT_R16_UNORM;
            }
            if constexpr (N == 4) return RFMT_RGBA16_UINT;
            if constexpr (N == 2) return RFMT_RG16_UINT;
            return RFMT_R16_UINT;
        }
        else { // 8-bit
            if (normalize) {
                if constexpr (N == 4) return RFMT_RGBA8_UNORM;
                if constexpr (N == 2) return RFMT_RG8_UNORM;
                return RFMT_R8_UNORM;
            }
            if constexpr (N == 4) return RFMT_RGBA8_UINT;
            if constexpr (N == 2) return RFMT_RG8_UINT;
            return RFMT_R8_UINT;
        }
    }
    else { // Signed 8/16-bit
        if constexpr (S == 2) {
            if (normalize) {
                if constexpr (N == 4) return RFMT_RGBA16_SNORM;
                if constexpr (N == 2) return RFMT_RG16_SNORM;
                return RFMT_R16_SNORM;
            }
            if constexpr (N == 4) return RFMT_RGBA16_SINT;
            if constexpr (N == 2) return RFMT_RG16_SINT;
            return RFMT_R16_SINT;
        }
        else {
            if (normalize) {
                if constexpr (N == 4) return RFMT_RGBA8_SNORM;
                if constexpr (N == 2) return RFMT_RG8_SNORM;
                return RFMT_R8_SNORM;
            }
            if constexpr (N == 4) return RFMT_RGBA8_SINT;
            if constexpr (N == 2) return RFMT_RG8_SINT;
            return RFMT_R8_SINT;
        }
    }
}
} // namespace nslib
