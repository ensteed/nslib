

#include "math/matrix4.h"
#include "profile_timer.h"
#include "containers/slot_pool.h"
#include "vkr_context.h"
#include "render_blueprint.h"
#include "render_manifest.h"
#include "rformat.h"
#include "rtexture_registry.h"
#include "vkr_chunked_buffer.h"

struct ImGuiContext;

namespace nslib
{

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

using rframebuffer_entry = gpu_resource_entry<vkr_framebuffer>;
using rframebuffer_handle = slot_handle<rframebuffer_entry>;
using rframebuffer_cache = gpu_resource_cache<vkr_framebuffer>;

using rpipeline_entry = gpu_resource_entry<gpu_handle>;
using rpipeline_handle = slot_handle<rpipeline_entry>;
using rpipeline_cache = gpu_resource_cache<gpu_handle>;

struct camera;
struct transform;
struct rsubgeom_range
{
    // Indice offset
    u32 offset;
    // Indice count
    u32 count;
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
    // Pass ind
    idx_t pid;
    // Subpass ind
    idx_t spi{0};
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
    sizet att_count;
    rtechnique_desc_flags tmask;
    rshader_handle shader;
    rbp_info bp_info{};
    idx_t geom_buffer_layout;
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
    rtechnique_handle tch_rhndl;
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
};
}

// We use a single vertex and indice buffer for all geometries
struct rgeom_info
{
    // Attached to the vert_mem allocation
    small_str name;

    // Mem for stream 0 of the vert layout
    VmaVirtualAllocation vert_mem{VK_NULL_HANDLE};

    // This is the block that was used for the virtual block allocation above
    VmaVirtualBlock vert_block{VK_NULL_HANDLE};

    // This is determined by taking the byte offset / sizeof(stream element)
    u32 vert_offset;

    // Mem for index stream
    VmaVirtualAllocation ind_mem{VK_NULL_HANDLE};

    // This is the block that was used for the ind virtual block allocations
    VmaVirtualBlock ind_block{VK_NULL_HANDLE};

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

struct rmaterial_info
{};

struct rshader_stage_info
{
    // Don't name your entry point longer than this
    small_str entry_point{};
    const VkSpecializationInfo *specialized_info{nullptr};
    VkShaderModule sm{0};
};

struct rshader_info
{
    small_str name;
    rshader_stage_info stages[RSHADER_STAGE_TYPE_COUNT]{};
};

struct rtechnique_pass_entry
{
    idx_t subpass;
    idx_t bp_pass;
    rpipeline_handle pline;
    rpline_dyn_state dstate;
};

struct rtechnique_info
{
    small_str name;
    static_array<rtechnique_pass_entry, MAX_BP_PASS_COUNT> rpass_plines;
};

struct imgui_ctxt
{
    ImGuiContext *ctxt;
    VkDescriptorPool pool{VK_NULL_HANDLE};
    VkRenderPass rpass{VK_NULL_HANDLE};
    mem_arena fl;
};

struct frame_thread_cmd
{
    VkCommandPool pool{VK_NULL_HANDLE};
    VkCommandBuffer buf{VK_NULL_HANDLE};
};

struct frame_context
{
    // Updated at the start of each render frame once the image ind is acquired
    // The only reason to keep it per frame is for debug reasons we can see what the im was last frame
    u32 cur_im_ind;

    // Frame cmd pool
    // TODO: There should really be a command pool for each frame, and a command buffer allocated for each draw set
    array<frame_thread_cmd> thread_pools;

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
    static_array<stream_buffer_entry, MAX_VERT_BINDINGS> vert_streams;
};

// Geometry stream groups all share the same indice buffer, so can all bound bound at the same time
struct geom_stream_group
{
    // The name is stored in the stream buffer entry for indices - the id is generated from that name
    rres_id id{INVALID_ID};
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
        .format = RFMT_RGBA16_SFLOAT,                                                                                                  \
        .type = RTARGET_TEXTURE_TYPE_COLOR,                                                                                                \
        .dims = WINDOW_SIZE,                                                                                                               \
        .flags = RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW,                                                                                  \
    }

#define TEXTURE_TARGET_COLOR(pname)                                                                                                        \
    {                                                                                                                                      \
        .name = pname,                                                                                                                     \
        .format = RFMT_RGBA8_SRGB,                                                                                                     \
        .type = RTARGET_TEXTURE_TYPE_COLOR,                                                                                                \
        .dims = WINDOW_SIZE,                                                                                                               \
        .flags = RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW,                                                                                  \
    }

#define TEXTURE_TARGET_DEPTH(pname)                                                                                                        \
    {                                                                                                                                      \
        .name = pname,                                                                                                                     \
        .format = RFMT_D32_SFLOAT,                                                                                                     \
        .type = RTARGET_TEXTURE_TYPE_DEPTH,                                                                                                \
        .dims = WINDOW_SIZE,                                                                                                               \
        .flags = RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW,                                                                                  \
    }

#define TEXTURE_TARGET_SHADOW_MAP(pname)                                                                                                   \
    {                                                                                                                                      \
        .name = pname,                                                                                                                     \
        .format = RFMT_D32_SFLOAT,                                                                                                     \
        .type = RTARGET_TEXTURE_TYPE_DEPTH,                                                                                                \
        .dims = DEFAULT_SHADOW_MAP_SIZE,                                                                                                   \
    }

// int init_render_blueprint(render_blueprint *bp);
// void terminate_render_blueprint(render_blueprint *bp);

// rbp_pass *create_pass(render_blueprint *bp, const char *pass_t);
// void add_subpass(rbp_pass *pass);

struct desc_block_buffer {
    vkr_buffer buffer;
    u32 fif_block_count;
    sizet block_size;
};

struct global_descriptor_info
{
    VkDescriptorSetLayout dset_layouts[RDSET_LAYOUT_COUNT];
    // instance/material/frame/draw set have per fif, image has just one desc set - a total of max fif + 1
    VkDescriptorSet main_data[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet images;
    VkPipelineLayout pline_layout;
    VkDescriptorPool pool;
    
    //////////////////////////////////////////////////////
    // For all buffers, each fif has its own subsection //
    //////////////////////////////////////////////////////
    // Filled every frame with per instance draw call data of the callers choosing
    desc_block_buffer draw_ssbo;
    // Filled every frame with per view data of user's choosing
    desc_block_buffer view_ssbo;
    // Filled every frame with per pass data of user's choosing
    desc_block_buffer pass_ssbo;
    // Per frame data
    desc_block_buffer frame_ubo;
    // All instance data
    desc_block_buffer instance_ssbo;
    // All material data
    vkr_chunked_buffer material_ssbo;
};

using rshader_pool = slot_pool<rshader_info>;
using rtechnique_pool = slot_pool<rtechnique_info>;
using rmaterial_pool = slot_pool<rmaterial_info>;
using rgeometry_pool = slot_pool<rgeom_info>;

struct renderer
{
    // Owned vulkan context and mem arenas used only for vulkan stuff
    vkr_context vk{};
    mem_arena vk_free_list;
    mem_arena vk_frame_linear;

    mem_arena persist_fl;
    mem_arena scratch_stack;
    mem_arena frame_linear;

    // Renderer resources
    // TODO: Implement this for smarter pipeline creation
    rpipeline_cache pline_cache;
    rframebuffer_cache fb_cache;

    rshader_pool shaders;
    rtechnique_pool techniques;
    rmaterial_pool materials;
    rgeometry_pool geometry;

    // Specialized registry for textures
    rtexture_registry textures;

    // Frames in flight
    static_array<frame_context, MAX_FRAMES_IN_FLIGHT> fifs{};

    // Global descriptor set layouts (used for creating descriptor sets and pipelines)
    global_descriptor_info desc_info{};

    // Really a single
    hmap<rres_id, idx_t> geom_group_id_map{};
    static_array<geom_stream_group, MAX_GEOMETRY_STREAM_GROUP_COUNT> geom_groups;

    // Transient pool for image transfers and such
    VkCommandPool transient_pool;

    // Global texture samplers
    VkSampler samplers[RSAMPLER_TYPE_COUNT];

// ImGUI context
#ifdef USE_IMGUI
    imgui_ctxt imgui{};
#endif

    // Stored on reset render frame - used in subsequent frame calls to get the current frame
    s32 finished_frames{0};

    // Render blueprints
    hmap<rres_id, render_blueprint_handle> blueprint_id_map{};
    slot_pool<render_blueprint> blueprints{};

    rresource_target_registry rtargets{};

    profile_timepoints pt{};
};

struct sbuffer_cfg
{
    u32 block_count;
    sizet block_size;
};

struct push_constant_range {
    u32 offset;
    u32 size;
    rshader_stage_flags stages;
};

struct rpipeline_layout_cfg
{
    sizet view_ssbo_block_sz;
    sizet pass_ssbo_block_sz;
    sizet frame_ubo_block_sz;
    sbuffer_cfg instance_ssbo;
    sbuffer_cfg material_ssbo;
    u32 push_const_range_count;
    const push_constant_range *push_const_ranges;
};

struct renderer_cfg
{
    void *win_hndl;
    mem_arena *upsream;
    sizet thread_count{4};
    sizet persist_fl_size;
    sizet scratch_stack_size;
    sizet extra_frame_linear_size;
    rpipeline_layout_cfg desc;
    manifest_max_counts mcounts;
    u32 texture_pool_count;
    const rtexture_pool_cfg *texture_pool_cfgs;
};

constexpr sizet calculate_render_job_needed_capacity(u32 draw_call_count, sizet draw_ssbo_block_size)
{
    // draw call data + sorted index + tmp sorted index, alloc headers for these 3 allocations plus the actual SSBO data and allocation
    // header for each draw call
    sizet ssbo_block_alloc_sz = sizeof(alloc_header) + draw_ssbo_block_size;
    return draw_call_count * (sizeof(mdraw_call) + sizeof(u32)*2 + ssbo_block_alloc_sz) + sizeof(alloc_header) * 3;
}

constexpr sizet calculate_manifest_approximate_needed_capacity(const manifest_max_counts &max_counts, sizet draw_ssbo_block_sz)
{
    sizet pass_sz = max_counts.passes * sizeof(mpass);
    sizet view_sz = max_counts.views * sizeof(mview);
    sizet job_sz = max_counts.render_jobs * calculate_render_job_needed_capacity(max_counts.draw_calls_per_job, draw_ssbo_block_sz);
    sizet buffer_targets = max_counts.buffer_targets * sizeof(rbuffer_target);
    sizet texture_targets = max_counts.texture_targets * sizeof(rtexture_target);
    return sizeof(rmanifest) + pass_sz + view_sz + job_sz + buffer_targets + texture_targets;
}


void init_imgui(renderer *rndr, const rbp_pass &pass);
void terminate_imgui(renderer *rndr);
void handle_window_resize(renderer *rndr);
rformat get_swapchain_format(renderer *rnd);
idx_t get_fif_ind(renderer *rndr);

idx_t push_geometry_stream_group(renderer *rndr, const geometry_stream_group_desc &desc);
idx_t find_geometry_stream_group(renderer *rndr, rres_id group_id);

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

rgeom_handle create_rgeometry(renderer *rndr, const rgeom_desc &ci);
rtexture_handle create_rtexture(renderer *rndr, const rtexture_desc &ctinfo);
rshader_handle create_rshader(renderer *rndr, const rshader_desc &sdr_info);
rtechnique_handle create_rtechnique(renderer *rndr, const rtechnique_desc &tdesc);
rmaterial_handle create_rmaterial(renderer *rndr, const rmaterial_desc &ctinfo);

void update_view_data(rmanifest *m, idx_t view, const void *view_data);
void update_instance_data(rmanifest* m, idx_t inst, const void* instance_data);
void post_material_update(renderer* rndr, u32 material_idx, const void* material_data);

rtexture_target_handle create_rtexture_target(renderer *rndr, const rtexture_target_desc &ci);
rtexture_target *get_rtexture_target(renderer *rndr, rtexture_target_handle hndl);
rtexture_target_handle find_rtexture_target(renderer *rndr, rres_id id);

rbuffer_target_handle create_rbuffer_target(renderer *rndr, const rbuffer_target_desc &ci);
rbuffer_target *get_rbuffer_target(renderer *rndr, rbuffer_target_handle hndl);
rbuffer_target_handle find_rbuffer_target(renderer *rndr, rres_id id);

bool init_renderer(renderer *rndr, const renderer_cfg &p);
void terminate_renderer(renderer *rndr);


} // namespace nslib
