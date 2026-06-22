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

constexpr VkShaderStageFlags get_vk_shader_stage_flags(rshader_stage_flags flags)
{
    VkShaderStageFlags ret{};
    ret |= (test_flags(flags, RSHADER_STAGE_VERTEX_BIT) ? VK_SHADER_STAGE_VERTEX_BIT : 0);
    ret |= (test_flags(flags, RSHADER_STAGE_FRAGMENT_BIT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0);
    ret |= (test_flags(flags, RSHADER_STAGE_COMPUTE_BIT) ? VK_SHADER_STAGE_COMPUTE_BIT : 0);
    return ret;
}

constexpr VkShaderStageFlagBits get_vk_shader_stage_flag_bit(rshader_stage_type type)
{
    switch (type) {
    case RSHADER_STAGE_TYPE_VERTEX:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case RSHADER_STAGE_TYPE_FRAGMENT:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case RSHADER_STAGE_TYPE_COMPUTE:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }
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
        return RFMT_RGBA8_SRGB;
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return RFMT_RGBA8_SRGB_COMPRESSED;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return RFMT_RGBA8_UNORM;
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return RFMT_RGBA8_UNORM_COMPRESSED;
    case VK_FORMAT_R8G8B8A8_SNORM:
        return RFMT_RGBA8_SNORM;
    case VK_FORMAT_R8G8B8A8_UINT:
        return RFMT_RGBA8_UINT;
    case VK_FORMAT_R8G8B8A8_SINT:
        return RFMT_RGBA8_SINT;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return RFMT_BGRA8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return RFMT_BGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SNORM:
        return RFMT_BGRA8_SNORM;
    case VK_FORMAT_B8G8R8A8_UINT:
        return RFMT_BGRA8_UINT;
    case VK_FORMAT_B8G8R8A8_SINT:
        return RFMT_BGRA8_SINT;
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return RFMT_ABGR8_SRGB;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        return RFMT_ABGR8_UNORM;
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        return RFMT_ABGR8_SNORM;
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        return RFMT_ABGR8_UINT;
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        return RFMT_ABGR8_SINT;
    case VK_FORMAT_R8G8B8_SRGB:
        return RFMT_RGB8_SRGB;
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        return RFMT_RGB8_SRGB_COMPRESSED;
    case VK_FORMAT_R8G8B8_UNORM:
        return RFMT_RGB8_UNORM;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        return RFMT_RGB8_UNORM_COMPRESSED;
    case VK_FORMAT_R8G8B8_SNORM:
        return RFMT_RGB8_SNORM;
    case VK_FORMAT_R8G8B8_UINT:
        return RFMT_RGB8_UINT;
    case VK_FORMAT_R8G8B8_SINT:
        return RFMT_RGB8_SINT;
    case VK_FORMAT_B8G8R8_SRGB:
        return RFMT_BGR8_SRGB;
    case VK_FORMAT_B8G8R8_UNORM:
        return RFMT_BGR8_UNORM;
    case VK_FORMAT_B8G8R8_SNORM:
        return RFMT_BGR8_SNORM;
    case VK_FORMAT_B8G8R8_UINT:
        return RFMT_BGR8_UINT;
    case VK_FORMAT_B8G8R8_SINT:
        return RFMT_BGR8_SINT;
    case VK_FORMAT_R8G8_SRGB:
        return RFMT_RG8_SRGB;
    case VK_FORMAT_R8G8_UNORM:
        return RFMT_RG8_UNORM;
    case VK_FORMAT_BC5_UNORM_BLOCK:
        return RFMT_RG8_UNORM_COMPRESSED;
    case VK_FORMAT_R8G8_SNORM:
        return RFMT_RG8_SNORM;
    case VK_FORMAT_BC5_SNORM_BLOCK:
        return RFMT_RG8_SNORM_COMPRESSED;
    case VK_FORMAT_R8G8_UINT:
        return RFMT_RG8_UINT;
    case VK_FORMAT_R8G8_SINT:
        return RFMT_RG8_SINT;
    case VK_FORMAT_R8_SRGB:
        return RFMT_R8_SRGB;
    case VK_FORMAT_R8_UNORM:
        return RFMT_R8_UNORM;
    case VK_FORMAT_BC4_UNORM_BLOCK:
        return RFMT_R8_UNORM_COMPRESSED;
    case VK_FORMAT_R8_SNORM:
        return RFMT_R8_SNORM;
    case VK_FORMAT_BC4_SNORM_BLOCK:
        return RFMT_R8_SNORM_COMPRESSED;
    case VK_FORMAT_R8_UINT:
        return RFMT_R8_UINT;
    case VK_FORMAT_R8_SINT:
        return RFMT_R8_SINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return RFMT_RGBA16_SFLOAT;
    case VK_FORMAT_R16G16B16A16_UNORM:
        return RFMT_RGBA16_UNORM;
    case VK_FORMAT_R16G16B16A16_SNORM:
        return RFMT_RGBA16_SNORM;
    case VK_FORMAT_R16G16B16A16_UINT:
        return RFMT_RGBA16_UINT;
    case VK_FORMAT_R16G16B16A16_SINT:
        return RFMT_RGBA16_SINT;
    case VK_FORMAT_R16G16B16_SFLOAT:
        return RFMT_RGB16_SFLOAT;
    case VK_FORMAT_R16G16B16_UNORM:
        return RFMT_RGB16_UNORM;
    case VK_FORMAT_R16G16B16_SNORM:
        return RFMT_RGB16_SNORM;
    case VK_FORMAT_R16G16B16_UINT:
        return RFMT_RGB16_UINT;
    case VK_FORMAT_R16G16B16_SINT:
        return RFMT_RGB16_SINT;
    case VK_FORMAT_R16G16_SFLOAT:
        return RFMT_RG16_SFLOAT;
    case VK_FORMAT_R16G16_UNORM:
        return RFMT_RG16_UNORM;
    case VK_FORMAT_R16G16_SNORM:
        return RFMT_RG16_SNORM;
    case VK_FORMAT_R16G16_UINT:
        return RFMT_RG16_UINT;
    case VK_FORMAT_R16G16_SINT:
        return RFMT_RG16_SINT;
    case VK_FORMAT_R16_SFLOAT:
        return RFMT_R16_SFLOAT;
    case VK_FORMAT_R16_UNORM:
        return RFMT_R16_UNORM;
    case VK_FORMAT_R16_SNORM:
        return RFMT_R16_SNORM;
    case VK_FORMAT_R16_UINT:
        return RFMT_R16_UINT;
    case VK_FORMAT_R16_SINT:
        return RFMT_R16_SINT;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return RFMT_RGBA32_SFLOAT;
    case VK_FORMAT_R32G32B32A32_UINT:
        return RFMT_RGBA32_UINT;
    case VK_FORMAT_R32G32B32A32_SINT:
        return RFMT_RGBA32_SINT;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return RFMT_RGB32_SFLOAT;
    case VK_FORMAT_R32G32B32_UINT:
        return RFMT_RGB32_UINT;
    case VK_FORMAT_R32G32B32_SINT:
        return RFMT_RGB32_SINT;
    case VK_FORMAT_R32G32_SFLOAT:
        return RFMT_RG32_SFLOAT;
    case VK_FORMAT_R32G32_UINT:
        return RFMT_RG32_UINT;
    case VK_FORMAT_R32G32_SINT:
        return RFMT_RG32_SINT;
    case VK_FORMAT_R32_SFLOAT:
        return RFMT_R32_SFLOAT;
    case VK_FORMAT_R32_UINT:
        return RFMT_R32_UINT;
    case VK_FORMAT_R32_SINT:
        return RFMT_R32_SINT;
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        return RFMT_RGBA64_SFLOAT;
    case VK_FORMAT_R64G64B64A64_UINT:
        return RFMT_RGBA64_UINT;
    case VK_FORMAT_R64G64B64A64_SINT:
        return RFMT_RGBA64_SINT;
    case VK_FORMAT_R64G64B64_SFLOAT:
        return RFMT_RGB64_SFLOAT;
    case VK_FORMAT_R64G64B64_UINT:
        return RFMT_RGB64_UINT;
    case VK_FORMAT_R64G64B64_SINT:
        return RFMT_RGB64_SINT;
    case VK_FORMAT_R64G64_SFLOAT:
        return RFMT_RG64_SFLOAT;
    case VK_FORMAT_R64G64_UINT:
        return RFMT_RG64_UINT;
    case VK_FORMAT_R64G64_SINT:
        return RFMT_RG64_SINT;
    case VK_FORMAT_R64_SFLOAT:
        return RFMT_R64_SFLOAT;
    case VK_FORMAT_R64_UINT:
        return RFMT_R64_UINT;
    case VK_FORMAT_R64_SINT:
        return RFMT_R64_SINT;
    case VK_FORMAT_D16_UNORM:
        return RFMT_D16_UNORM;
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return RFMT_D16_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT:
        return RFMT_D32_SFLOAT;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return RFMT_D24_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return RFMT_D32_SFLOAT_S8_UINT;
    default:
        return RFMT_INVALID;
    }
}

constexpr const char *get_vk_format_str(VkFormat fmt)
{
    return get_rformat_str(get_rformat(fmt));
}

constexpr VkCullModeFlags get_vk_cullmode(rtechnique_flags flags)
{
    VkCullModeFlags ret{};
    if (test_flags(flags, RTECHNIQUE_FLAG_CULL_BACK)) {
        ret |= VK_CULL_MODE_BACK_BIT;
    }
    if (test_flags(flags, RTECHNIQUE_FLAG_CULL_FRONT)) {
        ret |= VK_CULL_MODE_FRONT_BIT;
    }
    return ret;
}

constexpr VkFormat get_vk_format(rformat format)
{
    switch (format) {
    case (RFMT_RGBA8_SRGB):
        return VK_FORMAT_R8G8B8A8_SRGB;
    case (RFMT_RGBA8_SRGB_COMPRESSED):
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case (RFMT_RGBA8_UNORM):
        return VK_FORMAT_R8G8B8A8_UNORM;
    case (RFMT_RGBA8_UNORM_COMPRESSED):
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case (RFMT_RGBA8_SNORM):
        return VK_FORMAT_R8G8B8A8_SNORM;
    case (RFMT_RGBA8_UINT):
        return VK_FORMAT_R8G8B8A8_UINT;
    case (RFMT_RGBA8_SINT):
        return VK_FORMAT_R8G8B8A8_SINT;
    case (RFMT_BGRA8_SRGB):
        return VK_FORMAT_B8G8R8A8_SRGB;
    case (RFMT_BGRA8_UNORM):
        return VK_FORMAT_B8G8R8A8_UNORM;
    case (RFMT_BGRA8_SNORM):
        return VK_FORMAT_B8G8R8A8_SNORM;
    case (RFMT_BGRA8_UINT):
        return VK_FORMAT_B8G8R8A8_UINT;
    case (RFMT_BGRA8_SINT):
        return VK_FORMAT_B8G8R8A8_SINT;
    case (RFMT_ABGR8_SRGB):
        return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    case (RFMT_ABGR8_UNORM):
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case (RFMT_ABGR8_SNORM):
        return VK_FORMAT_A8B8G8R8_SNORM_PACK32;
    case (RFMT_ABGR8_UINT):
        return VK_FORMAT_A8B8G8R8_UINT_PACK32;
    case (RFMT_ABGR8_SINT):
        return VK_FORMAT_A8B8G8R8_SINT_PACK32;
    case (RFMT_RGB8_SRGB):
        return VK_FORMAT_R8G8B8_SRGB;
    case (RFMT_RGB8_SRGB_COMPRESSED):
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    case (RFMT_RGB8_UNORM):
        return VK_FORMAT_R8G8B8_UNORM;
    case (RFMT_RGB8_UNORM_COMPRESSED):
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case (RFMT_RGB8_SNORM):
        return VK_FORMAT_R8G8B8_SNORM;
    case (RFMT_RGB8_UINT):
        return VK_FORMAT_R8G8B8_UINT;
    case (RFMT_RGB8_SINT):
        return VK_FORMAT_R8G8B8_SINT;
    case (RFMT_BGR8_SRGB):
        return VK_FORMAT_B8G8R8_SRGB;
    case (RFMT_BGR8_UNORM):
        return VK_FORMAT_B8G8R8_UNORM;
    case (RFMT_BGR8_SNORM):
        return VK_FORMAT_B8G8R8_SNORM;
    case (RFMT_BGR8_UINT):
        return VK_FORMAT_B8G8R8_UINT;
    case (RFMT_BGR8_SINT):
        return VK_FORMAT_B8G8R8_SINT;
    case (RFMT_RG8_SRGB):
        return VK_FORMAT_R8G8_SRGB;
    case (RFMT_RG8_UNORM):
        return VK_FORMAT_R8G8_UNORM;
    case (RFMT_RG8_UNORM_COMPRESSED):
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case (RFMT_RG8_SNORM):
        return VK_FORMAT_R8G8_SNORM;
    case (RFMT_RG8_SNORM_COMPRESSED):
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case (RFMT_RG8_UINT):
        return VK_FORMAT_R8G8_UINT;
    case (RFMT_RG8_SINT):
        return VK_FORMAT_R8G8_SINT;
    case (RFMT_R8_SRGB):
        return VK_FORMAT_R8_SRGB;
    case (RFMT_R8_UNORM):
        return VK_FORMAT_R8_UNORM;
    case (RFMT_R8_UNORM_COMPRESSED):
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case (RFMT_R8_SNORM):
        return VK_FORMAT_R8_SNORM;
    case (RFMT_R8_SNORM_COMPRESSED):
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case (RFMT_R8_UINT):
        return VK_FORMAT_R8_UINT;
    case (RFMT_R8_SINT):
        return VK_FORMAT_R8_SINT;
    case (RFMT_RGBA16_SFLOAT):
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case (RFMT_RGBA16_UNORM):
        return VK_FORMAT_R16G16B16A16_UNORM;
    case (RFMT_RGBA16_SNORM):
        return VK_FORMAT_R16G16B16A16_SNORM;
    case (RFMT_RGBA16_UINT):
        return VK_FORMAT_R16G16B16A16_UINT;
    case (RFMT_RGBA16_SINT):
        return VK_FORMAT_R16G16B16A16_SINT;
    case (RFMT_RGB16_SFLOAT):
        return VK_FORMAT_R16G16B16_SFLOAT;
    case (RFMT_RGB16_UNORM):
        return VK_FORMAT_R16G16B16_UNORM;
    case (RFMT_RGB16_SNORM):
        return VK_FORMAT_R16G16B16_SNORM;
    case (RFMT_RGB16_UINT):
        return VK_FORMAT_R16G16B16_UINT;
    case (RFMT_RGB16_SINT):
        return VK_FORMAT_R16G16B16_SINT;
    case (RFMT_RG16_SFLOAT):
        return VK_FORMAT_R16G16_SFLOAT;
    case (RFMT_RG16_UNORM):
        return VK_FORMAT_R16G16_UNORM;
    case (RFMT_RG16_SNORM):
        return VK_FORMAT_R16G16_SNORM;
    case (RFMT_RG16_UINT):
        return VK_FORMAT_R16G16_UINT;
    case (RFMT_RG16_SINT):
        return VK_FORMAT_R16G16_SINT;
    case (RFMT_R16_SFLOAT):
        return VK_FORMAT_R16_SFLOAT;
    case (RFMT_R16_UNORM):
        return VK_FORMAT_R16_UNORM;
    case (RFMT_R16_SNORM):
        return VK_FORMAT_R16_SNORM;
    case (RFMT_R16_UINT):
        return VK_FORMAT_R16_UINT;
    case (RFMT_R16_SINT):
        return VK_FORMAT_R16_SINT;
    case (RFMT_RGBA32_SFLOAT):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case (RFMT_RGBA32_UINT):
        return VK_FORMAT_R32G32B32A32_UINT;
    case (RFMT_RGBA32_SINT):
        return VK_FORMAT_R32G32B32A32_SINT;
    case (RFMT_RGB32_SFLOAT):
        return VK_FORMAT_R32G32B32_SFLOAT;
    case (RFMT_RGB32_UINT):
        return VK_FORMAT_R32G32B32_UINT;
    case (RFMT_RGB32_SINT):
        return VK_FORMAT_R32G32B32_SINT;
    case (RFMT_RG32_SFLOAT):
        return VK_FORMAT_R32G32_SFLOAT;
    case (RFMT_RG32_UINT):
        return VK_FORMAT_R32G32_UINT;
    case (RFMT_RG32_SINT):
        return VK_FORMAT_R32G32_SINT;
    case (RFMT_R32_SFLOAT):
        return VK_FORMAT_R32_SFLOAT;
    case (RFMT_R32_UINT):
        return VK_FORMAT_R32_UINT;
    case (RFMT_R32_SINT):
        return VK_FORMAT_R32_SINT;
    case (RFMT_RGBA64_SFLOAT):
        return VK_FORMAT_R64G64B64A64_SFLOAT;
    case (RFMT_RGBA64_UINT):
        return VK_FORMAT_R64G64B64A64_UINT;
    case (RFMT_RGBA64_SINT):
        return VK_FORMAT_R64G64B64A64_SINT;
    case (RFMT_RGB64_SFLOAT):
        return VK_FORMAT_R64G64B64_SFLOAT;
    case (RFMT_RGB64_UINT):
        return VK_FORMAT_R64G64B64_UINT;
    case (RFMT_RGB64_SINT):
        return VK_FORMAT_R64G64B64_SINT;
    case (RFMT_RG64_SFLOAT):
        return VK_FORMAT_R64G64_SFLOAT;
    case (RFMT_RG64_UINT):
        return VK_FORMAT_R64G64_UINT;
    case (RFMT_RG64_SINT):
        return VK_FORMAT_R64G64_SINT;
    case (RFMT_R64_SFLOAT):
        return VK_FORMAT_R64_SFLOAT;
    case (RFMT_R64_UINT):
        return VK_FORMAT_R64_UINT;
    case (RFMT_R64_SINT):
        return VK_FORMAT_R64_SINT;
    case (RFMT_D16_UNORM):
        return VK_FORMAT_D16_UNORM;
    case (RFMT_D16_UNORM_S8_UINT):
        return VK_FORMAT_D16_UNORM_S8_UINT;
    case (RFMT_D32_SFLOAT):
        return VK_FORMAT_D32_SFLOAT;
    case (RFMT_D24_UNORM_S8_UINT):
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case (RFMT_D32_SFLOAT_S8_UINT):
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case (RFMT_INVALID):
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
