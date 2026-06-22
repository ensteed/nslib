#include "vkr_dynamic_state.h"

namespace nslib
{
void vkr_init_eds1_fptrs(const VkPhysicalDeviceProperties &props, VkDevice dev, vkr_eds1_fptrs *fns)
{
    const bool have_vk13 = (VK_API_VERSION_MAJOR(props.apiVersion) > 1) ||
                           (VK_API_VERSION_MAJOR(props.apiVersion) == 1 && VK_API_VERSION_MINOR(props.apiVersion) >= 3);

    auto load = [&](const char *name) -> PFN_vkVoidFunction { return vkGetDeviceProcAddr(dev, name); };

    if (have_vk13) {
        // Prefer core 1.3 entrypoints (no  suffix)
        fns->vkCmdSetCullMode = (PFN_vkCmdSetCullMode)(load("vkCmdSetCullMode"));
        fns->vkCmdSetFrontFace = (PFN_vkCmdSetFrontFace)(load("vkCmdSetFrontFace"));
        fns->vkCmdSetPrimitiveTopology = (PFN_vkCmdSetPrimitiveTopology)(load("vkCmdSetPrimitiveTopology"));

        fns->vkCmdSetViewportWithCount = (PFN_vkCmdSetViewportWithCount)(load("vkCmdSetViewportWithCount"));
        fns->vkCmdSetScissorWithCount = (PFN_vkCmdSetScissorWithCount)(load("vkCmdSetScissorWithCount"));

        fns->vkCmdBindVertexBuffers2 = (PFN_vkCmdBindVertexBuffers2)(load("vkCmdBindVertexBuffers2"));

        fns->vkCmdSetDepthTestEnable = (PFN_vkCmdSetDepthTestEnable)(load("vkCmdSetDepthTestEnable"));
        fns->vkCmdSetDepthWriteEnable = (PFN_vkCmdSetDepthWriteEnable)(load("vkCmdSetDepthWriteEnable"));
        fns->vkCmdSetDepthCompareOp = (PFN_vkCmdSetDepthCompareOp)(load("vkCmdSetDepthCompareOp"));
        fns->vkCmdSetDepthBoundsTestEnable = (PFN_vkCmdSetDepthBoundsTestEnable)(load("vkCmdSetDepthBoundsTestEnable"));

        fns->vkCmdSetStencilTestEnable = (PFN_vkCmdSetStencilTestEnable)(load("vkCmdSetStencilTestEnable"));
        fns->vkCmdSetStencilOp = (PFN_vkCmdSetStencilOp)(load("vkCmdSetStencilOp"));
    }
    else {
        // Fall back to VK__extended_dynamic_state entrypoints ( suffix)
        fns->vkCmdSetCullMode = (PFN_vkCmdSetCullMode)(load("vkCmdSetCullModeEXT"));
        fns->vkCmdSetFrontFace = (PFN_vkCmdSetFrontFace)(load("vkCmdSetFrontFaceEXT"));
        fns->vkCmdSetPrimitiveTopology = (PFN_vkCmdSetPrimitiveTopology)(load("vkCmdSetPrimitiveTopologyEXT"));

        fns->vkCmdSetViewportWithCount = (PFN_vkCmdSetViewportWithCount)(load("vkCmdSetViewportWithCountEXT"));
        fns->vkCmdSetScissorWithCount = (PFN_vkCmdSetScissorWithCount)(load("vkCmdSetScissorWithCountEXT"));

        fns->vkCmdBindVertexBuffers2 = (PFN_vkCmdBindVertexBuffers2)(load("vkCmdBindVertexBuffers2EXT"));

        fns->vkCmdSetDepthTestEnable = (PFN_vkCmdSetDepthTestEnable)(load("vkCmdSetDepthTestEnableEXT"));
        fns->vkCmdSetDepthWriteEnable = (PFN_vkCmdSetDepthWriteEnable)(load("vkCmdSetDepthWriteEnableEXT"));
        fns->vkCmdSetDepthCompareOp = (PFN_vkCmdSetDepthCompareOp)(load("vkCmdSetDepthCompareOpEXT"));
        fns->vkCmdSetDepthBoundsTestEnable = (PFN_vkCmdSetDepthBoundsTestEnable)(load("vkCmdSetDepthBoundsTestEnableEXT"));

        fns->vkCmdSetStencilTestEnable = (PFN_vkCmdSetStencilTestEnable)(load("vkCmdSetStencilTestEnableEXT"));
        fns->vkCmdSetStencilOp = (PFN_vkCmdSetStencilOp)(load("vkCmdSetStencilOpEXT"));
    }
}
void vkr_terminate_eds1_fptrs(vkr_eds1_fptrs *fns)
{
    *fns = {};
}

} // namespace nslib
