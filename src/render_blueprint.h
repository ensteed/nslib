#pragma once
#include "vk_context.h"
#include "containers/hmap.h"
#include "render_defs.h"

namespace nslib
{
struct renderer;
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

struct rtarget_res_buffer
{
    // Set during build
    small_str name;
    resource_id id;
    vkr_buffer_cfg buf_cfg{};
    
    // Filled after load if loading from disk
    runtime_id ind;

    // Filled in during compile
    vkr_buffer buffers[MAX_FRAMES_IN_FLIGHT];
    rbuffer_state states[MAX_FRAMES_IN_FLIGHT]; // Current state of each frame's image
};

struct rtarget_res_texture
{
    // Set during build
    small_str name;
    resource_id id;
    vkr_image_cfg img_cfg{};

    // Filled after load if loading from disk
    runtime_id ind;

    // Fille during compile
    vkr_image images[MAX_FRAMES_IN_FLIGHT];
    VkImageView views[MAX_FRAMES_IN_FLIGHT];
    rtexture_state states[MAX_FRAMES_IN_FLIGHT]; // Current state of each frame's image
};

struct rtarget_res_registry
{
    static_array<rtarget_res_buffer, MAX_BUFFER_RRESOURCE_COUNT> buffers;
    hmap<resource_id, runtime_id> buffer_id_map;
    static_array<rtarget_res_texture, MAX_TEXTURE_RRESOURCE_COUNT> textures;
    hmap<resource_id, runtime_id> texture_id_map;
};

enum rshader_stage_visibility
{
    VISIBILITY_VERTEX = 1 << 0,
    VISIBILITY_FRAGMENT = 1 << 1,
    VISIBILITY_COMPUTE = 1 << 2,
};

// The "Intent" of how a resource is used in a specific pass
enum struct rtarget_res_usage
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

enum rtarget_res_requirement_flag
{
    RESOURCE_REQUIREMENT_FLAG_READ,  // Resource read - load op is read if set
    RESOURCE_REQUIREMENT_FLAG_CLEAR, // Resource cleared on load op - ignored if read is set - otherwise clear or dont care
    RESOURCE_REQUIREMENT_FLAG_WRITE, // Resource overwritten - if set then store op write otherwise store op don't care
};

enum rbp_pass_type
{
    PASS_TYPE_GRAPHICS,
    PASS_TYPE_COMPUTE
};

struct rtarget_res_requirement
{
    // Set while building
    resource_id resid;
    rtarget_res_usage usage;
    u32 access;
    u32 visibility;

    // Filled during compile
    runtime_id id{INVALID_ID};
};

struct rbp_subpass
{
    static_array<rtarget_res_requirement, MAX_BP_PASS_ATTACHMENT_COUNT> resources;
};

struct rbp_pass
{
    // Set while building
    small_str name;
    resource_id id;
    rbp_pass_type type;
    bool use_subpass_bookends{false};
    static_array<rbp_subpass, MAX_BP_SUBPASS_COUNT> subpasses{};

    // Populated after load (if loading from disk)
    runtime_id ind;

    // Filld during compile
    VkRenderPass handle;
};

struct render_blueprint
{
    small_str name;
    resource_id id;
    static_array<rbp_pass, MAX_BP_PASS_COUNT> passes{};
    rtarget_res_registry targets{};

    // Populated after load (if loading from disk)
    runtime_id ind;
    hmap<resource_id, runtime_id> pass_idmap{};
};

const runtime_id DEFAULT_SUBPASS_ID = 0;

rtarget_res_requirement *push_res_requirement(rbp_pass *rbp, const char *name, runtime_id subpass = DEFAULT_SUBPASS_ID);
runtime_id push_rbp_subpass(rbp_pass *pass);
rbp_pass *push_rbp_pass(render_blueprint *rbp, const char *name);
runtime_id find_rbp_pass(render_blueprint *rbp, resource_id resid);

rtarget_res_texture *push_target_texture(render_blueprint *rbp, const char *name);
rtarget_res_texture *push_target_texture(rtarget_res_registry *reg, const char *name);
rtarget_res_buffer *push_target_buffer(render_blueprint *rbp, const char *name);
rtarget_res_buffer *push_target_buffer(rtarget_res_registry *reg, const char *name);


// Renderer takes ownership of blueprint
render_blueprint *push_render_blueprint(const char *name, renderer *rndr);
runtime_id find_render_blueprint(resource_id bpid, renderer *rndr);

void clean_render_blueprint(render_blueprint *rbp, renderer *rndr);
bool compile_render_blueprint(render_blueprint *rbp, renderer *rndr);

} // namespace nslib
