#pragma once
#include "basic_types.h"
#include "hashfuncs.h"

namespace nslib
{
// Rendering resource id
using rres_id = u64;

// Maximum number of techniques the renderer supports
inline constexpr u32 MAX_TECHNIQUE_COUNT = 1024;
// Maximum number of materials the renderer supports
inline constexpr u32 MAX_PIPELINE_COUNT = 2048;
// Maximum number of materials the renderer supports
inline constexpr u32 MAX_MATERIAL_COUNT = 4096;
// Maximum number of materials the renderer supports
inline constexpr u32 MAX_MESH_COUNT = 4096;
// Maximum number of textures the renderer supports
inline constexpr u32 MAX_TEXTURE_COUNT = 4096;
// Maximum number of objects
inline constexpr u32 MAX_OBJECT_COUNT = 1000000;
// Max submeshes per rmesh_info - easy to change later
inline constexpr u8 MAX_SUBMESH_COUNT = 16;
// Max number of geometry layouts - each layout gets it own set of buffers
inline constexpr u8 MAX_GEOMETRY_LAYOUT_COUNT = 8;
// Max number of vert groups per layout
inline constexpr u8 MAX_GEOMETRY_STREAM_GROUP_COUNT = 8;
// Max number of texture targets we can create in the renderer
inline constexpr u8 MAX_TEXTURE_TARGET_COUNT = 32;
// Max number of buffer targets we can create in the renderer
inline constexpr u8 MAX_BUFFER_TARGET_COUNT = 32;
// Max number of rendering texture resources supported
inline constexpr u8 MAX_TEXTURE_RRESOURCE_COUNT = 16;
// Max number of rendering buffer resources supported
inline constexpr u8 MAX_BUFFER_RRESOURCE_COUNT = 16;


// Max subpasses supported in a blueprint pass
inline constexpr u8 MAX_BP_SUBPASS_COUNT = 16;
// Max number of blueprint passes in a render blueprint
inline constexpr u8 MAX_BP_PASS_COUNT = 16;
// Max number of blueprints
inline constexpr u8 MAX_BP_COUNT = 8;
// Max number of resource requirements per subpass
inline constexpr u8 MAX_BP_RESOURCE_REQUIREMENT_COUNT = 8;
// Max number of blueprint pass attachments supported
inline constexpr u8 MAX_BP_PASS_SLOT_COUNT = 16;

inline constexpr const char *SWAPCHAIN_NAME = "swapchain";
// Cannot be constexpr since hash_type is not
inline const u64 SWAPCHAIN_ID = hash_type("swapchain");


// Indice type
using ind_t = u16;

template<typename T>
struct slot_handle;

template<typename T>
struct slot_item_ref;

struct rtexture_info;
using rtexture_handle = slot_handle<rtexture_info>;
using rtexture_ref = slot_item_ref<rtexture_info>;

struct rmaterial_info;
using rmaterial_handle = slot_handle<rmaterial_info>;
using rmaterial_ref = slot_item_ref<rmaterial_info>;

struct rtechnique_info;
using rtechnique_handle = slot_handle<rtechnique_info>;
using rtechnique_ref = slot_item_ref<rtechnique_info>;

struct rgeom_info;
using rgeom_handle = slot_handle<rgeom_info>;
using rgeom_ref = slot_item_ref<rgeom_info>;

struct rtexture_target;
using rtexture_target_handle = slot_handle<rtexture_target>;
using rtexture_target_ref = slot_item_ref<rtexture_target>;

struct rbuffer_target;
using rbuffer_target_handle = slot_handle<rbuffer_target>;
using rbuffer_target_ref = slot_item_ref<rbuffer_target>;

struct render_blueprint;
using render_blueprint_handle = slot_handle<render_blueprint>;
using render_blueprint_ref = slot_item_ref<render_blueprint>;

using gpu_handle = u64;
using rbp_pass_id = u32;
using rbp_subpass_id = u32;
using rbp_resource_req_id = u32;
using rbp_slot_id = u32;

using mpass_id = u32;
using mview_id = u32;
using mrender_job_id = u32;
using pipeline_key = u64;
using framebuffer_key = u64;
using instance_id = u32;

enum rmaterial_texture_slot
{
    RMATERIAL_TEXTURE0,
    RMATERIAL_TEXTURE1,
    RMATERIAL_TEXTURE2,
    RMATERIAL_TEXTURE3,
    RMATERIAL_TEXTURE4,
    RMATERIAL_TEXTURE5,
    RMATERIAL_TEXTURE6,
    RMATERIAL_TEXTURE7,
    RMATERIAL_TEXTURE_COUNT,
};

enum rsampler_type : u32
{
    RSAMPLER_TYPE_LINEAR_REPEAT,
    RSAMPLER_TYPE_COUNT
};

enum struct rgeom_topology : u8
{
    RMESH_TOPOLOGY_TRIANGLE_STRIP,
};

enum rdesc_set_layout : u32
{
    // Bound once per frame.
    RDESC_SET_LAYOUT_FRAME,
    // Bound per material change.
    RDESC_SET_LAYOUT_MATERIAL,
    // Count
    RDESC_SET_LAYOUT_COUNT,
};


} // namespace nslib
