#include "vkr_utils.h"
#include "render_manifest.h"
#include "render_blueprint.h"

namespace nslib
{

VkImageLayout get_vk_layout_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req, bool is_final)
{
    bool is_write = test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_WRITE);
    bool is_present_khr = test_flags(req.option_mask, RESOURCE_REQUIREMENT_OPTION_PRESENT_KHR);
    rbp_resource_usage usage = pass.slots[req.slot_ind].usage;
    asrt(!is_present_khr || usage == rbp_resource_usage::COLOR_ATTACHMENT);
    switch (usage) {
    case rbp_resource_usage::COLOR_ATTACHMENT:
        return (is_final && is_present_khr) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case rbp_resource_usage::DEPTH_ATTACHMENT:
    case rbp_resource_usage::STENCIL_ATTACHMENT:
    case rbp_resource_usage::DEPTH_STENCIL_ATTACHMENT:
        return is_write ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case rbp_resource_usage::SAMPLED_IMAGE:
        asrt(!is_write);
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case rbp_resource_usage::INPUT_ATTACHMENT:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case rbp_resource_usage::STORAGE_BUFFER:
        return VK_IMAGE_LAYOUT_GENERAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkAccessFlags get_vk_access_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &r)
{
    VkAccessFlags access = 0;
    bool is_read = (r.access_mask & RESOURCE_REQUIREMENT_ACCESS_READ);
    bool is_write = (r.access_mask & RESOURCE_REQUIREMENT_ACCESS_WRITE);

    switch (pass.slots[r.slot_ind].usage) {
    case rbp_resource_usage::COLOR_ATTACHMENT:
        if (is_read) access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        if (is_write) access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case rbp_resource_usage::DEPTH_ATTACHMENT:
    case rbp_resource_usage::STENCIL_ATTACHMENT:
    case rbp_resource_usage::DEPTH_STENCIL_ATTACHMENT:
        if (is_read) access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (is_write) access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case rbp_resource_usage::INPUT_ATTACHMENT:
        asrt(is_read);
        if (is_read) access |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    case rbp_resource_usage::SAMPLED_IMAGE:
        asrt(is_read);
        // Sampled images are effectively read-only in the shader
        if (is_read) access |= VK_ACCESS_SHADER_READ_BIT;
        break;
    case rbp_resource_usage::STORAGE_BUFFER:
        // Storage buffers/images can be both
        if (is_read) access |= VK_ACCESS_SHADER_READ_BIT;
        if (is_write) access |= VK_ACCESS_SHADER_WRITE_BIT;
        break;
    case rbp_resource_usage::UNDEFINED:
        // Do nothing - leave at 0
        break;
    }
    return access;
}

VkPipelineStageFlags get_vk_stage_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req)
{
    // 1. If it's a Compute pass, everything happens in the Compute Shader stage.
    if (pass.type == rbp_pass_type::PASS_TYPE_COMPUTE) {
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    // 2. If it's a Graphics pass, determine stage by usage.
    switch (pass.slots[req.slot_ind].usage) {
    case rbp_resource_usage::COLOR_ATTACHMENT:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case rbp_resource_usage::DEPTH_ATTACHMENT:
    case rbp_resource_usage::STENCIL_ATTACHMENT:
    case rbp_resource_usage::DEPTH_STENCIL_ATTACHMENT:
        // Combine both test stages to be safe
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case rbp_resource_usage::INPUT_ATTACHMENT:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case rbp_resource_usage::SAMPLED_IMAGE:
    case rbp_resource_usage::STORAGE_BUFFER: {
        // Use the visibility flags to determine the specific shader stages
        VkPipelineStageFlags stages = 0;
        if (req.visibility & RSHADER_STAGE_VERTEX_BIT) stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (req.visibility & RSHADER_STAGE_FRAGMENT_BIT) stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        // Fallback if no visibility is set
        return (stages != 0) ? stages : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    default:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkImageLayout get_baked_initial_vk_layout(const rbp_pass &pass, const rbp_resource_requirement &req)
{
    // Make sure we can't have both CLEAR and READ set
    asrt(!test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR) || !test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_READ));

    // Optimization: If we are clearing and don't care about previous contents,
    // we tell the render pass to ignore the current layout and just treat it as undefined.
    return test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR) ? VK_IMAGE_LAYOUT_UNDEFINED
                                                                          : get_vk_layout_from_requirement(pass, req, false);
}

VkAttachmentLoadOp get_requirement_vk_load_op(const rbp_resource_requirement &req)
{
    if (test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_READ)) {
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    if (test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR)) {
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp get_requirement_vk_store_op(const rbp_resource_requirement &req)
{
    if (test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_WRITE)) {
        return VK_ATTACHMENT_STORE_OP_STORE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkPipelineStageFlags normalize_vk_stage_mask(VkPipelineStageFlags stage)
{
    return stage == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : stage;
}

VkClearValue get_vk_clear_value(const mpass_clear_value &cv, rformat tex_format)
{
    VkClearValue ret{};
    ret.depthStencil.depth = cv.depth;
    ret.depthStencil.stencil = cv.stencil;
    auto vkf = get_vk_format(tex_format);
    if (is_depth_stencil(vkf)) {
        return ret;
    }

    if (cv.type == mpass_clear_value::COLOR_TYPE_FLOAT) {
        if (!is_floating_point_type(vkf)) {
            wlog("Rformat %d is not floating point despite clear color floating point", tex_format);
        }
        memcpy(ret.color.float32, cv.fc.data, sizeof(vec4));
    }
    else if (cv.type == mpass_clear_value::COLOR_TYPE_SINT) {
        if (!is_sint_type(vkf)) {
            wlog("Rformat %d is not signed int despite clear color signed int", tex_format);
        }
        memcpy(ret.color.int32, cv.sc.data, sizeof(svec4));
    }
    else if (cv.type == mpass_clear_value::COLOR_TYPE_SINT) {
        if (!is_uint_type(vkf)) {
            wlog("Rformat %d is not unsigned int despite clear color unsigned int", tex_format);
        }
        memcpy(ret.color.uint32, cv.uc.data, sizeof(uvec4));
    }
    return ret;
}

VkRect2D get_vk_rect(const svec2 &pos, const uvec2 &dims)
{
    return {
        .offset{
            .x = pos.x,
            .y = pos.y,
        },
        .extent{
            .width = dims.w,
            .height = dims.h,
        },
    };
}

VkRect2D get_vk_rect(const srect &r)
{
    return {
        .offset{
            .x = r.x,
            .y = r.y,
        },
        .extent{
            .width = (u32)r.w,
            .height = (u32)r.h,
        },
    };
}

VkRect2D get_vk_rect(const urect &r)
{
    return {
        .offset{
            .x = (s32)r.x,
            .y = (s32)r.y,
        },
        .extent{
            .width = r.w,
            .height = r.h,
        },
    };
}

VkRect2D get_vk_rect_from_normalized(const rect &norm, const uvec2 &dims)
{
    return {
        .offset{
            .x = (s32)math::round(norm.x * dims.x),
            .y = (s32)math::round(norm.y * dims.y),
        },
        .extent{
            std::clamp((u32)math::round(norm.w * dims.w), 0u, dims.w),
            std::clamp((u32)math::round(norm.h * dims.h), 0u, dims.h),
        },
    };
}

VkViewport get_vk_viewport(const rect &vp, const vec2 &depth_min_max)
{
    return {
        .x = vp.x,
        .y = vp.y,
        .width = vp.w,
        .height = vp.h,
        .minDepth = std::clamp(depth_min_max.x, 0.0f, 1.0f),
        .maxDepth = std::clamp(depth_min_max.y, 0.0f, 1.0f),
    };
}

VkViewport get_vk_viewport(const rect &norm_vp, const vec2 &depth_min_max, const uvec2 &dims)
{
    return {
        .x = norm_vp.x * dims.x,
        .y = norm_vp.y * dims.y,
        .width = norm_vp.w * dims.w,
        .height = norm_vp.h * dims.h,
        .minDepth = std::clamp(depth_min_max.x, 0.0f, 1.0f),
        .maxDepth = std::clamp(depth_min_max.y, 0.0f, 1.0f),
    };
}

void fill_vk_stencil_op_state(VkStencilOpState *to_fill, const rstencil_op_state &src)
{
    to_fill->failOp = get_vk_stencil_op(src.on_fail);
    to_fill->passOp = get_vk_stencil_op(src.on_pass);
    to_fill->depthFailOp = get_vk_stencil_op(src.on_depth_fail);
    to_fill->compareOp = get_vk_compare_op(src.comp);
    to_fill->compareMask = src.comp_mask;
    to_fill->writeMask = src.write_mask;
    to_fill->reference = src.ref;
}

vk_format_info get_vk_format_info(rformat format)
{
    return get_vk_format_info(get_vk_format(format));
}

vk_format_info get_vk_format_info(const rformat_info &finfo)
{
    return {
        .block_width = finfo.block_width,
        .block_height = finfo.block_height,
        .bytes_per_block = finfo.bytes_per_block,
        .components = finfo.components,
    };
}

vk_format_info get_vk_format_info(VkFormat format)
{
    vk_format_info ret{
        .block_width = 1u,
        .block_height = 1u,
        .components = get_component_count(format),
    };
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
        ret.bytes_per_block = get_bytes_per_component(format) * ret.components;
        break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
        ret.bytes_per_block = 4;
        break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        ret.bytes_per_block = 8;
        break;
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
        ret.block_width = 4u;
        ret.block_height = 4u;
        ret.bytes_per_block = 16u;
        break;
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
        ret.block_width = 4u;
        ret.block_height = 4u;
        ret.bytes_per_block = 8u;
        break;
    case VK_FORMAT_UNDEFINED:
    default:
        asrt_break("Uhandled format");
    }
    return ret;
}

sizet calculate_vk_image_size(const vk_format_info &info, u32 width, u32 height, u32 mip_level, u32 layer_count)
{
    // Calculate mip dimensions (clamped to 1)
    u32 mip_width = std::max(1u, width >> mip_level);
    u32 mip_height = std::max(1u, height >> mip_level);

    // Calculate how many blocks are needed to cover this mip level
    u32 num_blocks_x = (mip_width + info.block_width - 1) / info.block_width;
    u32 num_blocks_y = (mip_height + info.block_height - 1) / info.block_height;

    // Size for one layer of this mip
    sizet mip_size = (sizet)(num_blocks_x * num_blocks_y * info.bytes_per_block);

    // Add all layers for this mip level
    return mip_size * layer_count;
}

sizet calculate_vk_image_buffer_size(const vk_format_info &info, u32 width, u32 height, u32 mip_levels, u32 layer_count)
{
    sizet total_size = 0;
    for (u32 i = 0; i < mip_levels; ++i) {
        total_size += calculate_vk_image_size(info, width, height, i, layer_count);
    }
    return total_size;
}

} // namespace nslib
