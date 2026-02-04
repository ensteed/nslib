#include "vkr_utils.h"
#include "render_blueprint.h"

namespace nslib
{

VkImageLayout get_layout_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req, bool is_final)
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

VkAccessFlags get_access_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &r)
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

VkPipelineStageFlags get_stage_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req)
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
        if (req.visibility & VISIBILITY_VERTEX) stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (req.visibility & VISIBILITY_FRAGMENT) stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        // Fallback if no visibility is set
        return (stages != 0) ? stages : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    default:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkImageLayout get_baked_initial_layout(const rbp_pass &pass, const rbp_resource_requirement &req)
{
    // Make sure we can't have both CLEAR and READ set
    asrt(!test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR) || !test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_READ));

    // Optimization: If we are clearing and don't care about previous contents,
    // we tell the render pass to ignore the current layout and just treat it as undefined.
    return test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR) ? VK_IMAGE_LAYOUT_UNDEFINED
                                                                          : get_layout_from_requirement(pass, req, false);
}

VkAttachmentLoadOp get_requirement_load_op(const rbp_resource_requirement &req)
{
    if (test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_READ)) {
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    if (test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR)) {
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp get_requirement_store_op(const rbp_resource_requirement &req)
{
    if (test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_WRITE)) {
        return VK_ATTACHMENT_STORE_OP_STORE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkPipelineStageFlags normalize_stage_mask(VkPipelineStageFlags stage)
{
    return stage == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : stage;
}

VkRect2D get_rect(const svec2 &pos, const uvec2 &dims)
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

VkRect2D get_rect(const srect &r)
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

VkRect2D get_rect(const urect &r)
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

VkRect2D get_rect_from_normalized(const rect &norm, const uvec2 &dims)
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

VkViewport get_viewport(const rect &vp, const vec2 &depth_min_max)
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

VkViewport get_viewport(const rect &norm_vp, const vec2 &depth_min_max, const uvec2 &dims)
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

VkFormat get_format(const vkr_context *vk, rformat fmt)
{
    switch (fmt) {
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
    case (rformat::SWAPCHAIN):
        return vk->inst.device.swapchain.format;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

} // namespace nslib
