#pragma once

#include "asset_id.h"
#include "math/matrix4.h"
#include "containers/slot_pool.h"
#include "containers/hmap.h"
#include "render_blueprint.h"
#include "render_handles.h"

struct ImGuiContext;

namespace nslib
{
struct camera;
struct transform;

enum rsampler_type : u32
{
    RSAMPLER_TYPE_LINEAR_REPEAT,
    RSAMPLER_TYPE_COUNT
};

struct rsubgeom_range
{
    // Indice offset
    sizet offset;
    // Indice count
    sizet count;
};

using ind_t = u16;

enum struct rmesh_topology : u8
{
    RMESH_TOPOLOGY_TRIANGLE_STRIP,
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
    INVALID,
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

struct rtechnique_create_info
{
};

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

struct rmaterial_create_info
{
    const char *name;
    rtechnique_handle thndl;
    rtexture_handle slots[RMATERIAL_TEXTURE_COUNT];
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
struct rgeom_info
{
    // Attached to the vert_mem allocation
    small_str name;

    // Mem for stream 0 of the vert layout
    VmaVirtualAllocation vert_mem{VK_NULL_HANDLE};

    // This is the block that was used for the virtual block allocation above
    VmaVirtualBlock vert_block;

    // This is determined by taking the byte offset / sizeof(stream element)
    u32 vert_offset;

    // Mem for index stream
    VmaVirtualAllocation ind_mem{VK_NULL_HANDLE};

    // This is the block that was used for the ind virtual block allocations
    VmaVirtualBlock ind_block;

    // This is determined by taking the byte offset / sizeof(stream element)
    u32 ind_offset;

    // Indice range for each submesh
    static_array<rsubgeom_range, MAX_SUBMESH_COUNT> subgeom_vert_ind_counts;
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
    static_array<VkPipeline, MAX_BP_PASS_COUNT> rpass_plines;
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

struct stream_buffer_entry
{
    small_str dbg_name;
    vkr_buffer buffer;
};

struct geometry_buffer_layout_entry
{
    vkr_vertex_layout vert_layout;
    VmaVirtualBlock vert_block;
    static_array<stream_buffer_entry, 16> vert_streams;
};

// Geometry stream groups all share the same indice buffer, so can all bound bound at the same time
struct geom_streams_group {
    static_array<geometry_buffer_layout_entry, MAX_GEOMETRY_LAYOUT_COUNT> layouts{};
    VmaVirtualBlock indices_block{VK_NULL_HANDLE};
    stream_buffer_entry indice_stream;
};

struct rgeom_create_info
{
    const char *name{};
    // Which geometry streams group
    slot_handle<geom_streams_group> group{};
    // The specific layout to use within the geometry streams group
    u32 layout{};
    // Number of verts in this geometry. Each stream must have the same vert count
    u32 vert_count{};
    // Should be an array of void* mem pointers - the array size matching the number of vert streams specified in layout
    const void *const *vert_data{};
    // Indice data
    const void *ind_data{};
    // The number of indices in this geometry
    u32 ind_count{};
    // The indice range for each sub geometry
    const rsubgeom_range *subgeoms{};
    // The total number of sub geometries
    u32 subgeom_cnt{};
};

struct rview
{
    mat4 proj;
    mat4 cam;
    mat4 proj_cam;
};

// int init_render_blueprint(render_blueprint *bp);
// void terminate_render_blueprint(render_blueprint *bp);

// rbp_pass *create_pass(render_blueprint *bp, const char *pass_t);
// void add_subpass(rbp_pass *pass);

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
    static_array<rpass_info, MAX_BP_PASS_COUNT> rpasses{};

    // Created pipelines cached on pipeline state

    // Renderer resources
    // TODO: Implement this for smarter pipeline creation
    hmap<u64, VkPipeline> pline_cache;
    slot_pool<rtechnique_info> techniques{};
    slot_pool<rmaterial_info> materials{};
    slot_pool<rtexture_info> textures{};
    slot_pool<rgeom_info> geometry{};

    // Frames in flight
    static_array<frame_context, MAX_FRAMES_IN_FLIGHT> fifs{};

    // Global descriptor set layouts (used for creating descriptor sets and pipelines)
    static_array<VkDescriptorSetLayout, RDESC_SET_LAYOUT_COUNT> set_layouts{};

    // Really a single
    slot_pool<geom_streams_group> geom_groups;

    // global pipeline layout
    VkPipelineLayout g_layout{VK_NULL_HANDLE};

    // Transient pool for image transfers and such
    VkCommandPool transient_pool;

    // Global texture samplers
    static_array<rsampler_info, RSAMPLER_TYPE_COUNT> samplers{};

    // ImGUI context
    imgui_ctxt imgui{};

    // Stored on reset render frame - used in subsequent frame calls to get the current frame
    s32 finished_frames;

    // This is incremented every frame there are no resize events
    f64 no_resize_frames;

    // Render blueprints - last one is active one
    hmap<resource_id, runtime_id> blueprint_id_map{};
    static_array<render_blueprint, MAX_BP_COUNT> blueprints{};

    // TEMP
    rtechnique_handle default_technique{};
    rmaterial_handle default_mat{};

    rtexture_handle swapchain_fb_depth_stencil{};
};

rgeom_handle create_geometry(renderer *rndr, const rgeom_create_info &ci);

rtexture_handle create_texture(const rtexture_create_info &ctinfo, renderer *rndr);
rtexture_handle create_rtechnique(const rtechnique_create_info &ctinfo, renderer *rndr);
rtexture_handle create_material(const rmaterial_create_info &ctinfo, renderer *rndr);

int begin_render_frame(renderer *rndr, int finished_frames);
int end_render_frame(renderer *rndr, camera *cam, f64 dt);

int init_renderer(renderer *rndr, void *win_hndl, mem_arena *fl_arena);
void terminate_renderer(renderer *rndr);

} // namespace nslib
