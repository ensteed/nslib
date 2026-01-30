#pragma once
#include "containers/hmap.h"
#include "render_defs.h"
#include "rformat.h"

namespace nslib
{
struct renderer;

enum rshader_stage_visibility
{
    VISIBILITY_VERTEX = 1 << 0,
    VISIBILITY_FRAGMENT = 1 << 1,
    VISIBILITY_COMPUTE = 1 << 2,
};

// The "Intent" of how a resource is used in a specific pass
enum struct rbp_resource_usage
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

enum resource_requirement_access_flag
{
    RESOURCE_REQUIREMENT_ACCESS_NONE = 0,
    RESOURCE_REQUIREMENT_ACCESS_READ = (1 << 0),  // Resource read - load op is read if set
    RESOURCE_REQUIREMENT_ACCESS_CLEAR = (1 << 1), // Resource cleared on load op - ignored if read is set - otherwise clear or dont care
    RESOURCE_REQUIREMENT_ACCESS_WRITE = (1 << 2), // Resource overwritten - if set then store op write otherwise store op don't care
};

enum rbp_pass_type
{
    PASS_TYPE_GRAPHICS,
    PASS_TYPE_COMPUTE
};

enum resource_requirement_option_flag : u32
{
    RESOURCE_REQUIREMENT_OPTION_NONE = 0,
    RESOURCE_REQUIREMENT_OPTION_PRESENT_KHR = (1 << 0),
};

struct rbp_resource_slot_info
{
    small_str name;
    rformat format{rformat::INVALID};
    rbp_resource_usage usage{rbp_resource_usage::UNDEFINED};
    // Only valid if slot corresponds with attachment (not necessarily true for sampled images and storage buffers)
    u32 att_ind{INVALID_ID};
};

pup_func(rbp_resource_slot_info)
{
    pup_member(name);
    pup_enum_member(rformat, u32, format);
    pup_enum_member(rbp_resource_usage, u32, usage);
    pup_member(att_ind);
}

struct rbp_resource_requirement
{
    u32 slot_ind;
    u32 access_mask;
    u32 visibility;
    u32 option_mask;
};

pup_func(rbp_resource_requirement)
{
    pup_member(slot_ind);
    pup_member(access_mask);
    pup_member(visibility);
    pup_member(option_mask);
}

struct rbp_subpass
{
    static_array<rbp_resource_requirement, MAX_BP_RESOURCE_REQUIREMENT_COUNT> resources;
};

pup_func(rbp_subpass)
{
    pup_member(resources);
}

struct rbp_pass
{
    // Set while building
    small_str name;
    rres_id id;

    rbp_pass_type type;
    bool use_subpass_bookends{false};
    static_array<rbp_resource_slot_info, MAX_BP_PASS_SLOT_COUNT> slots{};
    static_array<rbp_subpass, MAX_BP_SUBPASS_COUNT> subpasses{};

    // Filled during compile
    gpu_handle vk_handle;
};

pup_func(rbp_pass)
{
    pup_member(name);
    pup_member(id);
    pup_enum_member(rbp_pass_type, u32, type);
    pup_member(use_subpass_bookends);
    pup_member(slots);
    pup_member(subpasses);
}

struct rbp_pass_desc
{
    const char *name;
    rbp_pass_type type;
    bool use_subpass_bookends;
};

struct rbp_resouce_requirement_desc
{
    rbp_resource_requirement req;
    u32 subpass_ind{INVALID_ID};
};

struct rbp_resource_slot_desc
{
    const char *name;
    rformat format;
    rbp_resource_usage usage;
};

struct render_blueprint
{
    small_str name;
    rres_id id;
    static_array<rbp_pass, MAX_BP_PASS_COUNT> passes{};
    hmap<rres_id, rbp_pass_id> pass_idmap{};
};

pup_func(render_blueprint)
{
    pup_member(name);
    pup_member(id);
    pup_member(passes);
    pup_member(pass_idmap);
}

inline bool is_valid(const rbp_resource_slot_info &si)
{
    return (si.format != rformat::INVALID && si.usage != rbp_resource_usage::UNDEFINED);
}

u32 get_rbp_attachment_count(rbp_pass *rbp);

rbp_slot_id add_rbp_resource_slot(render_blueprint *rbp, rbp_pass_id pid, const rbp_resource_slot_desc &desc);
rbp_resource_req_id add_rbp_resource_requirement(render_blueprint *rbp,
                                                 rbp_pass_id pid,
                                                 const rbp_resource_requirement &req,
                                                 rbp_subpass_id spid = 0);
rbp_subpass_id add_rbp_subpass(render_blueprint *rbp, rbp_pass_id pid);

rbp_pass_id add_rbp_pass(render_blueprint *rbp, const rbp_pass_desc &pdesc);
rbp_pass_id find_rbp_pass(render_blueprint *rbp, rres_id id);

// Renderer takes ownership of blueprint
render_blueprint_ref create_render_blueprint(renderer *rndr, const char *name);
bool destroy_render_blueprint(renderer *rndr, render_blueprint_handle hndl);
render_blueprint* get_render_blueprint(renderer *rndr, render_blueprint_handle hndl);
render_blueprint_handle find_render_blueprint(renderer *rndr, rres_id bpid);

void clean_render_blueprint(renderer *rndr, render_blueprint *rbp);
bool compile_render_blueprint(renderer *rndr, render_blueprint *rbp);

} // namespace nslib
