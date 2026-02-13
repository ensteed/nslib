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
inline constexpr u32 MAX_GEOM_COUNT = 4096;
// Maximum number of textures the renderer supports
inline constexpr u32 MAX_TEXTURE_COUNT = 4096;
// Maximum number of objects
inline constexpr u32 MAX_OBJECT_COUNT = 1000000;
// Max subgeometry per rgeom_info - easy to change later
inline constexpr u8 MAX_SUBGEOM_COUNT = 16;
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

inline constexpr f32 DEFAULT_DEPTH_CLEAR = 1.0f;

inline constexpr u32 DEFAULT_STENCIL_CLEAR = 0;

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

struct rshader_info;
using rshader_handle = slot_handle<rshader_info>;
using rshader_ref = slot_item_ref<rshader_info>;

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

enum rshader_stage_flag
{
    RSHADER_STAGE_VERTEX_BIT = 1 << 0,
    RSHADER_STAGE_FRAGMENT_BIT = 1 << 1,
    RSHADER_STAGE_COMPUTE_BIT = 1 << 2,
};

enum rpolygon_mode
{
    RPOLYGON_MODE_FILL,
    RPOLYGON_MODE_LINE,
    RPOLYGON_MODE_POINT,
};

enum rtechnique_flag
{
    RTECHNIQUE_FLAG_PRIMITIVE_RESTART_ENABLED = (1 << 0),
    RTECHNIQUE_FLAG_CULL_BACK = (1 << 1),
    RTECHNIQUE_FLAG_CULL_FRONT = (1 << 2),
    RTECHNIQUE_FLAG_CLAMP_DEPTH = (1 << 3),
    RTECHNIQUE_FLAG_DISCARD_RASTERIZER = (1 << 4),
    RTECHNIQUE_FLAG_SAMPLE_SHADING = (1 << 5),
    RTECHNIQUE_FLAG_MS_ALPHA_TO_COVERAGE = (1 << 6),
    RTECHNIQUE_FLAG_MS_ALPHA_TO_ONE = (1 << 7),
    RTECHNIQUE_FLAG_DEPTH_TEST = (1 << 8),
    RTECHNIQUE_FLAG_DEPTH_WRITE = (1 << 9),
    RTECHNIQUE_FLAG_DEPTH_BOUNDS_TEST = (1 << 10),
    RTECHNIQUE_FLAG_STENCIL_TEST = (1 << 11),
    RTECHNIQUE_FLAG_BLEND_LOGIC_OP = (1 << 12),
};
using rtechnique_flags = u32;

enum rcompare_op
{
    RCOMPARE_OP_NEVER = 0,
    RCOMPARE_OP_LESS = 1,
    RCOMPARE_OP_EQUAL = 2,
    RCOMPARE_OP_LESS_OR_EQUAL = 3,
    RCOMPARE_OP_GREATER = 4,
    RCOMPARE_OP_NOT_EQUAL = 5,
    RCOMPARE_OP_GREATER_OR_EQUAL = 6,
    RCOMPARE_OP_ALWAYS = 7,
};

enum rstencil_op
{
    RSTENCIL_OP_KEEP = 0,
    RSTENCIL_OP_ZERO = 1,
    RSTENCIL_OP_REPLACE = 2,
    RSTENCIL_OP_INCREMENT_AND_CLAMP = 3,
    RSTENCIL_OP_DECREMENT_AND_CLAMP = 4,
    RSTENCIL_OP_INVERT = 5,
    RSTENCIL_OP_INCREMENT_AND_WRAP = 6,
    RSTENCIL_OP_DECREMENT_AND_WRAP = 7,
};

// Provided by VK_VERSION_1_0
enum rlogic_op
{
    RLOGIC_OP_CLEAR = 0,
    RLOGIC_OP_AND = 1,
    RLOGIC_OP_AND_REVERSE = 2,
    RLOGIC_OP_COPY = 3,
    RLOGIC_OP_AND_INVERTED = 4,
    RLOGIC_OP_NO_OP = 5,
    RLOGIC_OP_XOR = 6,
    RLOGIC_OP_OR = 7,
    RLOGIC_OP_NOR = 8,
    RLOGIC_OP_EQUIVALENT = 9,
    RLOGIC_OP_INVERT = 10,
    RLOGIC_OP_OR_REVERSE = 11,
    RLOGIC_OP_COPY_INVERTED = 12,
    RLOGIC_OP_OR_INVERTED = 13,
    RLOGIC_OP_NAND = 14,
    RLOGIC_OP_SET = 15,
};

enum rblend_op
{
    RBLEND_OP_ADD = 0,
    RBLEND_OP_SUBTRACT = 1,
    RBLEND_OP_REVERSE_SUBTRACT = 2,
    RBLEND_OP_MIN = 3,
    RBLEND_OP_MAX = 4,
};

enum rblend_factor
{
    RBLEND_FACTOR_ZERO = 0,
    RBLEND_FACTOR_ONE = 1,
    RBLEND_FACTOR_SRC_COLOR = 2,
    RBLEND_FACTOR_ONE_MINUS_SRC_COLOR = 3,
    RBLEND_FACTOR_DST_COLOR = 4,
    RBLEND_FACTOR_ONE_MINUS_DST_COLOR = 5,
    RBLEND_FACTOR_SRC_ALPHA = 6,
    RBLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 7,
    RBLEND_FACTOR_DST_ALPHA = 8,
    RBLEND_FACTOR_ONE_MINUS_DST_ALPHA = 9,
    RBLEND_FACTOR_CONSTANT_COLOR = 10,
    RBLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = 11,
    RBLEND_FACTOR_CONSTANT_ALPHA = 12,
    RBLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = 13,
    RBLEND_FACTOR_SRC_ALPHA_SATURATE = 14,
    RBLEND_FACTOR_SRC1_COLOR = 15,
    RBLEND_FACTOR_ONE_MINUS_SRC1_COLOR = 16,
    RBLEND_FACTOR_SRC1_ALPHA = 17,
    RBLEND_FACTOR_ONE_MINUS_SRC1_ALPHA = 18,
};

enum rcolor_component_flag
{
    RCOLOR_COMPONENT_R = (1 << 0),
    RCOLOR_COMPONENT_G = (1 << 1),
    RCOLOR_COMPONENT_B = (1 << 2),
    RCOLOR_COMPONENT_A = (1 << 3),
};
using rcolor_component_flags = u32;

enum rfront_face_winding
{
    RFRONT_FACE_WINDING_CCW,
    RFRONT_FACE_WINDING_CW,
};

enum rsample_count : u8
{
    RSAMPLE_COUNT_1X,
    RSAMPLE_COUNT_2X,
    RSAMPLE_COUNT_4X,
    RSAMPLE_COUNT_8X,
    RSAMPLE_COUNT_16X
};

enum rmaterial_texture_slot : u8
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
    RGEOM_TOPOLOGY_POINT_LIST,
    RGEOM_TOPOLOGY_LINE_LIST,
    RGEOM_TOPOLOGY_LINE_STRIP,
    RGEOM_TOPOLOGY_TRIANGLE_LIST,
    RGEOM_TOPOLOGY_TRIANGLE_STRIP,
    RGEOM_TOPOLOGY_TRIANGLE_FAN,
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
