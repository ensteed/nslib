#pragma once

#include "math/matrix4.h"
#include "profile_timer.h"
#include "containers/slot_pool.h"
#include "vkr_context.h"
#include "render_blueprint.h"
#include "rformat.h"

struct ImGuiContext;

namespace nslib
{
struct camera;
struct transform;
struct rmanifest;
struct render_job_cb_params;
struct rsubgeom_range
{
    // Indice offset
    sizet offset;
    // Indice count
    sizet count;
};

struct rtexture_desc
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

struct rshader_stage_desc
{
    const void *src;
    sizet src_byte_size;
    const char *entry_point;
    rshader_stage_type stype;
};

struct rshader_desc
{
    const char *name;
    rshader_stage_desc *stages;
    u8 stage_cnt;
};

struct rbp_info
{
    render_blueprint_handle bp;
    rbp_pass_idx pid;
    u32 subpass_ind{0};
};

struct rblend_info
{
    rblend_factor src;
    rblend_factor dst;
    rblend_op op;
};

struct rattachment_blend_info
{
    b32 blend_enable;
    rblend_info color;
    rblend_info alpha;
    rcolor_component_flags write_mask;
};

enum rtechnique_desc_flag
{
    RTECHNIQUE_DESC_FLAG_PRIMITIVE_RESTART_ENABLED = RTECHNIQUE_FLAG_PRIMITIVE_RESTART_ENABLED,
    RTECHNIQUE_DESC_FLAG_CLAMP_DEPTH = RTECHNIQUE_FLAG_CLAMP_DEPTH,
    RTECHNIQUE_DESC_FLAG_DISCARD_RASTERIZER = RTECHNIQUE_FLAG_DISCARD_RASTERIZER,
    RTECHNIQUE_DESC_FLAG_SAMPLE_SHADING = RTECHNIQUE_FLAG_SAMPLE_SHADING,
    RTECHNIQUE_DESC_FLAG_MS_ALPHA_TO_COVERAGE = RTECHNIQUE_FLAG_MS_ALPHA_TO_COVERAGE,
    RTECHNIQUE_DESC_FLAG_MS_ALPHA_TO_ONE = RTECHNIQUE_FLAG_MS_ALPHA_TO_ONE,
    RTECHNIQUE_DESC_FLAG_DEPTH_TEST = RTECHNIQUE_FLAG_DEPTH_TEST,
    RTECHNIQUE_DESC_FLAG_DEPTH_WRITE = RTECHNIQUE_FLAG_DEPTH_WRITE,
    RTECHNIQUE_DESC_FLAG_DEPTH_BOUNDS_TEST = RTECHNIQUE_FLAG_DEPTH_BOUNDS_TEST,
    RTECHNIQUE_DESC_FLAG_DEPTH_BIAS = RTECHNIQUE_FLAG_DEPTH_BIAS,
    RTECHNIQUE_DESC_FLAG_BLEND_LOGIC_OP = RTECHNIQUE_FLAG_BLEND_LOGIC_OP,
};
using rtechnique_desc_flags = u32;

// TODO: Add depth bias to dynamic state along with viewports and scissor
struct rtechnique_pass_desc
{
    rgeom_topology topology;
    rpolygon_mode poly_mode;
    u32 tess_patch_control_points;
    // Depth op
    rcompare_op depth_compare_op;
    // Should leave as this unless doing something with depth bounds test turned on. Otherwise this isn't even looked at.
    vec2 depth_bounds{0.0f, 1.0f};
    rlogic_op logic_op;
    // Must match exactly the number of attachments in the blueprint or else will fail to create
    rattachment_blend_info atts_blending[MAX_FRAMEBUFFER_ATTACHMENT_COUNT];
    rtechnique_desc_flags tmask;
    rshader_handle shader;
    rbp_info bp_info{};
    geom_buffer_layout_idx geom_buffer_layout;
};

struct rtechnique_desc
{
    const rtechnique_pass_desc *passes;
    sizet pass_count;
    const char *name;
};

struct rmaterial_desc
{
    const char *name;
    rtechnique_handle thndl;
    rtexture_handle slots[RMATERIAL_TEXTURE_COUNT];
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

// We use a single vertex and indice buffer for all geometries
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

    // Indice range for each subgeometry
    static_array<rsubgeom_range, MAX_SUBGEOM_COUNT> subgeom_vert_ind_counts;
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

struct rshader_stage_info
{
    // Don't name your entry point longer than this
    small_str entry_point{};
    const VkSpecializationInfo *specialized_info{nullptr};
    VkShaderModule sm{VK_NULL_HANDLE};
};

struct rshader_info
{
    small_str name;
    rshader_stage_info stages[RSHADER_STAGE_TYPE_COUNT]{};
};

struct rtechnique_pass_entry
{
    rbp_pass_idx bp_pass;
    gpu_handle pline;
};

struct rtechnique_info
{
    small_str name;
    static_array<rtechnique_pass_entry, MAX_BP_PASS_COUNT> rpass_plines;
};

struct imgui_ctxt
{
    ImGuiContext *ctxt;
    VkDescriptorPool pool;
    VkRenderPass rpass;
    mem_arena fl;
};

struct frame_thread_cmd
{
    VkCommandPool pool;
    VkCommandBuffer buf;
};

struct frame_context
{
    // Updated at the start of each render frame once the image ind is acquired
    u32 cur_im_ind;

    // Frame cmd pool
    // TODO: There should really be a command pool for each frame, and a command buffer allocated for each draw set
    array<frame_thread_cmd> thread_pools;

    // Reset every frame
    VkDescriptorPool desc_pool;

    // Synchronization
    VkFence in_flight;
    VkSemaphore image_avail;

    // We use this as a debounce. A resize event set's the time to the debounce time, and we only actually resize the
    // swapchain once that is <= 0.0f
    f32 swapchain_resize;
};

struct stream_buffer_entry
{
    small_str name;
    vkr_buffer buffer;
};

struct geom_buffer_layout_entry
{
    vkr_vertex_layout vert_layout;
    VmaVirtualBlock vert_block;
    static_array<stream_buffer_entry, 16> vert_streams;
};

// Geometry stream groups all share the same indice buffer, so can all bound bound at the same time
struct geom_stream_group
{
    // The name is stored in the stream buffer entry for indices - the id is generated from that name
    rres_id id{INVALID_IND};
    static_array<geom_buffer_layout_entry, MAX_GEOMETRY_LAYOUT_COUNT> layouts{};
    VmaVirtualBlock indices_block{VK_NULL_HANDLE};
    stream_buffer_entry indice_stream;
};

struct rgeom_desc
{
    const char *name{};
    // Which geometry streams group
    u32 group{};
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
    // Topology
    rgeom_topology topology;
};

struct rview
{
    mat4 proj;
    mat4 cam;
    mat4 proj_cam;
};

// This will cause targets to resize dynamically with the swapchain as well
const svec2 WINDOW_SIZE = {};
const svec2 DEFAULT_SHADOW_MAP_SIZE = {2048, 2048};

struct rtexture_state
{
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkAccessFlags access{VK_ACCESS_NONE};
    VkPipelineStageFlags stage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
};

struct rbuffer_state
{
    VkAccessFlags access;
    VkPipelineStageFlags stage;
};

struct rbuffer_target_fif
{
    vkr_buffer buffer;
    rbuffer_state state;
    const vkr_buffer_cfg *cfg;
};

struct rbuffer_target
{
    // Set during build
    small_str name;
    rres_id id;
    vkr_buffer_cfg cfg{};
    // Filled in during compile
    rbuffer_target_fif frames[MAX_FRAMES_IN_FLIGHT];
};

struct rtexture_target_fif
{
    vkr_image image;
    VkImageView view;
    rtexture_state state;
    const vkr_image_cfg *cfg;
    const vkr_image_view_cfg *iv_cfg;
};

enum rtarget_texture_flag
{
    RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW = 1,
};

struct rtexture_target
{
    // Set during build
    small_str name;
    rres_id id;
    u32 flags;
    // We need to store this so we can resize the image
    vkr_image_cfg cfg{};
    vkr_image_view_cfg iv_cfg{};
    rtexture_target_fif frames[MAX_FRAMES_IN_FLIGHT];
};

struct rresource_target_registry
{
    slot_pool<rbuffer_target> buffers;
    hmap<rres_id, rbuffer_target_handle> buffer_id_map;
    slot_pool<rtexture_target> textures;
    hmap<rres_id, rtexture_target_handle> texture_id_map;
};

struct rbuffer_target_desc
{
    const char *name;
    vkr_buffer_cfg cfg{};
};

enum rtarget_texture_type
{
    RTARGET_TEXTURE_TYPE_COLOR,
    RTARGET_TEXTURE_TYPE_DEPTH,
    RTARGET_TEXTURE_TYPE_CUBE_COLOR,
    RTARGET_TEXTURE_TYPE_CUBE_DEPTH
};

struct rtexture_target_desc
{
    const char *name;
    rformat format;
    rtarget_texture_type type;
    svec2 dims;
    u32 flags;
};

#define TEXTURE_TARGET_COLOR_HDR(pname)                                                                                                    \
    {                                                                                                                                      \
        .name = pname,                                                                                                                     \
        .format = rformat::RGBA16_SFLOAT,                                                                                                  \
        .type = RTARGET_TEXTURE_TYPE_COLOR,                                                                                                \
        .dims = WINDOW_SIZE,                                                                                                               \
        .flags = RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW,                                                                                  \
    }

#define TEXTURE_TARGET_COLOR(pname)                                                                                                        \
    {                                                                                                                                      \
        .name = pname,                                                                                                                     \
        .format = rformat::RGBA8_SRGB,                                                                                                     \
        .type = RTARGET_TEXTURE_TYPE_COLOR,                                                                                                \
        .dims = WINDOW_SIZE,                                                                                                               \
        .flags = RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW,                                                                                  \
    }

#define TEXTURE_TARGET_DEPTH(pname)                                                                                                        \
    {                                                                                                                                      \
        .name = pname,                                                                                                                     \
        .format = rformat::D32_SFLOAT,                                                                                                     \
        .type = RTARGET_TEXTURE_TYPE_DEPTH,                                                                                                \
        .dims = WINDOW_SIZE,                                                                                                               \
        .flags = RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW,                                                                                  \
    }

#define TEXTURE_TARGET_SHADOW_MAP(pname)                                                                                                   \
    {                                                                                                                                      \
        .name = pname,                                                                                                                     \
        .format = rformat::D32_SFLOAT,                                                                                                     \
        .type = RTARGET_TEXTURE_TYPE_DEPTH,                                                                                                \
        .dims = DEFAULT_SHADOW_MAP_SIZE,                                                                                                   \
    }

// int init_render_blueprint(render_blueprint *bp);
// void terminate_render_blueprint(render_blueprint *bp);

// rbp_pass *create_pass(render_blueprint *bp, const char *pass_t);
// void add_subpass(rbp_pass *pass);
template<typename T>
struct gpu_resource_entry
{
    T gpu_d;
    u64 key;
};

template<typename T>
struct gpu_resource_cache
{
    hmap<u64, slot_handle<gpu_resource_entry<T>>> key_lut;
    slot_pool<gpu_resource_entry<T>> items;
};

using framebuffer_entry = gpu_resource_entry<vkr_framebuffer>;
using framebuffer_handle = slot_handle<framebuffer_entry>;
using framebuffer_cache = gpu_resource_cache<vkr_framebuffer>;

using pipeline_entry = gpu_resource_entry<gpu_handle>;
using pipeline_handle = slot_handle<pipeline_entry>;
using pipeline_cache = gpu_resource_cache<gpu_handle>;

struct renderer
{
    // Owned vulkan context and mem arenas used only for vulkan stuff
    vkr_context vk{};
    mem_arena vk_free_list;
    mem_arena vk_frame_linear;

    mem_arena persist_fl;
    mem_arena persist_stack;
    mem_arena frame_linear;

    // Renderer resources
    // TODO: Implement this for smarter pipeline creation
    pipeline_cache pline_cache;
    framebuffer_cache fb_cache;

    slot_pool<rshader_info> shaders{};
    slot_pool<rtechnique_info> techniques{};
    slot_pool<rmaterial_info> materials{};
    slot_pool<rtexture_info> textures{};
    slot_pool<rgeom_info> geometry{};

    // Frames in flight
    static_array<frame_context, MAX_FRAMES_IN_FLIGHT> fifs{};

    // Global descriptor set layouts (used for creating descriptor sets and pipelines)
    static_array<VkDescriptorSetLayout, RDESC_SET_LAYOUT_COUNT> set_layouts{};

    // Really a single
    hmap<rres_id, geom_stream_group_idx> geom_group_id_map{};
    static_array<geom_stream_group, MAX_GEOMETRY_STREAM_GROUP_COUNT> geom_groups;

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
    s32 finished_frames{0};

    // This is incremented every frame there are no resize events
    f64 no_resize_frames;

    // Render blueprints
    hmap<rres_id, render_blueprint_handle> blueprint_id_map{};
    slot_pool<render_blueprint> blueprints{};

    rresource_target_registry rtargets{};

    profile_timepoints pt{};
};

struct init_renderer_params
{
    void *win_hndl;
    mem_arena *upsream;
    sizet thread_count{4};
    sizet persist_fl_size;
    sizet persist_stack_size;
    sizet frame_linear_size;
};

// DRAW FUNCTIONS
#if defined USE_IMGUI
void draw_imgui(const render_job_cb_params &, void *);
#endif
void draw_geometry(const render_job_cb_params &, void *);

int init_renderer(renderer *rndr, const init_renderer_params &p);
void terminate_renderer(renderer *rndr);

void init_imgui(renderer *rndr, const rbp_pass &pass);
void terminate_imgui(renderer *rndr);

geom_stream_group_idx push_geometry_stream_group(renderer *rndr, const geometry_stream_group_desc &desc);
geom_stream_group_idx find_geometry_stream_group(renderer *rndr, rres_id group_id);

// Geometry group desc builder
geometry_vert_layout_desc *push_geometry_layout(geometry_stream_group_desc *desc, u32 layout_max_vert_count);
vert_stream_desc *push_geometry_stream(geometry_vert_layout_desc *vert_layout, const char *dbg_name);
void push_geometry_attribute(vert_stream_desc *stream, const vert_attrib_desc &att_desc);

template<typename T>
void push_geometry_attribute(vert_stream_desc *stream, u32 shader_location)
{
    push_geometry_attribute(stream, {.shader_location = shader_location, .fmt = get_rformat_for_type<T>()});
}

template<typename T>
void push_geometry_attribute(vert_stream_desc *stream, u32 shader_location, bool normalize_in_shader)
{
    push_geometry_attribute(stream, {.shader_location = shader_location, .fmt = get_rformat_for_type<T>(normalize_in_shader)});
}

rformat get_swapchain_format(renderer *rnd);

rgeom_handle create_rgeometry(renderer *rndr, const rgeom_desc &ci);
rtexture_handle create_rtexture(renderer *rndr, const rtexture_desc &ctinfo);
rshader_handle create_rshader(renderer *rndr, const rshader_desc &sdr_info);
rtechnique_handle create_rtechnique(renderer *rndr, const rtechnique_desc &ctinfo);
rmaterial_handle create_rmaterial(renderer *rndr, const rmaterial_desc &ctinfo);

rtexture_target_handle create_rtexture_target(renderer *rndr, const rtexture_target_desc &ci);
rtexture_target *get_rtexture_target(renderer *rndr, rtexture_target_handle hndl);
rtexture_target_handle find_rtexture_target(renderer *rndr, rres_id id);

rbuffer_target_handle create_rbuffer_target(renderer *rndr, const rbuffer_target_desc &ci);
rbuffer_target *get_rbuffer_target(renderer *rndr, rbuffer_target_handle hndl);
rbuffer_target_handle find_rbuffer_target(renderer *rndr, rres_id id);

rmanifest *begin_render_frame(renderer *rndr, render_blueprint_handle bp);
bool end_render_frame(rmanifest *m);

} // namespace nslib
