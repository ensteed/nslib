#pragma once
#include "vkr_context.h"
#include "render_defs.h"
#include "rformat.h"

namespace nslib
{
struct rbp_resource_requirement;
struct mpass_clear_value;
struct rbp_pass;
struct rstencil_op_state;

struct vk_format_info
{
    u8 block_width;
    u8 block_height;
    u8 bytes_per_block;
    u8 components;
};

VkImageLayout get_vk_layout_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req, bool is_final);
VkAccessFlags get_vk_access_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &r);
VkPipelineStageFlags get_vk_stage_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req);
VkImageLayout get_baked_initial_vk_layout(const rbp_pass &pass, const rbp_resource_requirement &req);
VkAttachmentLoadOp get_requirement_vk_load_op(const rbp_resource_requirement &req);
VkAttachmentStoreOp get_requirement_vk_store_op(const rbp_resource_requirement &req);
VkPipelineStageFlags normalize_vk_stage_mask(VkPipelineStageFlags stage);
VkClearValue get_vk_clear_value(const mpass_clear_value &cv, rformat tex_format);

VkRect2D get_vk_rect_from_normalized(const rect &norm, const uvec2 &dims);
VkRect2D get_vk_rect(const svec2 &pos, const uvec2 &dims);
VkRect2D get_vk_rect(const srect &r);
VkRect2D get_vk_rect(const urect &r);

VkViewport get_vk_viewport(const rect &norm_vp, const vec2 &depth_min_max, const uvec2 &dims);
VkViewport get_vk_viewport(const rect &vp, const vec2 &depth_min_max);
void fill_vk_stencil_op_state(VkStencilOpState *to_fill, const rstencil_op_state &src);

vk_format_info get_vk_format_info(VkFormat format);
vk_format_info get_vk_format_info(rformat format);
vk_format_info get_vk_format_info(const rformat_info &finfo);

sizet calculate_vk_image_size(const vk_format_info &fmt_info, u32 width, u32 height, u32 mip_level, u32 layer_count);
sizet calculate_vk_image_buffer_size(const vk_format_info &fmt_info, u32 width, u32 height, u32 mip_levels, u32 layer_count);

constexpr VkDescriptorType get_vk_descriptor_type(rdescriptor_type dt)
{
    switch (dt) {
    case (RDESCRIPTOR_TYPE_SAMPLER):
    case (RDESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER):
    case (RDESCRIPTOR_TYPE_SAMPLED_IMAGE):
    case (RDESCRIPTOR_TYPE_STORAGE_IMAGE):
    case (RDESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER):
    case (RDESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER):
    case (RDESCRIPTOR_TYPE_UNIFORM_BUFFER):
    case (RDESCRIPTOR_TYPE_STORAGE_BUFFER):
    case (RDESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC):
    case (RDESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC):
    case (RDESCRIPTOR_TYPE_INPUT_ATTACHMENT):
        return (VkDescriptorType)dt;
    case (RDESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK):
        return VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
    case (RDESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR):
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    case (RDESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV):
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    default:
        asrt_break("Unhandled case");
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

constexpr VkLogicOp get_vk_logic_op(rlogic_op lop)
{
    return (VkLogicOp)lop;
}

constexpr VkBlendOp get_vk_blend_op(rblend_op lop)
{
    return (VkBlendOp)lop;
}

constexpr VkCompareOp get_vk_compare_op(rcompare_op cop)
{
    return (VkCompareOp)cop;
}

constexpr VkStencilOp get_vk_stencil_op(rstencil_op sop)
{
    return (VkStencilOp)sop;
}

constexpr VkBlendFactor get_vk_blend_factor(rblend_factor bf)
{
    return (VkBlendFactor)bf;
}

constexpr VkShaderStageFlagBits get_vk_shader_stage_flag_bit(rshader_stage_type type)
{
    return (VkShaderStageFlagBits)RSHADER_STAGE_FLAG(type);
}

constexpr VkPrimitiveTopology get_vk_prim_topoloty(rgeom_topology gt)
{
    return (VkPrimitiveTopology)gt;
}

constexpr VkPolygonMode get_vk_polygon_mode(rpolygon_mode pm)
{
    return (VkPolygonMode)pm;
}

constexpr VkFrontFace get_vk_front_face(rfront_face_winding ffw)
{
    return (VkFrontFace)ffw;
}

constexpr rformat get_rformat(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_SRGB:
        return rformat::RGBA8_SRGB;
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return rformat::RGBA8_SRGB_COMPRESSED;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return rformat::RGBA8_UNORM;
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return rformat::RGBA8_UNORM_COMPRESSED;
    case VK_FORMAT_R8G8B8A8_SNORM:
        return rformat::RGBA8_SNORM;
    case VK_FORMAT_R8G8B8A8_UINT:
        return rformat::RGBA8_UINT;
    case VK_FORMAT_R8G8B8A8_SINT:
        return rformat::RGBA8_SINT;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return rformat::BGRA8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return rformat::BGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SNORM:
        return rformat::BGRA8_SNORM;
    case VK_FORMAT_B8G8R8A8_UINT:
        return rformat::BGRA8_UINT;
    case VK_FORMAT_B8G8R8A8_SINT:
        return rformat::BGRA8_SINT;
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return rformat::ABGR8_SRGB;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        return rformat::ABGR8_UNORM;
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        return rformat::ABGR8_SNORM;
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        return rformat::ABGR8_UINT;
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        return rformat::ABGR8_SINT;
    case VK_FORMAT_R8G8B8_SRGB:
        return rformat::RGB8_SRGB;
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        return rformat::RGB8_SRGB_COMPRESSED;
    case VK_FORMAT_R8G8B8_UNORM:
        return rformat::RGB8_UNORM;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        return rformat::RGB8_UNORM_COMPRESSED;
    case VK_FORMAT_R8G8B8_SNORM:
        return rformat::RGB8_SNORM;
    case VK_FORMAT_R8G8B8_UINT:
        return rformat::RGB8_UINT;
    case VK_FORMAT_R8G8B8_SINT:
        return rformat::RGB8_SINT;
    case VK_FORMAT_B8G8R8_SRGB:
        return rformat::BGR8_SRGB;
    case VK_FORMAT_B8G8R8_UNORM:
        return rformat::BGR8_UNORM;
    case VK_FORMAT_B8G8R8_SNORM:
        return rformat::BGR8_SNORM;
    case VK_FORMAT_B8G8R8_UINT:
        return rformat::BGR8_UINT;
    case VK_FORMAT_B8G8R8_SINT:
        return rformat::BGR8_SINT;
    case VK_FORMAT_R8G8_SRGB:
        return rformat::RG8_SRGB;
    case VK_FORMAT_R8G8_UNORM:
        return rformat::RG8_UNORM;
    case VK_FORMAT_BC5_UNORM_BLOCK:
        return rformat::RG8_UNORM_COMPRESSED;
    case VK_FORMAT_R8G8_SNORM:
        return rformat::RG8_SNORM;
    case VK_FORMAT_BC5_SNORM_BLOCK:
        return rformat::RG8_SNORM_COMPRESSED;
    case VK_FORMAT_R8G8_UINT:
        return rformat::RG8_UINT;
    case VK_FORMAT_R8G8_SINT:
        return rformat::RG8_SINT;
    case VK_FORMAT_R8_SRGB:
        return rformat::R8_SRGB;
    case VK_FORMAT_R8_UNORM:
        return rformat::R8_UNORM;
    case VK_FORMAT_BC4_UNORM_BLOCK:
        return rformat::R8_UNORM_COMPRESSED;
    case VK_FORMAT_R8_SNORM:
        return rformat::R8_SNORM;
    case VK_FORMAT_BC4_SNORM_BLOCK:
        return rformat::R8_SNORM_COMPRESSED;
    case VK_FORMAT_R8_UINT:
        return rformat::R8_UINT;
    case VK_FORMAT_R8_SINT:
        return rformat::R8_SINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return rformat::RGBA16_SFLOAT;
    case VK_FORMAT_R16G16B16A16_UNORM:
        return rformat::RGBA16_UNORM;
    case VK_FORMAT_R16G16B16A16_SNORM:
        return rformat::RGBA16_SNORM;
    case VK_FORMAT_R16G16B16A16_UINT:
        return rformat::RGBA16_UINT;
    case VK_FORMAT_R16G16B16A16_SINT:
        return rformat::RGBA16_SINT;
    case VK_FORMAT_R16G16B16_SFLOAT:
        return rformat::RGB16_SFLOAT;
    case VK_FORMAT_R16G16B16_UNORM:
        return rformat::RGB16_UNORM;
    case VK_FORMAT_R16G16B16_SNORM:
        return rformat::RGB16_SNORM;
    case VK_FORMAT_R16G16B16_UINT:
        return rformat::RGB16_UINT;
    case VK_FORMAT_R16G16B16_SINT:
        return rformat::RGB16_SINT;
    case VK_FORMAT_R16G16_SFLOAT:
        return rformat::RG16_SFLOAT;
    case VK_FORMAT_R16G16_UNORM:
        return rformat::RG16_UNORM;
    case VK_FORMAT_R16G16_SNORM:
        return rformat::RG16_SNORM;
    case VK_FORMAT_R16G16_UINT:
        return rformat::RG16_UINT;
    case VK_FORMAT_R16G16_SINT:
        return rformat::RG16_SINT;
    case VK_FORMAT_R16_SFLOAT:
        return rformat::R16_SFLOAT;
    case VK_FORMAT_R16_UNORM:
        return rformat::R16_UNORM;
    case VK_FORMAT_R16_SNORM:
        return rformat::R16_SNORM;
    case VK_FORMAT_R16_UINT:
        return rformat::R16_UINT;
    case VK_FORMAT_R16_SINT:
        return rformat::R16_SINT;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return rformat::RGBA32_SFLOAT;
    case VK_FORMAT_R32G32B32A32_UINT:
        return rformat::RGBA32_UINT;
    case VK_FORMAT_R32G32B32A32_SINT:
        return rformat::RGBA32_SINT;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return rformat::RGB32_SFLOAT;
    case VK_FORMAT_R32G32B32_UINT:
        return rformat::RGB32_UINT;
    case VK_FORMAT_R32G32B32_SINT:
        return rformat::RGB32_SINT;
    case VK_FORMAT_R32G32_SFLOAT:
        return rformat::RG32_SFLOAT;
    case VK_FORMAT_R32G32_UINT:
        return rformat::RG32_UINT;
    case VK_FORMAT_R32G32_SINT:
        return rformat::RG32_SINT;
    case VK_FORMAT_R32_SFLOAT:
        return rformat::R32_SFLOAT;
    case VK_FORMAT_R32_UINT:
        return rformat::R32_UINT;
    case VK_FORMAT_R32_SINT:
        return rformat::R32_SINT;
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        return rformat::RGBA64_SFLOAT;
    case VK_FORMAT_R64G64B64A64_UINT:
        return rformat::RGBA64_UINT;
    case VK_FORMAT_R64G64B64A64_SINT:
        return rformat::RGBA64_SINT;
    case VK_FORMAT_R64G64B64_SFLOAT:
        return rformat::RGB64_SFLOAT;
    case VK_FORMAT_R64G64B64_UINT:
        return rformat::RGB64_UINT;
    case VK_FORMAT_R64G64B64_SINT:
        return rformat::RGB64_SINT;
    case VK_FORMAT_R64G64_SFLOAT:
        return rformat::RG64_SFLOAT;
    case VK_FORMAT_R64G64_UINT:
        return rformat::RG64_UINT;
    case VK_FORMAT_R64G64_SINT:
        return rformat::RG64_SINT;
    case VK_FORMAT_R64_SFLOAT:
        return rformat::R64_SFLOAT;
    case VK_FORMAT_R64_UINT:
        return rformat::R64_UINT;
    case VK_FORMAT_R64_SINT:
        return rformat::R64_SINT;
    case VK_FORMAT_D16_UNORM:
        return rformat::D16_UNORM;
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return rformat::D16_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT:
        return rformat::D32_SFLOAT;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return rformat::D24_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return rformat::D32_SFLOAT_S8_UINT;
    default:
        return rformat::INVALID;
    }
}

constexpr const char *get_vk_format_str(VkFormat fmt)
{
    return get_rformat_str(get_rformat(fmt));
}

constexpr VkFormat get_vk_format(rformat format)
{
    switch (format) {
    case (rformat::RGBA8_SRGB):
        return VK_FORMAT_R8G8B8A8_SRGB;
    case (rformat::RGBA8_SRGB_COMPRESSED):
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case (rformat::RGBA8_UNORM):
        return VK_FORMAT_R8G8B8A8_UNORM;
    case (rformat::RGBA8_UNORM_COMPRESSED):
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case (rformat::RGBA8_SNORM):
        return VK_FORMAT_R8G8B8A8_SNORM;
    case (rformat::RGBA8_UINT):
        return VK_FORMAT_R8G8B8A8_UINT;
    case (rformat::RGBA8_SINT):
        return VK_FORMAT_R8G8B8A8_SINT;
    case (rformat::BGRA8_SRGB):
        return VK_FORMAT_B8G8R8A8_SRGB;
    case (rformat::BGRA8_UNORM):
        return VK_FORMAT_B8G8R8A8_UNORM;
    case (rformat::BGRA8_SNORM):
        return VK_FORMAT_B8G8R8A8_SNORM;
    case (rformat::BGRA8_UINT):
        return VK_FORMAT_B8G8R8A8_UINT;
    case (rformat::BGRA8_SINT):
        return VK_FORMAT_B8G8R8A8_SINT;
    case (rformat::ABGR8_SRGB):
        return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    case (rformat::ABGR8_UNORM):
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case (rformat::ABGR8_SNORM):
        return VK_FORMAT_A8B8G8R8_SNORM_PACK32;
    case (rformat::ABGR8_UINT):
        return VK_FORMAT_A8B8G8R8_UINT_PACK32;
    case (rformat::ABGR8_SINT):
        return VK_FORMAT_A8B8G8R8_SINT_PACK32;
    case (rformat::RGB8_SRGB):
        return VK_FORMAT_R8G8B8_SRGB;
    case (rformat::RGB8_SRGB_COMPRESSED):
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    case (rformat::RGB8_UNORM):
        return VK_FORMAT_R8G8B8_UNORM;
    case (rformat::RGB8_UNORM_COMPRESSED):
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case (rformat::RGB8_SNORM):
        return VK_FORMAT_R8G8B8_SNORM;
    case (rformat::RGB8_UINT):
        return VK_FORMAT_R8G8B8_UINT;
    case (rformat::RGB8_SINT):
        return VK_FORMAT_R8G8B8_SINT;
    case (rformat::BGR8_SRGB):
        return VK_FORMAT_B8G8R8_SRGB;
    case (rformat::BGR8_UNORM):
        return VK_FORMAT_B8G8R8_UNORM;
    case (rformat::BGR8_SNORM):
        return VK_FORMAT_B8G8R8_SNORM;
    case (rformat::BGR8_UINT):
        return VK_FORMAT_B8G8R8_UINT;
    case (rformat::BGR8_SINT):
        return VK_FORMAT_B8G8R8_SINT;
    case (rformat::RG8_SRGB):
        return VK_FORMAT_R8G8_SRGB;
    case (rformat::RG8_UNORM):
        return VK_FORMAT_R8G8_UNORM;
    case (rformat::RG8_UNORM_COMPRESSED):
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case (rformat::RG8_SNORM):
        return VK_FORMAT_R8G8_SNORM;
    case (rformat::RG8_SNORM_COMPRESSED):
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case (rformat::RG8_UINT):
        return VK_FORMAT_R8G8_UINT;
    case (rformat::RG8_SINT):
        return VK_FORMAT_R8G8_SINT;
    case (rformat::R8_SRGB):
        return VK_FORMAT_R8_SRGB;
    case (rformat::R8_UNORM):
        return VK_FORMAT_R8_UNORM;
    case (rformat::R8_UNORM_COMPRESSED):
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case (rformat::R8_SNORM):
        return VK_FORMAT_R8_SNORM;
    case (rformat::R8_SNORM_COMPRESSED):
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case (rformat::R8_UINT):
        return VK_FORMAT_R8_UINT;
    case (rformat::R8_SINT):
        return VK_FORMAT_R8_SINT;
    case (rformat::RGBA16_SFLOAT):
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case (rformat::RGBA16_UNORM):
        return VK_FORMAT_R16G16B16A16_UNORM;
    case (rformat::RGBA16_SNORM):
        return VK_FORMAT_R16G16B16A16_SNORM;
    case (rformat::RGBA16_UINT):
        return VK_FORMAT_R16G16B16A16_UINT;
    case (rformat::RGBA16_SINT):
        return VK_FORMAT_R16G16B16A16_SINT;
    case (rformat::RGB16_SFLOAT):
        return VK_FORMAT_R16G16B16_SFLOAT;
    case (rformat::RGB16_UNORM):
        return VK_FORMAT_R16G16B16_UNORM;
    case (rformat::RGB16_SNORM):
        return VK_FORMAT_R16G16B16_SNORM;
    case (rformat::RGB16_UINT):
        return VK_FORMAT_R16G16B16_UINT;
    case (rformat::RGB16_SINT):
        return VK_FORMAT_R16G16B16_SINT;
    case (rformat::RG16_SFLOAT):
        return VK_FORMAT_R16G16_SFLOAT;
    case (rformat::RG16_UNORM):
        return VK_FORMAT_R16G16_UNORM;
    case (rformat::RG16_SNORM):
        return VK_FORMAT_R16G16_SNORM;
    case (rformat::RG16_UINT):
        return VK_FORMAT_R16G16_UINT;
    case (rformat::RG16_SINT):
        return VK_FORMAT_R16G16_SINT;
    case (rformat::R16_SFLOAT):
        return VK_FORMAT_R16_SFLOAT;
    case (rformat::R16_UNORM):
        return VK_FORMAT_R16_UNORM;
    case (rformat::R16_SNORM):
        return VK_FORMAT_R16_SNORM;
    case (rformat::R16_UINT):
        return VK_FORMAT_R16_UINT;
    case (rformat::R16_SINT):
        return VK_FORMAT_R16_SINT;
    case (rformat::RGBA32_SFLOAT):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case (rformat::RGBA32_UINT):
        return VK_FORMAT_R32G32B32A32_UINT;
    case (rformat::RGBA32_SINT):
        return VK_FORMAT_R32G32B32A32_SINT;
    case (rformat::RGB32_SFLOAT):
        return VK_FORMAT_R32G32B32_SFLOAT;
    case (rformat::RGB32_UINT):
        return VK_FORMAT_R32G32B32_UINT;
    case (rformat::RGB32_SINT):
        return VK_FORMAT_R32G32B32_SINT;
    case (rformat::RG32_SFLOAT):
        return VK_FORMAT_R32G32_SFLOAT;
    case (rformat::RG32_UINT):
        return VK_FORMAT_R32G32_UINT;
    case (rformat::RG32_SINT):
        return VK_FORMAT_R32G32_SINT;
    case (rformat::R32_SFLOAT):
        return VK_FORMAT_R32_SFLOAT;
    case (rformat::R32_UINT):
        return VK_FORMAT_R32_UINT;
    case (rformat::R32_SINT):
        return VK_FORMAT_R32_SINT;
    case (rformat::RGBA64_SFLOAT):
        return VK_FORMAT_R64G64B64A64_SFLOAT;
    case (rformat::RGBA64_UINT):
        return VK_FORMAT_R64G64B64A64_UINT;
    case (rformat::RGBA64_SINT):
        return VK_FORMAT_R64G64B64A64_SINT;
    case (rformat::RGB64_SFLOAT):
        return VK_FORMAT_R64G64B64_SFLOAT;
    case (rformat::RGB64_UINT):
        return VK_FORMAT_R64G64B64_UINT;
    case (rformat::RGB64_SINT):
        return VK_FORMAT_R64G64B64_SINT;
    case (rformat::RG64_SFLOAT):
        return VK_FORMAT_R64G64_SFLOAT;
    case (rformat::RG64_UINT):
        return VK_FORMAT_R64G64_UINT;
    case (rformat::RG64_SINT):
        return VK_FORMAT_R64G64_SINT;
    case (rformat::R64_SFLOAT):
        return VK_FORMAT_R64_SFLOAT;
    case (rformat::R64_UINT):
        return VK_FORMAT_R64_UINT;
    case (rformat::R64_SINT):
        return VK_FORMAT_R64_SINT;
    case (rformat::D16_UNORM):
        return VK_FORMAT_D16_UNORM;
    case (rformat::D16_UNORM_S8_UINT):
        return VK_FORMAT_D16_UNORM_S8_UINT;
    case (rformat::D32_SFLOAT):
        return VK_FORMAT_D32_SFLOAT;
    case (rformat::D24_UNORM_S8_UINT):
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case (rformat::D32_SFLOAT_S8_UINT):
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case (rformat::INVALID):
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

constexpr bool is_floating_point_type(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return true;
    default:
        return false;
    }
}

constexpr bool is_uint_type(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64_UINT:
        return true;
    default:
        return false;
    }
}

constexpr bool is_sint_type(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64_SINT:
        return true;
    default:
        return false;
    }
}

constexpr bool has_depth_component(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

constexpr bool has_stencil_component(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

constexpr bool is_depth_only(VkFormat format)
{
    return has_depth_component(format) && !has_stencil_component(format);
}

constexpr bool is_stencil_only(VkFormat format)
{
    return has_stencil_component(format) && !has_depth_component(format);
}

constexpr bool is_depth_stencil(VkFormat format)
{
    return has_depth_component(format) || has_stencil_component(format);
}

constexpr VkImageAspectFlags get_vk_aspect_flags(VkFormat format)
{
    if (is_depth_stencil(format)) {
        return (has_depth_component(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
               (has_stencil_component(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

constexpr u8 get_bytes_per_component(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
        return 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_D32_SFLOAT:
        return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_D16_UNORM:
        return 2;
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
        return 1;
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_UNDEFINED:
    default:
        return (u8)~0;
    }
}

constexpr u8 get_component_count(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return 4;

    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        return 3;

    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
        return 2;

    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
        return 1;
    case VK_FORMAT_UNDEFINED:
    default:
        return (u8)~0;
    }
}

} // namespace nslib
