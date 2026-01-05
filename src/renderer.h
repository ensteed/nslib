#pragma once

#include "model.h"
#include "math/matrix4.h"
#include "containers/slot_pool.h"
#include "containers/hmap.h"
#include "sim_region.h"
#include "vk_context.h"
#include "render_handles.h"

struct ImGuiContext;

namespace nslib
{
#define INVALID_IND ((sizet) - 1)

inline const rid FWD_RPASS = make_rid("forward");
inline const rid PLINE_FWD_RPASS_S0_OPAQUE_COL = make_rid("forward-s0-opaque-col");
inline const rid PLINE_FWD_RPASS_S0_OPAQUE_DIFFUSE = make_rid("forward-s0-opaque-diffuse");

struct vkr_context;
struct camera;
struct static_model;
struct transform;

// 20 million triangles... thats a lot - works on desktop
const sizet MAX_TRIANGLE_COUNT = 20000000;
// Default ind buffer size (holding all of our inds) in ind count (not byte size)
const sizet DEFAULT_IND_BUFFER_SIZE = MAX_TRIANGLE_COUNT * 3;
// Default vert buffer size (holding all of our verts) in vert count (not byte size)
// Consider there is on average 6 shared triangles per vert - i think dividing the above by 3 is plenty
const sizet DEFAULT_STATIC_MESH_VERT_BUFFER_SIZE = 1000000;
// Consider there is on average 6 shared triangles per vert - i think dividing the above by 3 is plenty
const sizet DEFAULT_SKINNED_MESH_VERT_BUFFER_SIZE = 100000;
// Initial mem pool size (in element count) for our sbuffer mem pools
const sizet MAX_FREE_SBUFFER_NODE_COUNT = 1024;
// Minimum allowed sbuffer_entry block size in the free list for verts
const sizet MIN_VERT_FREE_BLOCK_SIZE = 4;
// Minimum allowed sbuffer_entry block size in the free list for indices
const sizet MIN_IND_FREE_BLOCK_SIZE = 6;
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

struct rsubmesh_range
{
    // Indice offset
    sizet offset;
    // Indice count
    sizet count;
};

struct rstatic_mesh_vert_pos_col
{
    vec3 pos;
    u32 col;
};

struct rstatic_mesh_vert_norm_tan_uv
{
    vec3 norm;
    vec3 tangent;
    vec2 uv;
};

struct rstatic_mesh_vert_bone_weights_ids
{
    vec4 bone_weights;
    svec4 bone_ids;
};

using ind_t = u32;

struct rstatic_mesh
{
    const rstatic_mesh_vert_pos_col *pos;
    const rstatic_mesh_vert_norm_tan_uv *norm_tan_uv;
    const rstatic_mesh_vert_bone_weights_ids *weights_ids;
    sizet vert_count;
    
    const ind_t *inds;
    sizet ind_count;
    
    const rsubmesh_range *sm_info;
    sizet sm_count;
};

enum rstatic_mesh_vert_stream {
    RSTATIC_MESH_STREAM_VERT_POS_COL,
    RSTATIC_MESH_STREAM_VERT_NORM_TAN_UV,
    RSTATIC_MESH_STREAM_VERT_BONES_WEIGHT_ID,
    RSTATIC_MESH_STREAM_IND,
    RSTATIC_MESH_STREAM_COUNT,
};

enum rvert_layout : u32
{
    RVERT_LAYOUT_STATIC_MESH,
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

struct rmesh_block_virtual_alloc
{
    VmaVirtualAllocation mem{VK_NULL_HANDLE};
    // This is the byte offset / sizeof(element) amount
    u32 offset;
};

// We use a single vertex and indice buffer for all meshes
struct rmesh_info
{
    small_str name;
    
    // Number of verts and inds for each submesh
    static_array<rsubmesh_range, MAX_SUBMESH_COUNT> submesh_vert_ind_counts;

    // Vert buffers - sub0 verts, sub1 verts, sub2 verts, etc
    // All vert attribs have the same count for each submesh
    // To get the final submesh vert offset: att_streams[*_VERT_*].offset + submesh_vert_ind_counts[SUBMESH_IND].verts
    // To get the final submesh ind offset: att_streams[*_IND].offset + submesh_vert_ind_counts[SUBMESH_IND].inds
    rmesh_block_virtual_alloc att_streams[RSTATIC_MESH_STREAM_COUNT]{};
};

struct imgui_ctxt;

struct rtexture_info
{
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

struct geometry_buffer_info {
    vkr_buffer data{};
    VmaVirtualBlock block_info{VK_NULL_HANDLE};
};

struct renderer
{
    // Passed in
    mem_arena *upstream_fl_arena;

    // Owned vulkan context and mem arenas used only for vulkan stuff
    vkr_context vk{};
    mem_arena vk_free_list;
    mem_arena vk_frame_linear;

    mem_arena frame_stack;
    mem_arena frame_linear;

    // Render pass indices referenced by ids which are just pass names - map a pass name to a static array indice
    hmap<rid, sizet> rpass_name_map;
    static_array<rpass_info, RPASS_TYPE_COUNT> rpasses{};

    // Created pipelines cached on pipeline state

    // Renderer resources
    // TODO: Implement this for smarter pipeline creation
    hmap<u64, VkPipeline> pline_cache;
    slot_pool<rtechnique_info, MAX_TECHNIQUE_COUNT> techniques{};
    slot_pool<rmaterial_info, MAX_MATERIAL_COUNT> materials{};
    slot_pool<rtexture_info, MAX_TEXTURE_COUNT> textures{};
    slot_pool<rmesh_info, MAX_MESH_COUNT> meshes{};

    // Frames in flight
    static_array<frame_context, MAX_FRAMES_IN_FLIGHT> fifs{};

    // Global descriptor set layouts (used for creating descriptor sets and pipelines)
    static_array<VkDescriptorSetLayout, RDESC_SET_LAYOUT_COUNT> set_layouts{};

    // Globabl geometry attribute buffers
    geometry_buffer_info geometry_buffers[RSTATIC_MESH_STREAM_COUNT];

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

rmesh_handle register_mesh(const rstatic_mesh &mdata, const char *name, renderer *rndr);

// NOTE: All of these mesh operations kind of need to wait on all rendering operations to complete as they modify the
// vertex and index buffers - not sure yet if this is better done within the functions or in the caller. Also these should be done at the
// start of a frame because any indices submitted in command buffers will be invalid after these operations. It almost seems like we should
// get a list of these and then just do it at start of frame after we wait for sync if there are any to do.

int init_renderer(renderer *rndr, void *win_hndl, mem_arena *fl_arena);

int begin_render_frame(renderer *rndr, int finished_frames);

int end_render_frame(renderer *rndr, camera *cam, f64 dt);

void terminate_renderer(renderer *rndr);

} // namespace nslib
