#pragma once
#include "vk_context.h"
#include "render_defs.h"

namespace nslib {
struct rtexture_state
{
    VkImageLayout layout;
    VkAccessFlags access;
    VkPipelineStageFlags stage;
};

struct rbuffer_state
{
    VkAccessFlags access;
    VkPipelineStageFlags stage;
};

struct rbuffer_resource
{
    vkr_buffer buffers[MAX_FRAMES_IN_FLIGHT];
    rbuffer_state states[MAX_FRAMES_IN_FLIGHT]; // Current state of each frame's image
};

struct rtexture_resource
{
    vkr_image images[MAX_FRAMES_IN_FLIGHT];
    VkImageView views[MAX_FRAMES_IN_FLIGHT];
    rtexture_state states[MAX_FRAMES_IN_FLIGHT]; // Current state of each frame's image
};

struct rresource_registry
{
    static_array<rbuffer_resource, MAX_BUFFER_RRESOURCE_COUNT> buffers;
    static_array<rtexture_resource, MAX_TEXTURE_RRESOURCE_COUNT> textures;
};

enum rshader_stage_visibility {
    VISIBILITY_VERTEX   = 1 << 0,
    VISIBILITY_FRAGMENT = 1 << 1,
    VISIBILITY_COMPUTE  = 1 << 2,
};

// The "Intent" of how a resource is used in a specific pass
enum struct rresource_usage
{
    COLOR_ATTACHMENT,         // Written to via Rasterizer
    DEPTH_ATTACHMENT,         // Depth/Stencil testing
    STENCIL_ATTACHMENT,       // Depth/Stencil testing
    DEPTH_STENCIL_ATTACHMENT, // Depth/Stencil testing
    INPUT_ATTACHMENT,         // Read via subpassLoad() (on-chip)
    SAMPLED_IMAGE,            // Read via texture() (from VRAM)
    STORAGE_BUFFER,           // Read/Write via SSBO
    UNDEFINED
};

enum rresource_requirement_flag
{
    RESOURCE_REQUIREMENT_FLAG_READ,  // Resource read - load op is read if set
    RESOURCE_REQUIREMENT_FLAG_CLEAR, // Resource cleared on load op - ignored if read is set - otherwise clear or dont care
    RESOURCE_REQUIREMENT_FLAG_WRITE, // Resource overwritten - if set then store op write otherwise store op don't care
};

enum rbp_pass_type
{
    GRAPHICS,
    COMPUTE
};

struct rresource_requirement
{
    resid id{INVALID_ID};
    rresource_usage usage;
    u32 access;
    u32 visibility;
};

struct rbp_subpass
{
    static_array<rresource_requirement, MAX_BP_PASS_ATTACHMENT_COUNT> resources;
};

struct rbp_pass
{
    rbp_pass_type type;
    bool use_subpass_bookends{false};
    union
    {
        struct
        {
            static_array<rbp_subpass, MAX_BP_SUBPASS_COUNT> subpasses;
            // The pre-baked Vulkan object
            VkRenderPass handle;
        };
        rbp_subpass p;
    };
};

struct render_blueprint
{
    small_str name;
    static_array<rbp_pass, MAX_BP_PASS_COUNT> passes{};
};


void compile_render_blueprint(render_blueprint *rbp, const rresource_registry *render_resources, const vkr_context *vk);

}
