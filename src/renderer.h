#pragma once

#include "asset_id.h"
#include "math/matrix4.h"
#include "containers/slot_pool.h"
#include "containers/hmap.h"
#include "render_blueprint.h"
#include "render_handles.h"
#include "rformat.h"

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

enum struct rgeom_topology : u8
{
    RMESH_TOPOLOGY_TRIANGLE_STRIP,
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
{};

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

struct vert_attrib_desc
{
    u32 shader_location;
    rformat fmt;
};

struct vert_stream_desc
{
    const char *dbg_name;
    static_array<vert_attrib_desc, MAX_VERT_ATTRIBS> attribs;
};

struct geometry_vert_layout_desc
{
    static_array<vert_stream_desc, MAX_VERT_BINDINGS> streams;
    u32 max_vert_count;
};

struct geometry_stream_group_desc
{
    const char *name;
    static_array<geometry_vert_layout_desc, MAX_GEOMETRY_LAYOUT_COUNT> layouts;
    u32 max_ind_count;
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
    small_str name;
    vkr_buffer buffer;
};

struct geometry_buffer_layout_entry
{
    vkr_vertex_layout vert_layout;
    VmaVirtualBlock vert_block;
    static_array<stream_buffer_entry, 16> vert_streams;
};

// Geometry stream groups all share the same indice buffer, so can all bound bound at the same time
struct geom_streams_group
{
    // The name is stored in the stream buffer entry for indices - the id is generated from that name
    resource_id id{INVALID_IND};
    runtime_id ind{INVALID_ID};
    static_array<geometry_buffer_layout_entry, MAX_GEOMETRY_LAYOUT_COUNT> layouts{};
    VmaVirtualBlock indices_block{VK_NULL_HANDLE};
    stream_buffer_entry indice_stream;
};

struct rgeom_create_info
{
    const char *name{};
    // Which geometry streams group
    runtime_id group{};
    // The specific layout to use within the geometry streams group
    runtime_id layout{};
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
    // Topology
    rgeom_topology topology;
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
    hmap<resource_id, runtime_id> geom_group_id_map{};
    static_array<geom_streams_group, MAX_GEOMETRY_STREAM_GROUP_COUNT> geom_groups;

    // global pipeline layout
    VkPipelineLayout g_layout{VK_NULL_HANDLE};

    // Transient pool for image transfers and such
    VkCommandPool transient_pool;

    // Global texture samplers
    static_array<rsampler_info, RSAMPLER_TYPE_COUNT> samplers{};

    // ImGUI context
    #ifdef USE_IMGUI
    imgui_ctxt imgui{};
    #endif

    // Stored on reset render frame - used in subsequent frame calls to get the current frame
    s32 finished_frames;

    // This is incremented every frame there are no resize events
    f64 no_resize_frames;

    // Render blueprints - last one is active one
    hmap<resource_id, runtime_id> blueprint_id_map{};
    static_array<render_blueprint, MAX_BP_COUNT> blueprints{};

    rtexture_handle swapchain_fb_depth_stencil{};
};

geom_streams_group* push_geometry_stream_group(renderer *rndr, const geometry_stream_group_desc &desc);

// Geometry group desc builder
geometry_vert_layout_desc *push_geometry_layout(geometry_stream_group_desc *desc, u32 layout_max_vert_count);
vert_stream_desc *push_geometry_stream(geometry_vert_layout_desc *vert_layout, const char *dbg_name);
void push_geometry_attribute(vert_stream_desc *stream, const vert_attrib_desc &att_desc);

template<typename T>
void push_geometry_attribute(vert_stream_desc *stream, u32 shader_location) {
    push_geometry_attribute(stream, {.shader_location=shader_location, .fmt = get_rformat_for_type<T>()});
}

template<typename T>
void push_geometry_attribute(vert_stream_desc *stream, u32 shader_location, bool normalize_in_shader) {
    push_geometry_attribute(stream, {.shader_location=shader_location, .fmt = get_rformat_for_type<T>(normalize_in_shader)});
}

rgeom_handle create_geometry(renderer *rndr, const rgeom_create_info &ci);
rtexture_handle create_texture(renderer *rndr, const rtexture_create_info &ctinfo);
rtexture_handle create_rtechnique(renderer *rndr, const rtechnique_create_info &ctinfo);
rtexture_handle create_material(renderer *rndr, const rmaterial_create_info &ctinfo);

int begin_render_frame(renderer *rndr, int finished_frames);
int end_render_frame(renderer *rndr, camera *cam, f64 dt);

int init_renderer(renderer *rndr, void *win_hndl, mem_arena *fl_arena);
void terminate_renderer(renderer *rndr);

} // namespace nslib
