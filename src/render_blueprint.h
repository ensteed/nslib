#pragma once
#include "containers/hmap.h"
#include "render_defs.h"
#include "rformat.h"

namespace nslib
{
struct renderer;

// The "Intent" of how a resource is used in a specific pass
enum rbp_resource_usage
{
    RBP_RES_USAGE_COLOR_ATTACHMENT,         // Written to via Rasterizer
    RBP_RES_USAGE_DEPTH_ATTACHMENT,         // Depth/Stencil testing
    RBP_RES_USAGE_STENCIL_ATTACHMENT,       // Depth/Stencil testing
    RBP_RES_USAGE_DEPTH_STENCIL_ATTACHMENT, // Depth/Stencil testing
    RBP_RES_USAGE_INPUT_ATTACHMENT,         // Read via subpassLoad() (on-chip)
    RBP_RES_USAGE_SAMPLED_IMAGE,            // Read via texture() (from VRAM)
    RBP_RES_USAGE_STORAGE_BUFFER,           // Read/Write via SSBO
    RBP_RES_USAGE_UNDEFINED
};

enum rbp_resource_usage_flag
{
    RBP_RES_USAGE_FLAG_COLOR_ATTACHMENT = make_flag(RBP_RES_USAGE_COLOR_ATTACHMENT),
    RBP_RES_USAGE_FLAG_DEPTH_ATTACHMENT = make_flag(RBP_RES_USAGE_DEPTH_ATTACHMENT),
    RBP_RES_USAGE_FLAG_STENCIL_ATTACHMENT = make_flag(RBP_RES_USAGE_STENCIL_ATTACHMENT),
    RBP_RES_USAGE_FLAG_DEPTH_STENCIL_ATTACHMENT = make_flag(RBP_RES_USAGE_DEPTH_STENCIL_ATTACHMENT),
    RBP_RES_USAGE_FLAG_INPUT_ATTACHMENT = make_flag(RBP_RES_USAGE_INPUT_ATTACHMENT),
    RBP_RES_USAGE_FLAG_SAMPLED_IMAGE = make_flag(RBP_RES_USAGE_SAMPLED_IMAGE),
    RBP_RES_USAGE_FLAG_STORAGE_BUFFER = make_flag(RBP_RES_USAGE_STORAGE_BUFFER),
    RBP_RES_USAGE_FLAGS_OUPUT_ATTACHMENT = RBP_RES_USAGE_FLAG_COLOR_ATTACHMENT | RBP_RES_USAGE_FLAG_DEPTH_ATTACHMENT |
                                           RBP_RES_USAGE_FLAG_STENCIL_ATTACHMENT | RBP_RES_USAGE_FLAG_DEPTH_STENCIL_ATTACHMENT,
    RBP_RES_USAGE_FLAGS_ANY_ATTACHMENT = RBP_RES_USAGE_FLAG_COLOR_ATTACHMENT | RBP_RES_USAGE_FLAG_DEPTH_ATTACHMENT |
                                         RBP_RES_USAGE_FLAG_STENCIL_ATTACHMENT | RBP_RES_USAGE_FLAG_DEPTH_STENCIL_ATTACHMENT |
                                         RBP_RES_USAGE_FLAG_INPUT_ATTACHMENT,
};

using rbp_resource_usage_flags = u32;

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
    small_str name{};
    rformat format{RFMT_INVALID};
    rbp_resource_usage usage{RBP_RES_USAGE_UNDEFINED};
    // Only valid if slot corresponds with attachment (not necessarily true for sampled images and storage buffers)
    u32 att_ind{INVALID_IDX};
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

struct rmultisample_settings
{
    u32 ms_flags;
    rsample_count sc;
    float min_sample_shading{1.0};
};

pup_func(rmultisample_settings)
{
    pup_member(ms_flags);
    pup_enum_member(rsample_count, u8, sc);
}

op_eq_func(rmultisample_settings)
{
    return lhs.ms_flags == rhs.ms_flags && lhs.sc == rhs.sc && fequals(lhs.min_sample_shading, rhs.min_sample_shading);
}

op_neq_func(rmultisample_settings);

struct rmultisample_info
{
    bool use_override;
    rmultisample_settings override;
};

pup_func(rmultisample_info)
{
    pup_member(use_override);
    pup_member(override);
}

struct rbp_pass
{
    // Set while building
    small_str name{};
    rid id;

    rbp_pass_type type;
    bool use_subpass_bookends{false};
    static_array<rbp_resource_slot_info, MAX_BP_PASS_SLOT_COUNT> slots{};
    static_array<rbp_subpass, MAX_BP_SUBPASS_COUNT> subpasses{};
    rid geom_streams_group;
    rmultisample_info msi;

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
    pup_member(geom_streams_group);
}

struct rbp_pass_desc
{
    const char *name;
    rbp_pass_type type;
    bool use_subpass_bookends;
    // Which stream group does this pass use? All vert buffers within a stream group share a single indice buffer,
    // but there can be multiple groups of vert buffers. Each group of vert buffers are allocated together, and use the
    // first buffer in the group's allocation/free offsets for all of the other buffers in the group.
    rid geom_streams_group;
    // Null if no override, other wise the override settings to use
    const rmultisample_settings *override;
};

struct rbp_resouce_requirement_desc
{
    rbp_resource_requirement req;
    u32 subpass_ind{INVALID_IDX};
};

struct rbp_resource_slot_desc
{
    const char *name;
    rformat format;
    rbp_resource_usage usage;
};

struct render_blueprint
{
    small_str name{};
    rid id;
    static_array<rbp_pass, MAX_BP_PASS_COUNT> passes{};
    hmap<rid, idx_t> pass_idmap{};
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
    return (si.format != RFMT_INVALID && si.usage != RBP_RES_USAGE_UNDEFINED);
}

u32 get_rbp_slot_count(const rbp_pass &rbp, rbp_resource_usage_flags flags = RBP_RES_USAGE_FLAGS_ANY_ATTACHMENT);
bool is_usage_attachment(rbp_resource_usage usage);

idx_t add_rbp_resource_slot(render_blueprint *rbp, idx_t pid, const rbp_resource_slot_desc &desc);
idx_t add_rbp_resource_requirement(render_blueprint *rbp,
                                                  idx_t pid,
                                                  const rbp_resource_requirement &req,
                                                  idx_t spid = 0);
idx_t add_rbp_subpass(render_blueprint *rbp, idx_t pid);

idx_t add_rbp_pass(render_blueprint *rbp, const rbp_pass_desc &pdesc);
idx_t find_rbp_pass(render_blueprint *rbp, rid id);

// Renderer takes ownership of blueprint
render_blueprint_ref create_render_blueprint(renderer *rndr, const char *name);
bool destroy_render_blueprint(renderer *rndr, render_blueprint_handle hndl);
render_blueprint *get_render_blueprint(renderer *rndr, render_blueprint_handle hndl);
render_blueprint_ref find_render_blueprint(renderer *rndr, rid bpid);

void clean_render_blueprint(renderer *rndr, render_blueprint *rbp);
bool compile_render_blueprint(renderer *rndr, render_blueprint *rbp);

} // namespace nslib
