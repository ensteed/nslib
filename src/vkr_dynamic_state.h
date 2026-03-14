#pragma once
#include "vulkan/vulkan.h"

namespace nslib
{


using PFN_vkCmdSetCullMode = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode);
using PFN_vkCmdSetFrontFace = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkFrontFace frontFace);
using PFN_vkCmdSetPrimitiveTopology = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology);

using PFN_vkCmdSetViewportWithCount = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport *pViewports);
using PFN_vkCmdSetScissorWithCount = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D *pScissors);

using PFN_vkCmdBindVertexBuffers2 = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer,
                                                         uint32_t firstBinding,
                                                         uint32_t bindingCount,
                                                         const VkBuffer *pBuffers,
                                                         const VkDeviceSize *pOffsets,
                                                         const VkDeviceSize *pSizes,
                                                         const VkDeviceSize *pStrides);

using PFN_vkCmdSetDepthTestEnable = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable);
using PFN_vkCmdSetDepthWriteEnable = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable);
using PFN_vkCmdSetDepthCompareOp = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp);
using PFN_vkCmdSetDepthBoundsTestEnable = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable);

using PFN_vkCmdSetStencilTestEnable = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable);
using PFN_vkCmdSetStencilOp = void(VKAPI_PTR *)(VkCommandBuffer commandBuffer,
                                                   VkStencilFaceFlags faceMask,
                                                   VkStencilOp failOp,
                                                   VkStencilOp passOp,
                                                   VkStencilOp depthFailOp,
                                                   VkCompareOp compareOp);

struct vkr_eds1_fptrs
{
    PFN_vkCmdSetCullMode vkCmdSetCullMode;
    PFN_vkCmdSetFrontFace vkCmdSetFrontFace;
    PFN_vkCmdSetPrimitiveTopology vkCmdSetPrimitiveTopology;

    PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount;
    PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount;

    PFN_vkCmdBindVertexBuffers2 vkCmdBindVertexBuffers2;

    PFN_vkCmdSetDepthTestEnable vkCmdSetDepthTestEnable;
    PFN_vkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable;
    PFN_vkCmdSetDepthCompareOp vkCmdSetDepthCompareOp;
    PFN_vkCmdSetDepthBoundsTestEnable vkCmdSetDepthBoundsTestEnable;

    PFN_vkCmdSetStencilTestEnable vkCmdSetStencilTestEnable;
    PFN_vkCmdSetStencilOp vkCmdSetStencilOp;
};

void vkr_init_eds1_fptrs(const VkPhysicalDeviceProperties &props, VkDevice dev, vkr_eds1_fptrs *fns);
void vkr_terminate_eds1_fptrs(vkr_eds1_fptrs *fns);

/* - =vkCmdSetCullMode= */
/* - =vkCmdSetFrontFace= */
/* - =vkCmdSetPrimitiveTopology= */
/* - =vkCmdSetViewportWithCount= */
/* - =vkCmdSetScissorWithCount= */
/* - =vkCmdBindVertexBuffers2= */
/* - =vkCmdSetDepthTestEnable= */
/* - =vkCmdSetDepthWriteEnable= */
/* - =vkCmdSetDepthCompareOp= */
/* - =vkCmdSetDepthBoundsTestEnable= */
/* - =vkCmdSetStencilTestEnable= */
/* - =vkCmdSetStencilOp= */

} // namespace nslib
