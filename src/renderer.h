#pragma once

#include "math/matrix4.h"
#include "containers/slot_pool.h"
#include "containers/hmap.h"
#include "vk_context.h"
#include "render_handles.h"

struct ImGuiContext;

namespace nslib
{
#define INVALID_IND ((sizet) - 1)

inline const asset_id FWD_RPASS = make_asset_id("forward");
inline const asset_id PLINE_FWD_RPASS_S0_OPAQUE_COL = make_asset_id("forward-s0-opaque-col");
inline const asset_id PLINE_FWD_RPASS_S0_OPAQUE_DIFFUSE = make_asset_id("forward-s0-opaque-diffuse");

struct vkr_context;
struct camera;
struct static_model;
struct transform;

// 20 million triangles... thats a lot - works on desktop
const sizet MAX_STATIC_TRIANGLE_COUNT = 2000000;
const sizet MAX_SKINNED_TRIANGLE_COUNT = 200000;
// Default ind buffer size (holding all of our inds) in ind count (not byte size)
const sizet MAX_TOTAL_MESH_IND_COUNT = (MAX_STATIC_TRIANGLE_COUNT + MAX_SKINNED_TRIANGLE_COUNT) * 3;
// Default vert buffer size (holding all of our verts) in vert count (not byte size)
// Gemeni showed me that on average we will have 2 : 1 triangle to vert ratio
const sizet MAX_STATIC_MESH_VERT_COUNT = MAX_STATIC_TRIANGLE_COUNT / 2;
const sizet MAX_SKINNED_MESH_VERT_COUNT = MAX_SKINNED_TRIANGLE_COUNT / 2;

// Maximum number of render passes supported
// const sizet MAX_RENDERPASS_COUNT = 32;
// Maximum number of techniques the renderer supports
const sizet MAX_TECHNIQUE_COUNT = 1024;
// Maximum number of materials the renderer supports
const sizet MAX_PIPELINE_COUNT = 2048;
// Maximum number of materials the renderer supports
const sizet MAX_MATERIAL_COUNT = 4096;
// Maximum number of materials the renderer supports
const sizet MAX_MESH_COUNT = 4096;
// Maximum number of textures the renderer supports
const sizet MAX_TEXTURE_COUNT = 4096;
// Maximum number of objects
const sizet MAX_OBJECT_COUNT = 1000000;
// Max submeshes per rmesh_info - easy to change later
const sizet MAX_SUBMESH_COUNT = 16;

struct rsubmesh_range
{
    // Indice offset
    sizet offset;
    // Indice count
    sizet count;
};

struct rmesh_vert_pos_col
{
    vec3 pos;
    u32 col;
};

struct rmesh_vert_norm_tan_uv
{
    vec3 norm;
    vec3 tangent;
    vec2 uv;
};

struct rmesh_vert_bone_weights_ids
{
    // TODO: Pack these to unorm weights and u8 bonde ids
    vec4 bone_weights;
    uvec4 bone_ids;
};

using ind_t = u16;

enum struct rmesh_topology : u8
{
    RMESH_TOPOLOGY_TRIANGLE_STRIP,
};

struct rmesh_create_info
{
    const char *name;
    const rmesh_vert_pos_col *pos_col;
    const rmesh_vert_norm_tan_uv *norm_tan_uv;
    // If weight ids are none null then the mesh is skinned
    const rmesh_vert_bone_weights_ids *weights_ids;
    sizet vert_count;

    const ind_t *inds;
    sizet ind_count;

    const rsubmesh_range *sm_info;
    sizet sm_count;

    rmesh_topology topology;
};

enum struct rformat
{
    // RGBA
    RGBA8_SRGB,
    RGBA8_SRGB_COMPRESSED,
    RGBA8_UNORM,
    RGBA8_UNORM_COMPRESSED,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    // RGB
    RGB8_SRGB,
    RGB8_SRGB_COMPRESSED,
    RGB8_UNORM,
    RGB8_UNORM_COMPRESSED,
    RGB8_SNORM,
    RGB8_UINT,
    RGB8_SINT,
    // RG
    RG8_SRGB,
    RG8_UNORM,
    RG8_UNORM_COMPRESSED,
    RG8_SNORM,
    RG8_SNORM_COMPRESSED,
    RG8_UINT,
    RG8_SINT,
    // R
    R8_SRGB,
    R8_UNORM,
    R8_UNORM_COMPRESSED,
    R8_SNORM,
    R8_SNORM_COMPRESSED,
    R8_UINT,
    R8_SINT,
    // RGBA 16 bpp
    RGBA16_SFLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RGBA16_UINT,
    RGBA16_SINT,
    // RGB
    RGB16_SFLOAT,
    RGB16_UNORM,
    RGB16_SNORM,
    RGB16_UINT,
    RGB16_SINT,
    // RG
    RG16_SFLOAT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_UINT,
    RG16_SINT,
    // R
    R16_SFLOAT,
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    // RGBA 32 bpp
    RGBA32_SFLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    // RGB
    RGB32_SFLOAT,
    RGB32_UINT,
    RGB32_SINT,
    // RG
    RG32_SFLOAT,
    RG32_UINT,
    RG32_SINT,
    // R
    R32_SFLOAT,
    R32_UINT,
    R32_SINT,
};

enum rtexture_create_flag : u32
{
    RTEXTURE_CREATE_FLAG_NONE,
    RTEXTURE_CREATE_FLAG_CUBE_MAP
};

struct rtexture_create_info
{
    const char *name;
    // Pixel data
    const void *data;
    // For validation basically
    sizet data_size;
    // Texture dimensions - z is layer count
    uvec3 dims;
    // Format
    rformat format;
    // See rtexture_create_flags
    u32 flags;
};

// TODO: Rename these to something more sensible
enum rvert_stream
{
    RVERT_STREAM_POS_COL,
    RVERT_STREAM_NORM_TAN_UV,
    RVERT_STREAM_SKINNED_POS_COL,
    RVERT_STREAM_SKINNED_NORM_TAN_UV,
    RVERT_STREAM_SKINNED_BONES_WEIGHT_ID,
    RVERT_STREAM_COUNT,
};

enum rvert_layout : u32
{
    RVERT_LAYOUT_STATIC_MESH,
    RVERT_LAYOUT_SKINNED_MESH,
    RVERT_LAYOUT_COUNT
};

enum rsampler_type : u32
{
    RSAMPLER_TYPE_LINEAR_REPEAT,
    RSAMPLER_TYPE_COUNT
};

enum rpass_type
{
    RPASS_TYPE_OPAQUE,
    RPASS_TYPE_COUNT
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

namespace err_code
{
enum render
{
    RENDER_NO_ERROR,
    RENDER_INIT_FAIL,
    RENDER_SETUP_GEOMETRY_BUFFERS_FAIL,
    RENDER_LOAD_SHADERS_FAIL,
    RENDER_ACQUIRE_IMAGE_FAIL,
    RENDER_INIT_IMAGE_FAIL,
    RENDER_UPLOAD_IMAGE_FAIL,
    RENDER_INIT_IMAGE_VIEW_FAIL,
    RENDER_ADD_IMAGE_FAIL,
    RENDER_WAIT_FENCE_FAIL,
    RENDER_RESET_FENCE_FAIL,
    RENDER_SUBMIT_QUEUE_FAIL,
    RENDER_PRESENT_KHR_FAIL,
    RENDER_INIT_SAMPLER_FAIL,
};
}

struct push_constants
{
    mat4 transform;
};

struct frame_ubo_data
{
    mat4 proj_view;
};

struct material_ubo_data
{
    vec4 color;
    vec4 misc;
};

// We use a single vertex and indice buffer for all meshes
struct rmesh_info
{
    // Attached to the vert_mem allocation
    small_str name;

    // Mem for pos buf that will need to be released in pos/buf
    VmaVirtualAllocation vert_mem{VK_NULL_HANDLE};

    // This is the block that was used for the vert virtual block allocations (either static or skinned mesh pos/col
    // stream block)
    VmaVirtualBlock vert_block;

    // This is determined by taking the byte offset / sizeof(stream element)
    u32 vert_offset;

    // Mem for pos buf that will need to be released in pos/buf
    VmaVirtualAllocation ind_mem{VK_NULL_HANDLE};

    // This is the block that was used for the ind virtual block allocations
    VmaVirtualBlock ind_block;

    // This is determined by taking the byte offset / sizeof(stream element)
    u32 ind_offset;

    // Indice range for each submesh
    static_array<rsubmesh_range, MAX_SUBMESH_COUNT> submesh_vert_ind_counts;
};

struct imgui_ctxt;

struct rtexture_info
{
    small_str name;
    vkr_image im;
    VkImageView im_view;
};

struct rsampler_info
{
    VkSampler vk_hndl;
};

struct rmaterial_info
{};

struct rtechnique_info
{
    static_array<VkPipeline, RPASS_TYPE_COUNT> rpass_plines;
};

struct rpass_info
{
    VkRenderPass vk_hndl;
};

struct imgui_ctxt
{
    ImGuiContext *ctxt;
    VkDescriptorPool pool;
    VkRenderPass rpass;
    mem_arena fl;
};

struct frame_context
{
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;

    // Reset every frame
    VkDescriptorPool desc_pool;

    // Synchronization
    VkFence in_flight;
    VkSemaphore image_avail;
};

struct geometry_buffer_info
{
    VmaVirtualBlock static_mesh_block{VK_NULL_HANDLE};
    VmaVirtualBlock skinned_mesh_block{VK_NULL_HANDLE};
    VmaVirtualBlock indices_block{VK_NULL_HANDLE};
    vkr_buffer vert_buffers[RVERT_STREAM_COUNT];
    vkr_buffer ind_buffer;
};

struct rview
{};

struct renderer
{
    // Owned vulkan context and mem arenas used only for vulkan stuff
    vkr_context vk{};
    mem_arena vk_free_list;
    mem_arena vk_frame_linear;

    mem_arena persist_fl;
    mem_arena frame_stack;
    mem_arena frame_linear;

    // Render pass indices referenced by ids which are just pass names - map a pass name to a static array indice
    hmap<asset_id, sizet> rpass_name_map;
    static_array<rpass_info, RPASS_TYPE_COUNT> rpasses{};

    // Created pipelines cached on pipeline state

    // Renderer resources
    // TODO: Implement this for smarter pipeline creation
    hmap<u64, VkPipeline> pline_cache;
    slot_pool<rtechnique_info> techniques{};
    slot_pool<rmaterial_info> materials{};
    slot_pool<rtexture_info> textures{};
    slot_pool<rmesh_info> meshes{};

    // Frames in flight
    static_array<frame_context, MAX_FRAMES_IN_FLIGHT> fifs{};

    // Global descriptor set layouts (used for creating descriptor sets and pipelines)
    static_array<VkDescriptorSetLayout, RDESC_SET_LAYOUT_COUNT> set_layouts{};

    // Globabl geometry attribute buffers
    geometry_buffer_info geometry_buffers;

    // Global pipeline layout
    VkPipelineLayout g_layout{VK_NULL_HANDLE};

    // Transient pool for image transfers and such
    VkCommandPool transient_pool;

    // Global texture samplers
    static_array<rsampler_info, RSAMPLER_TYPE_COUNT> samplers{};

    // Vert layout presets
    static_array<vkr_vertex_layout, RVERT_LAYOUT_COUNT> vertex_layouts;

    // ImGUI context
    imgui_ctxt imgui{};

    // Stored on reset render frame - used in subsequent frame calls to get the current frame
    s32 finished_frames;

    // This is incremented every frame there are no resize events
    f64 no_resize_frames;

    // TEMP
    rtechnique_handle default_technique{};
    rmaterial_handle default_mat{};

    rtexture_handle swapchain_fb_depth_stencil{};
};

// rmaterial_handle register_material(rtechnique_handle technique, static_array<rtexture_handle, )
// rtexture_handle register_texture(const texture *tex, renderer *rndr);

rmesh_handle create_mesh(const rmesh_create_info &cminfo, renderer *rndr);

rtexture_handle create_texture(const rtexture_create_info &ctinfo, renderer *rndr);

// NOTE: All of these mesh operations kind of need to wait on all rendering operations to complete as they modify the
// vertex and index buffers - not sure yet if this is better done within the functions or in the caller. Also these should be done at the
// start of a frame because any indices submitted in command buffers will be invalid after these operations. It almost seems like we should
// get a list of these and then just do it at start of frame after we wait for sync if there are any to do.

int init_renderer(renderer *rndr, void *win_hndl, mem_arena *fl_arena);

int begin_render_frame(renderer *rndr, int finished_frames);

int end_render_frame(renderer *rndr, camera *cam, f64 dt);

void terminate_renderer(renderer *rndr);

} // namespace nslib
