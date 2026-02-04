#pragma once
#include "vkr_context.h"
#include "rformat.h"

namespace nslib
{
struct rbp_resource_requirement;
struct rbp_pass;

VkImageLayout get_layout_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req, bool is_final);
VkAccessFlags get_access_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &r);
VkPipelineStageFlags get_stage_from_requirement(const rbp_pass &pass, const rbp_resource_requirement &req);
VkImageLayout get_baked_initial_layout(const rbp_pass &pass, const rbp_resource_requirement &req);
VkAttachmentLoadOp get_requirement_load_op(const rbp_resource_requirement &req);
VkAttachmentStoreOp get_requirement_store_op(const rbp_resource_requirement &req);
VkPipelineStageFlags normalize_stage_mask(VkPipelineStageFlags stage);

VkRect2D get_rect_from_normalized(const rect &norm, const uvec2 &dims);
VkRect2D get_rect(const svec2 &pos, const uvec2 &dims);
VkRect2D get_rect(const srect &r);
VkRect2D get_rect(const urect &r);

VkFormat get_format(const vkr_context *vk, rformat fmt);
VkViewport get_viewport(const rect &norm_vp, const vec2 &depth_min_max, const uvec2 &dims);
VkViewport get_viewport(const rect &vp, const vec2 &depth_min_max);

} // namespace nslib
