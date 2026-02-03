#pragma once
#include "vkr_context.h"
#include "rformat.h"

namespace nslib
{

VkRect2D vkr_get_rect_from_normalized(const rect &norm, const uvec2 &dims);
VkRect2D vkr_get_rect(const svec2 &pos, const uvec2 &dims);
VkRect2D vkr_get_rect(const srect &r);
VkRect2D vkr_get_rect(const urect &r);

VkFormat vkr_get_format(const vkr_context *vk, rformat fmt);
VkViewport vkr_get_viewport(const rect &norm_vp, const vec2 &depth_min_max, const uvec2 &dims);
VkViewport vkr_get_viewport(const rect &vp, const vec2 &depth_min_max);

} // namespace nslib
