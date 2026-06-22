#pragma once
#include "basic_types.h"
#include "hashfuncs.h"

namespace nslib
{

// Maximum number of techniques the renderer supports
inline constexpr u32 MAX_SHADER_COUNT = 1024;
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
// Max number of vert groups
inline constexpr u8 MAX_GEOMETRY_STREAM_GROUP_COUNT = 8;
// Max number of geometry (pipeline) layouts per stream group - each layout gets it own set of vert buffers
inline constexpr u8 MAX_GEOMETRY_LAYOUT_COUNT = 8;
// Max number of texture targets we can create in the renderer
inline constexpr u8 MAX_TEXTURE_TARGET_COUNT = 32;
// Max number of buffer targets we can create in the renderer
inline constexpr u8 MAX_BUFFER_TARGET_COUNT = 32;
// Max number of rendering texture resources supported
inline constexpr u8 MAX_TEXTURE_RRESOURCE_COUNT = 16;
// Max number of rendering buffer resources supported
inline constexpr u8 MAX_BUFFER_RRESOURCE_COUNT = 16;
// Max number of descriptor set layouts which are referenced by techniques
inline constexpr u8 MAX_DESCRIPTOR_SET_LAYOUT_COUNT = 32;
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

// Global descriptor set info
enum rdset_layout_type
{
    RDSET_LAYOUT_MAIN_DATA,
    RDSET_LAYOUT_IMAGES,
    RDSET_LAYOUT_COUNT,
};

enum rdset_main_data_binding:u32
{
    RDSET_MAIN_DATA_BINDING_DRAW_SSBO,
    RDSET_MAIN_DATA_BINDING_VIEW_SSBO,
    RDSET_MAIN_DATA_BINDING_PASS_SSBO,
    RDSET_MAIN_DATA_BINDING_FRAME_UBO,
    RDSET_MAIN_DATA_BINDING_INSTANCE_SSBO,
    RDSET_MAIN_DATA_BINDING_MATERIAL_SSBO,
    RDSET_MAIN_DATA_BINDING_IMMUTABLE_SAMPLERS,
    RDSET_MAIN_DATA_BINDING_COUNT,
};

enum rdset_image_binding:u32
{
    RDSET_IMAGE_BINDING_IMAGE_ARRAYS,
    RDSET_IMAGE_BINDING_COUNT,
};


// Indice type
using ind_t = u16;

template<typename T>
struct slot_handle;

template<typename T>
struct slot_item_ref;

template<typename T>
struct slot_handle;

using rtexture_pool_idx = u32;

template<typename T>
struct registry_handle
{
    rtexture_pool_idx pool_idx{INVALID_IDX};
    slot_handle<T> hndl;
};

template<typename T>
struct registry_ref
{
    rtexture_pool_idx pool_idx{INVALID_IDX};
    slot_item_ref<T> ref;
};

struct rtexture_info;
using rtexture_handle = registry_handle<rtexture_info>;
using rtexture_ref = registry_ref<rtexture_info>;

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
using rres_id = u64;

enum rshader_stage_type
{
    RSHADER_STAGE_TYPE_VERTEX,
    RSHADER_STAGE_TYPE_FRAGMENT,
    RSHADER_STAGE_TYPE_COMPUTE,
    RSHADER_STAGE_TYPE_COUNT,
};

#define RSHADER_STAGE_FLAG(stage_type) make_flag(stage_type)

enum rshader_stage_flag
{
    RSHADER_STAGE_VERTEX_BIT = make_flag(RSHADER_STAGE_TYPE_VERTEX),
    RSHADER_STAGE_FRAGMENT_BIT = make_flag(RSHADER_STAGE_TYPE_FRAGMENT),
    RSHADER_STAGE_COMPUTE_BIT = make_flag(RSHADER_STAGE_TYPE_COMPUTE),
};
using rshader_stage_flags = u8;

enum rpolygon_mode
{
    RPOLYGON_MODE_FILL,
    RPOLYGON_MODE_LINE,
    RPOLYGON_MODE_POINT,
};

enum rtechnique_flag
{
    RTECHNIQUE_FLAG_PRIMITIVE_RESTART_ENABLED = make_flag(0),
    RTECHNIQUE_FLAG_CULL_BACK = make_flag(1),
    RTECHNIQUE_FLAG_CULL_FRONT = make_flag(2),
    RTECHNIQUE_FLAG_CLAMP_DEPTH = make_flag(3),
    RTECHNIQUE_FLAG_DISCARD_RASTERIZER = make_flag(4),
    RTECHNIQUE_FLAG_SAMPLE_SHADING = make_flag(5),
    RTECHNIQUE_FLAG_MS_ALPHA_TO_COVERAGE = make_flag(6),
    RTECHNIQUE_FLAG_MS_ALPHA_TO_ONE = make_flag(7),
    RTECHNIQUE_FLAG_DEPTH_TEST = make_flag(8),
    RTECHNIQUE_FLAG_DEPTH_WRITE = make_flag(9),
    RTECHNIQUE_FLAG_DEPTH_BIAS = make_flag(10),
    RTECHNIQUE_FLAG_DEPTH_BOUNDS_TEST = make_flag(11),
    RTECHNIQUE_FLAG_STENCIL_TEST = make_flag(12),
    RTECHNIQUE_FLAG_BLEND_LOGIC_OP = make_flag(13),
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
    RCOLOR_COMPONENT_R = make_flag(0),
    RCOLOR_COMPONENT_G = make_flag(1),
    RCOLOR_COMPONENT_B = make_flag(2),
    RCOLOR_COMPONENT_A = make_flag(3),
};
using rcolor_component_flags = u32;

enum rfront_face_winding
{
    RFRONT_FACE_WINDING_CCW,
    RFRONT_FACE_WINDING_CW,
};

inline constexpr rfront_face_winding DEFAULT_FRONT_FACE_WINDING = RFRONT_FACE_WINDING_CCW;

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

enum rgeom_topology : u8
{
    RGEOM_TOPOLOGY_POINT_LIST,
    RGEOM_TOPOLOGY_LINE_LIST,
    RGEOM_TOPOLOGY_LINE_STRIP,
    RGEOM_TOPOLOGY_TRIANGLE_LIST,
    RGEOM_TOPOLOGY_TRIANGLE_STRIP,
    RGEOM_TOPOLOGY_TRIANGLE_FAN,
};

enum rdescriptor_type
{
    RDESCRIPTOR_TYPE_SAMPLER,
    RDESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    RDESCRIPTOR_TYPE_SAMPLED_IMAGE,
    RDESCRIPTOR_TYPE_STORAGE_IMAGE,
    RDESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
    RDESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
    RDESCRIPTOR_TYPE_UNIFORM_BUFFER,
    RDESCRIPTOR_TYPE_STORAGE_BUFFER,
    RDESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    RDESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    RDESCRIPTOR_TYPE_INPUT_ATTACHMENT,
    RDESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
    RDESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    RDESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV,
};

} // namespace nslib
