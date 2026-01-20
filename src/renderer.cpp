#include "platform.h"
#include "vk_context.h"
#include "renderer.h"
#include "sim_region.h"

#ifdef USE_IMGUI
    #include "imgui/imgui.h"
    #include "imgui/imgui_impl_sdl3.h"
    #include "imgui/imgui_impl_vulkan.h"
    #include "SDL3/SDL_events.h"
#endif

namespace nslib
{

#if defined(NDEBUG)
intern const u32 VALIDATION_LAYER_COUNT = 0;
intern const char **VALIDATION_LAYERS = nullptr;
#else
intern const u32 VALIDATION_LAYER_COUNT = 1;
intern const char *VALIDATION_LAYERS[VALIDATION_LAYER_COUNT] = {"VK_LAYER_KHRONOS_validation"};
#endif

#if __APPLE__
intern const VkInstanceCreateFlags INST_CREATE_FLAGS = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 2;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                                                                  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
intern const u32 DEVICE_EXTENSION_COUNT = 2;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
#else
intern constexpr VkInstanceCreateFlags INST_CREATE_FLAGS = {};
intern constexpr u32 ADDITIONAL_INST_EXTENSION_COUNT = 1;
intern constexpr const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
intern constexpr u32 DEVICE_EXTENSION_COUNT = 1;
intern constexpr const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif

intern constexpr f64 RESIZE_DEBOUNCE_FRAME_COUNT = 0.15; // 100 ms
intern VkPipelineLayout G_FRAME_PL_LAYOUT{};

intern void imgui_mem_free(void *ptr, void *usr)
{
    mem_free(ptr, (mem_arena *)usr);
}

intern void *imgui_mem_alloc(sizet sz, void *usr)
{
    return mem_alloc(sz, (mem_arena *)usr, SIMD_MIN_ALIGNMENT);
}

intern void check_vk_result(VkResult err)
{
    if (err != VK_SUCCESS) wlog("vulkan err: %d", err);
    asrt(err >= 0);
}

#ifdef USE_IMGUI
bool sdl_event_func(void *sdl_event, void *)
{
    auto *ev = (SDL_Event *)sdl_event;

    auto io = ImGui::GetIO();
    ImGui_ImplSDL3_ProcessEvent(ev);

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL) {
        return io.WantCaptureMouse;
    }
    // NOTE: We might uncomment this in the future but for now we don't need to capture keyboard..
    if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP) {
        return io.WantCaptureKeyboard;
    }
    return false;
}
intern void init_imgui(renderer *rndr, void *win_hndl)
{
    auto dev = &rndr->vk.inst.device;
    // 263 KB seems to be about the min required - we'll give it a MB
    init_fl_arena(&rndr->imgui.fl, MB_SIZE, &rndr->persist_fl, "imgui");

    // Use the main forward pass for imgui.. this might only change if we use deferred shading.. but i think the imgui
    // created pipeling only requires a color attachment
    // rndr->imgui.rpass = rndr->rpasses[RPASS_TYPE_OPAQUE].vk_hndl;

    ImGui::SetAllocatorFunctions(imgui_mem_alloc, imgui_mem_free, &rndr->imgui.fl);
    rndr->imgui.ctxt = ImGui::CreateContext();
    ImGui::StyleColorsDark();
    auto &io = ImGui::GetIO();
    io.FontGlobalScale = get_window_display_scale(win_hndl);

    vkr_desc_cfg cfg{};
    cfg.max_sets = 1;
    cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
    cfg.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    if (vkr_init_desc_pool(&rndr->imgui.pool, cfg, &rndr->vk) != err_code::VKR_NO_ERROR) {
        wlog("Could not create imgui descriptor pool");
    }

    ImGui_ImplSDL3_InitForVulkan((SDL_Window *)rndr->vk.cfg.window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VKR_API_VERSION;
    init_info.Instance = rndr->vk.inst.hndl;
    init_info.PhysicalDevice = rndr->vk.inst.pdev_info.hndl;
    init_info.Device = rndr->vk.inst.device.hndl;
    init_info.QueueFamily = rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].fam_ind;
    init_info.Queue = rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE];
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = rndr->imgui.pool;
    init_info.Allocator = &rndr->vk.alloc_cbs;
    init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.ImageCount = rndr->vk.inst.device.swapchain.images.size;
    init_info.RenderPass = rndr->imgui.rpass;
    init_info.Subpass = 0;
    init_info.CheckVkResultFn = check_vk_result;
    // init_info.MSAASamples
    ImGui_ImplVulkan_Init(&init_info);

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        wlog("Could not create imgui vulkan font texture");
    }

    set_platform_sdl_event_hook(win_hndl, {.cb = sdl_event_func});
}
intern void terminate_imgui(renderer *rndr)
{
    ImGui_ImplVulkan_Shutdown();
    vkr_terminate_desc_pool(rndr->imgui.pool, &rndr->vk);
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(rndr->imgui.ctxt);
    terminate_arena(&rndr->imgui.fl);
}
#endif

intern int setup_render_passes(renderer *rndr)
{
    auto vk = &rndr->vk;
    vkr_rpass_cfg rp_cfg{};
    VkAttachmentDescription att{};
    att.format = vk->inst.device.swapchain.format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    arr_push_back(&rp_cfg.attachments, att);

    att.format = vkr_find_best_depth_format(&rndr->vk.inst.pdev_info);
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    arr_push_back(&rp_cfg.attachments, att);

    vkr_rpass_cfg_subpass subpass{};

    VkAttachmentReference att_ref{};
    att_ref.attachment = 0;
    att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    arr_push_back(&subpass.color_attachments, att_ref);

    att_ref.attachment = 1;
    att_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    subpass.depth_stencil_attachment = att_ref;
    arr_push_back(&rp_cfg.subpasses, subpass);

    // Because we use this render pass each frame, this dependency makes it so that we won't begin our first subpass
    // until all color attachment and depth attachment (early fragment tests) operations are done for any subpass
    // (pipeline) associated with this render pass.
    // Since the depth image isn't 'presented', we don't have to have a separate depth image across each FIF.
    VkSubpassDependency sp_dep{};
    sp_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    sp_dep.dstSubpass = 0;
    sp_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    sp_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    sp_dep.srcAccessMask = 0;
    sp_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    arr_push_back(&rp_cfg.subpass_dependencies, sp_dep);

    sp_dep.srcSubpass = 0;
    sp_dep.dstSubpass = VK_SUBPASS_EXTERNAL;
    sp_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    sp_dep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    sp_dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    sp_dep.dstAccessMask = 0;
    arr_push_back(&rp_cfg.subpass_dependencies, sp_dep);

    rpass_info rpi{};
    int geom_hndl = vkr_init_render_pass(&rpi.vk_hndl, rp_cfg, vk);
    if (geom_hndl == err_code::VKR_NO_ERROR) {
        arr_push_back(&rndr->rpasses, rpi);
    }
    return geom_hndl;
}

intern void fill_default_pipeline_config(vkr_pipeline_cfg *cfg, renderer *rndr)
{

    arr_push_back(&cfg->dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    arr_push_back(&cfg->dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

    // Must be set to 1 at least
    cfg->viewports.size = 1;
    cfg->scissors.size = 1;

    // Input Assembly
    cfg->input_assembly.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    cfg->input_assembly.primitive_restart_enable = false;

    // Raster options
    cfg->raster.depth_clamp_enable = false;
    cfg->raster.rasterizer_discard_enable = false;
    cfg->raster.polygon_mode = VK_POLYGON_MODE_FILL;
    cfg->raster.line_width = 1.0f;
    cfg->raster.cull_mode = VK_CULL_MODE_NONE;
    cfg->raster.front_face = VK_FRONT_FACE_CLOCKWISE;
    cfg->raster.depth_bias_enable = false;
    cfg->raster.depth_bias_constant_factor = 0.0f;
    cfg->raster.depth_bias_clamp = 0.0f;
    cfg->raster.depth_bias_slope_factor = 0.0f;

    // Multisampling defaults are good

    // Color blending - none for this pipeline
    VkPipelineColorBlendAttachmentState col_blnd_att{};
    col_blnd_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    col_blnd_att.blendEnable = false;
    arr_push_back(&cfg->col_blend.attachments, col_blnd_att);

    // Depth Stencil
    cfg->depth_stencil.depth_test_enable = true;
    cfg->depth_stencil.depth_write_enable = true;
    cfg->depth_stencil.depth_compare_op = VK_COMPARE_OP_LESS;
    cfg->depth_stencil.depth_bounds_test_enable = false;
    cfg->depth_stencil.min_depth_bounds = 0.0f;
    cfg->depth_stencil.max_depth_bounds = 1.0f;

    // Global layout for desc sets
    cfg->layout_hndl = rndr->g_layout;
}

intern int setup_diffuse_technique(renderer *rndr)
{
#if 0
    auto vk = &rndr->vk;
    vkr_pipeline_cfg info{};

    fill_default_pipeline_config(&info, rndr);
    info.vert_desc = rndr->vertex_layouts[RVERT_LAYOUT_STATIC_MESH];
    info.rpass = rndr->rpasses[RPASS_TYPE_OPAQUE].vk_hndl;

    // Our basic shaders
    const char *fnames[] = {"data/shaders/fwd-diffuse.vert.spv", "data/shaders/fwd-diffuse.frag.spv"};
    for (int i = 0; i <= VKR_SHADER_STAGE_FRAG; ++i) {
        platform_file_err_desc err{};
        arr_init(&info.shader_stages[i].code, &rndr->vk_frame_linear);
        read_file(fnames[i], &info.shader_stages[i].code, 0, &err);
        if (err.code != err_code::PLATFORM_NO_ERROR) {
            wlog("Error reading file %s from disk (code %d): %s", fnames[i], err.code, err.str);
            return err_code::RENDER_LOAD_SHADERS_FAIL;
        }
        info.shader_stages[i].entry_point = "main";
    }

    VkPipeline pl{};
    int code = vkr_init_pipeline(&pl, info, vk);
    if (code != err_code::VKR_NO_ERROR) {
        wlog("Failed to initialize pipeline with code %d", code);
        return code;
    }

    // TEMP: This is just a dummy id for the pipeline for now - this eventually needs to be a hash of things unique from
    // the technique/pass that needed it
    hmap_set(&rndr->pline_cache, (u64)123, pl);

    rndr->default_technique = acquire_slot(&rndr->techniques);
    auto item = get_slot_item(&rndr->techniques, rndr->default_technique);
    item->rpass_plines[RPASS_TYPE_OPAQUE] = pl;

    rndr->default_mat = acquire_slot(&rndr->materials);

    auto mat_item = get_slot_item(&rndr->materials, rndr->default_mat);
#endif
    return err_code::RENDER_NO_ERROR;
}

intern bool destroy_geometry(rgeom_handle hndl, renderer *rndr)
{
    auto sl_item = get_slot_item(&rndr->geometry, hndl);
    vmaVirtualFree(sl_item->vert_block, sl_item->vert_mem);
    vmaVirtualFree(sl_item->ind_block, sl_item->ind_mem);
    *sl_item = {};
    return release_slot(&rndr->geometry, hndl);
}

intern int setup_global_samplers(renderer *rndr)
{
    auto dev = &rndr->vk.inst.device;
    rndr->samplers.size = RSAMPLER_TYPE_COUNT;

    // Create image sampler
    vkr_sampler_cfg samp_cfg{};
    for (int i = 0; i < 3; ++i) {
        samp_cfg.address_mode_uvw[i] = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
    samp_cfg.mag_filter = VK_FILTER_LINEAR;
    samp_cfg.min_filter = VK_FILTER_LINEAR;
    samp_cfg.anisotropy_enable = true;
    samp_cfg.max_anisotropy = rndr->vk.inst.pdev_info.props.limits.maxSamplerAnisotropy;
    samp_cfg.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samp_cfg.compare_op = VK_COMPARE_OP_ALWAYS;
    samp_cfg.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    rsampler_info sdata{};
    int err = vkr_init_sampler(&sdata.vk_hndl, samp_cfg, &rndr->vk);
    if (err != err_code::VKR_NO_ERROR) {
        wlog("Failed to initialize sampler - vk err code: %d", err);
        return err_code::RENDER_INIT_SAMPLER_FAIL;
    }
    rndr->samplers[RSAMPLER_TYPE_LINEAR_REPEAT] = sdata;
    return err_code::RENDER_NO_ERROR;
}

intern VkFormat get_vk_format(rformat fmt)
{
    switch (fmt) {
    case (rformat::RGBA8_SRGB):
        return VK_FORMAT_R8G8B8A8_SRGB;
    case (rformat::RGBA8_SRGB_COMPRESSED):
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case (rformat::RGBA8_UNORM):
        return VK_FORMAT_R8G8B8A8_UNORM;
    case (rformat::RGBA8_UNORM_COMPRESSED):
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case (rformat::RGBA8_SNORM):
        return VK_FORMAT_R8G8B8A8_SNORM;
    case (rformat::RGBA8_UINT):
        return VK_FORMAT_R8G8B8A8_UINT;
    case (rformat::RGBA8_SINT):
        return VK_FORMAT_R8G8B8A8_SINT;
    case (rformat::RGB8_SRGB):
        return VK_FORMAT_R8G8B8_SRGB;
    case (rformat::RGB8_SRGB_COMPRESSED):
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    case (rformat::RGB8_UNORM):
        return VK_FORMAT_R8G8B8_UNORM;
    case (rformat::RGB8_UNORM_COMPRESSED):
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case (rformat::RGB8_SNORM):
        return VK_FORMAT_R8G8B8_SNORM;
    case (rformat::RGB8_UINT):
        return VK_FORMAT_R8G8B8_UINT;
    case (rformat::RGB8_SINT):
        return VK_FORMAT_R8G8B8_SINT;
    case (rformat::RG8_SRGB):
        return VK_FORMAT_R8G8_SRGB;
    case (rformat::RG8_UNORM):
        return VK_FORMAT_R8G8_UNORM;
    case (rformat::RG8_UNORM_COMPRESSED):
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case (rformat::RG8_SNORM):
        return VK_FORMAT_R8G8_SNORM;
    case (rformat::RG8_SNORM_COMPRESSED):
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case (rformat::RG8_UINT):
        return VK_FORMAT_R8G8_UINT;
    case (rformat::RG8_SINT):
        return VK_FORMAT_R8G8_SINT;
    case (rformat::R8_SRGB):
        return VK_FORMAT_R8_SRGB;
    case (rformat::R8_UNORM):
        return VK_FORMAT_R8_UNORM;
    case (rformat::R8_UNORM_COMPRESSED):
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case (rformat::R8_SNORM):
        return VK_FORMAT_R8_SNORM;
    case (rformat::R8_SNORM_COMPRESSED):
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case (rformat::R8_UINT):
        return VK_FORMAT_R8_UINT;
    case (rformat::R8_SINT):
        return VK_FORMAT_R8_SINT;
    case (rformat::RGBA16_SFLOAT):
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case (rformat::RGBA16_UNORM):
        return VK_FORMAT_R16G16B16A16_UNORM;
    case (rformat::RGBA16_SNORM):
        return VK_FORMAT_R16G16B16A16_SNORM;
    case (rformat::RGBA16_UINT):
        return VK_FORMAT_R16G16B16A16_UINT;
    case (rformat::RGBA16_SINT):
        return VK_FORMAT_R16G16B16A16_SINT;
    case (rformat::RGB16_SFLOAT):
        return VK_FORMAT_R16G16B16_SFLOAT;
    case (rformat::RGB16_UNORM):
        return VK_FORMAT_R16G16B16_UNORM;
    case (rformat::RGB16_SNORM):
        return VK_FORMAT_R16G16B16_SNORM;
    case (rformat::RGB16_UINT):
        return VK_FORMAT_R16G16B16_UINT;
    case (rformat::RGB16_SINT):
        return VK_FORMAT_R16G16B16_SINT;
    case (rformat::RG16_SFLOAT):
        return VK_FORMAT_R16G16_SFLOAT;
    case (rformat::RG16_UNORM):
        return VK_FORMAT_R16G16_UNORM;
    case (rformat::RG16_SNORM):
        return VK_FORMAT_R16G16_SNORM;
    case (rformat::RG16_UINT):
        return VK_FORMAT_R16G16_UINT;
    case (rformat::RG16_SINT):
        return VK_FORMAT_R16G16_SINT;
    case (rformat::R16_SFLOAT):
        return VK_FORMAT_R16_SFLOAT;
    case (rformat::R16_UNORM):
        return VK_FORMAT_R16_UNORM;
    case (rformat::R16_SNORM):
        return VK_FORMAT_R16_SNORM;
    case (rformat::R16_UINT):
        return VK_FORMAT_R16_UINT;
    case (rformat::R16_SINT):
        return VK_FORMAT_R16_SINT;
    case (rformat::RGBA32_SFLOAT):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case (rformat::RGBA32_UINT):
        return VK_FORMAT_R32G32B32A32_UINT;
    case (rformat::RGBA32_SINT):
        return VK_FORMAT_R32G32B32A32_SINT;
    case (rformat::RGB32_SFLOAT):
        return VK_FORMAT_R32G32B32_SFLOAT;
    case (rformat::RGB32_UINT):
        return VK_FORMAT_R32G32B32_UINT;
    case (rformat::RGB32_SINT):
        return VK_FORMAT_R32G32B32_SINT;
    case (rformat::RG32_SFLOAT):
        return VK_FORMAT_R32G32_SFLOAT;
    case (rformat::RG32_UINT):
        return VK_FORMAT_R32G32_UINT;
    case (rformat::RG32_SINT):
        return VK_FORMAT_R32G32_SINT;
    case (rformat::R32_SFLOAT):
        return VK_FORMAT_R32_SFLOAT;
    case (rformat::R32_UINT):
        return VK_FORMAT_R32_UINT;
    case (rformat::R32_SINT):
        return VK_FORMAT_R32_SINT;
    case (rformat::RGBA64_SFLOAT):
        return VK_FORMAT_R64G64B64A64_SFLOAT;
    case (rformat::RGBA64_UINT):
        return VK_FORMAT_R64G64B64A64_UINT;
    case (rformat::RGBA64_SINT):
        return VK_FORMAT_R64G64B64A64_SINT;
    case (rformat::RGB64_SFLOAT):
        return VK_FORMAT_R64G64B64_SFLOAT;
    case (rformat::RGB64_UINT):
        return VK_FORMAT_R64G64B64_UINT;
    case (rformat::RGB64_SINT):
        return VK_FORMAT_R64G64B64_SINT;
    case (rformat::RG64_SFLOAT):
        return VK_FORMAT_R64G64_SFLOAT;
    case (rformat::RG64_UINT):
        return VK_FORMAT_R64G64_UINT;
    case (rformat::RG64_SINT):
        return VK_FORMAT_R64G64_SINT;
    case (rformat::R64_SFLOAT):
        return VK_FORMAT_R64_SFLOAT;
    case (rformat::R64_UINT):
        return VK_FORMAT_R64_UINT;
    case (rformat::R64_SINT):
        return VK_FORMAT_R64_SINT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

rtexture_handle create_texture(const rtexture_create_info &ctinfo, renderer *rndr)
{
    asrt(rndr);
    asrt(ctinfo.data);
    asrt(ctinfo.dims > uvec3{});
    asrt(ctinfo.data_size > 0);
    asrt(ctinfo.name);

    VkCommandBuffer tmp_cmd_buf;
    int result = vkr_alloc_cmd_bufs(&tmp_cmd_buf, {.pool = rndr->transient_pool}, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        return {};
    }
    auto tmp_q = rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE];

    rtexture_info ti{};

    // Just in case we ensure null terminated
    strncpy(ti.name, ctinfo.name, SMALL_STR_LEN - 1);
    ti.name[SMALL_STR_LEN - 1] = 0;

    vkr_image_cfg cfg{};
    cfg.format = get_vk_format(ctinfo.format);
    asrt(cfg.format != VK_FORMAT_UNDEFINED && "Forgot to add vk support to rformat type");
    cfg.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    cfg.dims = ctinfo.dims;
    cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;

    // Create image
    int vk_ret = vkr_init_image(&ti.im, cfg);
    if (vk_ret != err_code::VKR_NO_ERROR) {
        vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
        return {};
    }

    // Upload texture data to created image using staging buffer
    // NOTE: This call currently blocks with waiting on a
    vk_ret = vkr_stage_and_upload_image_data(&ti.im, ctinfo.data, ctinfo.data_size, tmp_cmd_buf, tmp_q, &rndr->vk);
    vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
    if (vk_ret != err_code::VKR_NO_ERROR) {
        vkr_terminate_image(&ti.im, &rndr->vk);
        return {};
    }

    // Create image view
    vkr_image_view_cfg iview_cfg{};
    iview_cfg.image = &ti.im;
    vk_ret = vkr_init_image_view(&ti.im_view, iview_cfg, &rndr->vk);
    if (vk_ret != err_code::VKR_NO_ERROR) {
        vkr_terminate_image(&ti.im, &rndr->vk);
        return {};
    }

    rtexture_handle thndl = acquire_slot(&rndr->textures);
    if (!is_valid(thndl)) {
        vkr_terminate_image(&ti.im, &rndr->vk);
        vkr_terminate_image_view(ti.im_view, &rndr->vk);
        return thndl;
    }
    rtexture_info *tex_item = get_slot_item(&rndr->textures, thndl);
    asrt(tex_item);
    *tex_item = ti;
    return thndl;
}

intern int init_swapchain_images_and_framebuffer(renderer *rndr)
{
    auto vk = &rndr->vk;
    auto dev = &rndr->vk.inst.device;

    vkr_image_cfg im_cfg{};
    im_cfg.dims = {dev->swapchain.extent.width, dev->swapchain.extent.height, 1};
    im_cfg.format = vkr_find_best_depth_format(&rndr->vk.inst.pdev_info);
    im_cfg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    im_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    im_cfg.vma_alloc = &dev->vma_alloc;

    if (!is_valid(rndr->swapchain_fb_depth_stencil)) {
        rndr->swapchain_fb_depth_stencil = acquire_slot(&rndr->textures);
    }
    auto sl_item = get_slot_item(&rndr->textures, rndr->swapchain_fb_depth_stencil);
    int err = vkr_init_image(&sl_item->im, im_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    vkr_image_view_cfg imv_cfg{};
    imv_cfg.srange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    imv_cfg.image = &sl_item->im;

    err = vkr_init_image_view(&sl_item->im_view, imv_cfg, vk);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // We need the render pass associated with our main framebuffer
    // vkr_framebuffer_attachment fb_att{.im_view = sl_item->im_view};
    // vkr_init_swapchain_framebuffers(dev, vk, rndr->rpasses[RPASS_TYPE_OPAQUE].vk_hndl, fb_att);
    return err;
}

intern int setup_global_descriptor_set_layouts(renderer *rndr)
{
    vkr_descriptor_set_layout_cfg cfg{};

    // Descripitor Set Layouts - Just one layout for the moment with a binding at 0 for uniforms and a binding at 1 for
    // image sampler
    cfg.set_layout_descs.size = RDESC_SET_LAYOUT_COUNT;

    // Descriptor layouts
    // Single Uniform buffer
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    // Add uniform buffer binding to each set, and image sampler to material set as well
    arr_push_back(&cfg.set_layout_descs[RDESC_SET_LAYOUT_FRAME].bindings, b);

    // Add image sampler to material
    b.binding = 0;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    arr_push_back(&cfg.set_layout_descs[RDESC_SET_LAYOUT_MATERIAL].bindings, b);

    // Add image sampler to material
    b.binding = 1;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    arr_push_back(&cfg.set_layout_descs[RDESC_SET_LAYOUT_MATERIAL].bindings, b);

    // Set the size to the same as config
    rndr->set_layouts.size = cfg.set_layout_descs.size;
    return vkr_init_desc_set_layouts(rndr->set_layouts.data, cfg, &rndr->vk);
}

intern int setup_global_pipeline_layout(renderer *rndr)
{
    vkr_pipeline_layout_cfg cfg{};
    cfg.set_layouts = rndr->set_layouts.data;
    cfg.set_layout_count = rndr->set_layouts.size;

    // Setup our push constant
    ++cfg.push_constant_ranges.size;
    cfg.push_constant_ranges[0].offset = 0;
    cfg.push_constant_ranges[0].size = sizeof(push_constants);
    cfg.push_constant_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    return vkr_init_pipeline_layout(&rndr->g_layout, cfg, &rndr->vk);
}

intern int setup_rendering(renderer *rndr)
{
    ilog("Setting up default rendering...");
    auto vk = &rndr->vk;
    auto dev = &rndr->vk.inst.device;

    int err = setup_render_passes(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup render pass");
        return err;
    }

    err = setup_global_descriptor_set_layouts(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup global descriptor set layouts");
        return err;
    }

    err = setup_global_pipeline_layout(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup global pipeline layout");
        return err;
    }

    err = setup_diffuse_technique(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup pipeline");
        return err;
    }

    err = init_swapchain_images_and_framebuffer(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup swapchain images/framebuffers");
        return err;
    }

    // err = setup_geometry_buffers(rndr);
    // if (err != err_code::VKR_NO_ERROR) {
    //     elog("Failed to setup geometry buffers");
    //     return err;
    // }

    ////////////////////////////////////////////////////////////////////////////////
    // Create uniform buffers and descriptor sets pointing to them for each frame //
    ////////////////////////////////////////////////////////////////////////////////
    // for (int i = 0; i < dev->rframes.size; ++i) {
    //     // Create a uniform buffers for each frame

    //     // Frame uniform buffer - per frame data
    //     vkr_buffer_cfg buf_cfg{};
    //     buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    //     buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    //     buf_cfg.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    //     buf_cfg.buffer_size = vkr_uniform_buffer_offset_alignment(vk, sizeof(frame_ubo_data));
    //     buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    //     buf_cfg.vma_alloc = &dev->vma_alloc;

    //     vkr_buffer frame_uniform_buf{};
    //     int err = vkr_init_buffer(&frame_uniform_buf, buf_cfg);
    //     if (err != err_code::VKR_NO_ERROR) {
    //         return err;
    //     }
    //     dev->rframes[i].frame_ubo_ind = vkr_add_buffer(dev, frame_uniform_buf);

    //     // Pipeline uniform buffer - per pipeline data
    //     buf_cfg.buffer_size = MAX_PIPELINE_COUNT * vkr_uniform_buffer_offset_alignment(vk, sizeof(pipeline_ubo_data));
    //     vkr_buffer pl_uniform_buf{};
    //     err = vkr_init_buffer(&pl_uniform_buf, buf_cfg);
    //     if (err != err_code::VKR_NO_ERROR) {
    //         return err;
    //     }
    //     dev->rframes[i].pl_ubo_ind = vkr_add_buffer(dev, pl_uniform_buf);

    //     // Material uniform buffer - per material data
    //     buf_cfg.buffer_size = MAX_MATERIAL_COUNT * vkr_uniform_buffer_offset_alignment(vk, sizeof(material_ubo_data));
    //     vkr_buffer mat_uniform_buf{};
    //     err = vkr_init_buffer(&mat_uniform_buf, buf_cfg);
    //     if (err != err_code::VKR_NO_ERROR) {
    //         return err;
    //     }
    //     dev->rframes[i].mat_ubo_ind = vkr_add_buffer(dev, mat_uniform_buf);

    //     // Object uniform buffer - per material data
    //     buf_cfg.buffer_size = MAX_OBJECT_COUNT * vkr_uniform_buffer_offset_alignment(vk, sizeof(obj_ubo_data));
    //     vkr_buffer obj_uniform_buf{};
    //     err = vkr_init_buffer(&obj_uniform_buf, buf_cfg);
    //     if (err != err_code::VKR_NO_ERROR) {
    //         return err;
    //     }
    //     dev->rframes[i].obj_ubo_ind = vkr_add_buffer(dev, obj_uniform_buf);
    // }
    return err_code::VKR_NO_ERROR;
}

intern int record_command_buffer(renderer *rndr, vkr_framebuffer *fb, frame_context *cur_frame)
{
    auto dev = &rndr->vk.inst.device;

    int err = vkr_begin_cmd_buf(cur_frame->cmd_buffer, {});
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    VkClearValue att_clear_vals[] = {{.color{{0.05f, 0.05f, 0.05f, 1.0f}}}, {.depthStencil{1.0f, 0}}};

    // // Bind the global vertex/index buffer/s
    // VkBuffer vert_bufs[RVERT_STREAM_COUNT]{};
    // VkDeviceSize offsets[RVERT_STREAM_COUNT]{};
    // for (int i = 0; i < RVERT_STREAM_COUNT; ++i) {
    //     vert_bufs[i] = rndr->geometry_buffers.vert_buffers[i].hndl;
    // }
    // vkCmdBindVertexBuffers(cur_frame->cmd_buffer, 0, 1, vert_bufs, offsets);
    // vkCmdBindIndexBuffer(cur_frame->cmd_buffer, rndr->geometry_buffers.ind_buffer.hndl, 0, get_vk_index_type(sizeof(ind_t)));

    // TODO: This really can't go in any order we want for render passes.. We might have dependency ordered between
    // them.. for now we just have the one.. I'm not sure if we need to do a
    for (int rpind = 0; rpind < rndr->rpasses.size; ++rpind) {
        auto rpass = &rndr->rpasses[rpind];
        vkr_cmd_begin_rpass(cur_frame->cmd_buffer, rpass->vk_hndl, fb, att_clear_vals, 2);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)fb->size.w;
        viewport.height = (float)fb->size.h;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cur_frame->cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {fb->size.w, fb->size.h};
        vkCmdSetScissor(cur_frame->cmd_buffer, 0, 1, &scissor);

// Bind frame rpass descriptor set
// auto ds = cur_frame->desc_pool.desc_sets[rpass_iter->val->frame_set_layouti].hndl;
// vkCmdBindDescriptorSets(
//     cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, G_FRAME_PL_LAYOUT, DESCRIPTOR_SET_LAYOUT_FRAME, 1, &ds, 0, nullptr);

// // We could make our render pass have the vert/index buffer info.. ie
// auto pl_iter = hmap_begin(&rpass_iter->val->plines);
// while (pl_iter) {
//     // Grab the pipeline and set it, and set the viewport/scissor
//     auto pipeline = &dev->pipelines[pl_iter->val->plinfo->plind];
//     vkCmdBindPipeline(cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->hndl);

//     auto ds = cur_frame->desc_pool.desc_sets[pl_iter->val->set_layouti].hndl;
//     vkCmdBindDescriptorSets(
//         cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_hndl, DESCRIPTOR_SET_LAYOUT_PIPELINE, 1, &ds, 0,
//         nullptr);

//     auto mat_iter = hmap_begin(&pl_iter->val->mats);
//     while (mat_iter) {
//         // Bind the material set
//         auto ds = cur_frame->desc_pool.desc_sets[mat_iter->val->set_layouti].hndl;
//         vkCmdBindDescriptorSets(
//             cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_hndl, DESCRIPTOR_SET_LAYOUT_MATERIAL, 1, &ds, 0,
//             nullptr);

//         for (u32 dci = 0; dci < mat_iter->val->dcs.size; ++dci) {
//             const draw_call *cur_dc = &mat_iter->val->dcs[dci];
//             auto ds = cur_frame->desc_pool.desc_sets[rpass_iter->val->obj_set_layouti].hndl;
//             sizet obj_ubo_item_size = vkr_uniform_buffer_offset_alignment(&rndr->vk, sizeof(obj_ubo_data));
//             // Our dynamic ubo_offset in to our singlestoring all of our transforms is computed by adding
//             // the material base draw call ubo_offset (computed each frame).
//             u32 dyn_offset = (u32)(obj_ubo_item_size * cur_dc->ubo_offset);
//             vkCmdBindDescriptorSets(cmd_buf->hndl,
//                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
//                                     pipeline->layout_hndl,
//                                     DESCRIPTOR_SET_LAYOUT_OBJECT,
//                                     1,
//                                     &ds,
//                                     1,
//                                     &dyn_offset);

//             push_constants pc{3};
//             vkCmdPushConstants(cmd_buf->hndl, pipeline->layout_hndl, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &pc);
//             vkCmdDrawIndexed(cmd_buf->hndl,
//                              cur_dc->index_count,
//                              cur_dc->instance_count,
//                              cur_dc->first_index,
//                              cur_dc->vertex_offset,
//                              cur_dc->first_instance);
//         }
//         mat_iter = hmap_next(&pl_iter->val->mats, mat_iter);
//     }
//     pl_iter = hmap_next(&rpass_iter->val->plines, pl_iter);
// }

// If we are on the imgui rpass, render its stuff. It has it's own pipeling, vertex/index buffers, etc
#ifdef USE_IMGUI
        if (rpass->vk_hndl == rndr->imgui.rpass) {
            auto img_data = ImGui::GetDrawData();
            ImGui_ImplVulkan_RenderDrawData(img_data, cur_frame->cmd_buffer);
        }
#endif

        vkr_cmd_end_rpass(cur_frame->cmd_buffer);
    }

    return vkr_end_cmd_buf(cur_frame->cmd_buffer);
}

intern int init_frame_contexts(renderer *rndr)
{
    auto dev = &rndr->vk.inst.device;
    rndr->fifs.size = rndr->fifs.capacity;

    // Create frame synchronization objects - start all fences as signalled already
    for (u32 framei = 0; framei < rndr->fifs.size; ++framei) {
        auto cur_fif = &rndr->fifs[framei];

        int result = vkr_init_fence(&cur_fif->in_flight, VK_FENCE_CREATE_SIGNALED_BIT, &rndr->vk);
        if (result != VK_SUCCESS) {
            return result;
        }

        result = vkr_init_semaphore(&cur_fif->image_avail, {}, &rndr->vk);
        if (result != VK_SUCCESS) {
            return result;
        }

        // Get a count of the number of descriptors we are making avaialable for each desc type
        vkr_desc_cfg desc_cfg{};
        desc_cfg.max_sets = MAX_MATERIAL_COUNT;
        desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = MAX_MATERIAL_COUNT;
        desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = MAX_MATERIAL_COUNT;
        result = vkr_init_desc_pool(&cur_fif->desc_pool, desc_cfg, &rndr->vk);
        if (result != VK_SUCCESS) {
            elog("Failed to create descriptor pool for frame %d - aborting init", framei);
            return result;
        }

        // Create frame command pool
        vkr_init_cmd_pool(
            &cur_fif->cmd_pool, dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].fam_ind, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &rndr->vk);

        // Create frame command buffer
        vkr_alloc_cmd_bufs(&cur_fif->cmd_buffer, {.pool = cur_fif->cmd_pool}, &rndr->vk);
    }
    ilog("Successfully initialized %lu render frames in flight", rndr->fifs.size);
    return err_code::VKR_NO_ERROR;
}

intern void terminate_frame_contexts(renderer *rndr)
{
    auto dev = &rndr->vk.inst.device;
    for (u32 framei = 0; framei < rndr->fifs.size; ++framei) {
        auto cur_fif = &rndr->fifs[framei];
        vkr_terminate_fence(cur_fif->in_flight, &rndr->vk);
        vkr_terminate_semaphore(cur_fif->image_avail, &rndr->vk);
        vkr_terminate_desc_pool(cur_fif->desc_pool, &rndr->vk);
        vkr_terminate_cmd_pool(cur_fif->cmd_pool, &rndr->vk);
    }
    arr_clear(&rndr->fifs);
}

intern void terminate_swapchain_images_and_framebuffer(renderer *rndr)
{
    auto dev = &rndr->vk.inst.device;
    auto sl_item = get_slot_item(&rndr->textures, rndr->swapchain_fb_depth_stencil);
    vkr_terminate_image_view(sl_item->im_view, &rndr->vk);
    vkr_terminate_image(&sl_item->im, &rndr->vk);
    vkr_terminate_swapchain_framebuffers(dev, &rndr->vk);
}

intern void recreate_swapchain(renderer *rndr)
{
    ilog("Recreating swapchain");
    // Recreating the swapchain will wait on all semaphores and fences before continuing
    auto dev = &rndr->vk.inst.device;
    vkr_device_wait_idle(dev);
    terminate_swapchain_images_and_framebuffer(rndr);
    vkr_terminate_swapchain(&dev->swapchain, &rndr->vk);
    vkr_terminate_surface(&rndr->vk, rndr->vk.inst.surface);
    vkr_init_surface(&rndr->vk, &rndr->vk.inst.surface);
    vkr_init_swapchain(&dev->swapchain, &rndr->vk);
    init_swapchain_images_and_framebuffer(rndr);
}

intern void teardown_geometry_stream_group(geom_streams_group *gp, const vkr_context *vk)
{
    for (u32 i = 0; i < gp->layouts.size; ++i) {
        vmaDestroyVirtualBlock(gp->layouts[i].vert_block);
        for (u32 bufi = 0; bufi < gp->layouts[i].vert_streams.size; ++bufi) {
            vkr_terminate_buffer(&gp->layouts[i].vert_streams[bufi].buffer, vk);
        }
    }
    vmaDestroyVirtualBlock(gp->indices_block);
    vkr_terminate_buffer(&gp->indice_stream.buffer, vk);
    *gp = {};
}

int begin_render_frame(renderer *rndr, int finished_frames)
{
    auto dev = &rndr->vk.inst.device;

    reset_arena(&rndr->vk_frame_linear);
    reset_arena(&rndr->frame_linear);
    reset_arena(&rndr->frame_stack);

// Start GUI frame
#ifdef USE_IMGUI
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#endif

    // Update finished frames which is used to get the current frame
    rndr->finished_frames = finished_frames;
    int current_frame_ind = rndr->finished_frames % MAX_FRAMES_IN_FLIGHT;
    auto *cur_frame = &rndr->fifs[current_frame_ind];

    // We wait until this FIF's fence has been triggered before rendering the frame. FIF fences are created in a
    // triggered state so there will be no waiting on the first time. We then reset the fence (aka set it to
    // untriggered) and it is passed to the vkQueueSubmit call to trigger it again. So if not the first time rendering
    // this FIF, we are waiting for the vkQueueSubmit from the previous time this FIF was rendered to complete
    int vk_res = vkWaitForFences(dev->hndl, 1, &cur_frame->in_flight, VK_TRUE, UINT64_MAX);
    if (vk_res != VK_SUCCESS) {
        elog("Failed to wait for fence");
        return err_code::RENDER_WAIT_FENCE_FAIL;
    }

    // Clear all prev desc sets
    // vkr_reset_descriptor_pool(&cur_frame->desc_pool, &rndr->vk);

    return err_code::RENDER_NO_ERROR;
}

int end_render_frame(renderer *rndr, camera *cam, f64 dt)
{
    auto dev = &rndr->vk.inst.device;
    int current_frame_ind = rndr->finished_frames % MAX_FRAMES_IN_FLIGHT;
    auto *cur_frame = &rndr->fifs[current_frame_ind];

    if (window_resized_this_frame(rndr->vk.cfg.window)) {
        rndr->no_resize_frames = 0.0;
    }
    else {
        rndr->no_resize_frames += dt;
    }

    /////////////////////////////////
    // Acquire Swapchain Image Ind //
    /////////////////////////////////
    // Acquire the image, signal the image_avail semaphore once the image has been acquired. We get the index back, but
    // that doesn't mean the image is ready. The image is only ready (on the GPU side) once the image avail semaphore is triggered
    u32 im_ind{};
    int vk_res = vkAcquireNextImageKHR(dev->hndl, dev->swapchain.swapchain, UINT64_MAX, cur_frame->image_avail, VK_NULL_HANDLE, &im_ind);

    // If the image is out of date/suboptimal we need to recreate the swapchain and our caller needs to exit early as
    // well. It seems that on some platforms, if the result from above is out of date or suboptimal, the semaphore
    // associated with it will never get triggered. So if we were to continue and just resize at the end of frame it
    // wouldn't work because the queue submit would never fire as it depends on this image available semaphore.
    // At least.. i think?
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR) {
        if (rndr->no_resize_frames > RESIZE_DEBOUNCE_FRAME_COUNT) {
            recreate_swapchain(rndr);
        }
#ifdef USE_IMGUI
        ImGui::EndFrame();
#endif
        return vk_res;
    }
    else if (vk_res != VK_SUCCESS && vk_res != VK_SUBOPTIMAL_KHR) {
        elog("Failed to acquire swapchain image");
#ifdef USE_IMGUI
        ImGui::EndFrame();
#endif
        return err_code::RENDER_ACQUIRE_IMAGE_FAIL;
    }

    if (cam) {
        svec2 sz = get_window_pixel_size(rndr->vk.cfg.window);
        if (cam->vp_size != sz) {
            rndr->no_resize_frames = 0;
            cam->vp_size = sz;
            cam->proj = (math::perspective(cam->fov, (f32)cam->vp_size.w / (f32)cam->vp_size.h, cam->near_far.x, cam->near_far.y));
        }
    }

// Finalize IM GUI data - not dependent on our render stuff currently
#ifdef USE_IMGUI
    ImGui::Render();
#endif

    // The command buf index struct has an ind struct into the pool the cmd buf comes from, and then an ind into the buffer
    // The ind into the pool has an ind into the queue family (as that contains our array of command pools) and then and
    // ind to the command pool
    auto fb = &dev->swapchain.fbs[im_ind];
    asrt(fb && "Invalid framebuffer");

    ///////////////////////////
    // Record Command Buffer //
    ///////////////////////////
    // We have the acquired image index, though we don't know when it will be ready to have ops submitted, we can record
    // the ops in the command buffer and submit once it is ready
    // This takes about %80 of the run frame
    vk_res = record_command_buffer(rndr, fb, cur_frame);
    if (vk_res != err_code::RENDER_NO_ERROR) {
        return vk_res;
    }

    /////////////////////
    // Reset FIF Fence //
    /////////////////////
    // Here we reset the fence for the current frame fence as we know we are going to call queue submit which is the
    // only thing that will trigger the fence - so this is why this reset needs to come here (rather than right after waiting)
    vk_res = vkResetFences(dev->hndl, 1, &cur_frame->in_flight);
    if (vk_res != VK_SUCCESS) {
        elog("Failed to reset fence");
        return err_code::RENDER_RESET_FENCE_FAIL;
    }

    //////////////////////////////////
    // Submit command buffer to GPU //
    //////////////////////////////////
    // Get the info ready to submit our command buffer to the queue. We need to wait until the image avail semaphore has
    // signaled, and then we need to trigger the render finished signal once the the command buffer completes
    VkSubmitInfo submit_info{};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &cur_frame->image_avail;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cur_frame->cmd_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &rndr->vk.inst.device.swapchain.renders_finished[im_ind];
    if (vkQueueSubmit(dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE], 1, &submit_info, cur_frame->in_flight) != VK_SUCCESS) {
        return err_code::RENDER_SUBMIT_QUEUE_FAIL;
    }

    ///////////////////
    // Present Image //
    ///////////////////
    // Once the rendering signal has fired, present the image (show it on screen)
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &rndr->vk.inst.device.swapchain.renders_finished[im_ind];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &dev->swapchain.swapchain;
    present_info.pImageIndices = &im_ind;
    present_info.pResults = nullptr; // Optional - check for individual swaps
    vk_res = vkQueuePresentKHR(dev->qfams[VKR_QUEUE_FAM_TYPE_PRESENT].qs[VKR_RENDER_QUEUE], &present_info);

    // This purely helps with smoothness - it works fine without recreating the swapchain here and instead doing it on
    // the next frame, but it seems to resize more smoothly doing it here
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR || vk_res == VK_SUBOPTIMAL_KHR) {
        if (rndr->no_resize_frames > RESIZE_DEBOUNCE_FRAME_COUNT) {
            recreate_swapchain(rndr);
        }
    }
    else if (vk_res != VK_SUCCESS) {
        elog("Failed to presenet KHR");
        return err_code::RENDER_PRESENT_KHR_FAIL;
    }

    return err_code::RENDER_NO_ERROR;
}

int init_renderer(renderer *rndr, void *win_hndl, mem_arena *fl_arena)
{
    asrt(fl_arena->alloc_type == mem_alloc_type::FREE_LIST);
    init_fl_arena(&rndr->persist_fl, 200 * MB_SIZE, fl_arena, "rndr-fl");
    init_fl_arena(&rndr->vk_free_list, 50 * MB_SIZE, &rndr->persist_fl, "rndr-vk-fl");
    init_lin_arena(&rndr->vk_frame_linear, 10 * MB_SIZE, &rndr->persist_fl, "rndr-vk-frame");
    init_lin_arena(&rndr->frame_linear, 10 * KB_SIZE, fl_arena, "rndr-frame-linear");
    init_lin_arena(&rndr->frame_stack, 10 * MB_SIZE, fl_arena, "rndr-frame-stack");

    // Slot pools
    init_slot_pool(&rndr->techniques, MAX_TECHNIQUE_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->materials, MAX_MATERIAL_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->textures, MAX_TEXTURE_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->geometry, MAX_MESH_COUNT, &rndr->persist_fl);
    dlog("HERE");
    // Render pass names
    hmap_init(&rndr->rpass_name_map, hash_type, &rndr->persist_fl);
    hmap_init(&rndr->pline_cache, hash_type, &rndr->persist_fl);

    vkr_cfg vkii{.app_name = "rdev",
                 .vi{1, 0, 0},
                 .arenas{.persistent_arena = &rndr->vk_free_list, .command_arena = &rndr->vk_frame_linear},
                 .log_verbosity = LOG_DEBUG,
                 .window = win_hndl,
                 .inst_create_flags = INST_CREATE_FLAGS,
                 .extra_instance_extension_names = ADDITIONAL_INST_EXTENSIONS,
                 .extra_instance_extension_count = ADDITIONAL_INST_EXTENSION_COUNT,
                 .device_extension_names = DEVICE_EXTENSIONS,
                 .device_extension_count = DEVICE_EXTENSION_COUNT,
                 .validation_layer_names = VALIDATION_LAYERS,
                 .validation_layer_count = VALIDATION_LAYER_COUNT};

    if (vkr_init(&vkii, &rndr->vk) != err_code::VKR_NO_ERROR) {
        return err_code::RENDER_INIT_FAIL;
    }

    // Create transient command pool
    vkr_init_cmd_pool(&rndr->transient_pool,
                      rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].fam_ind,
                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                      &rndr->vk);

    // Setup frames in flight
    init_frame_contexts(rndr);

    int err = setup_rendering(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to initialize renderer with code %d", err);
        return err;
    }

#ifdef USE_IMGUI
    init_imgui(rndr, win_hndl);
#endif

    // Setup our indice and vert buffer sbuffer
    return err_code::RENDER_NO_ERROR;
}

void terminate_renderer(renderer *rndr)
{
    ilog("Terminating");
    hmap_terminate(&rndr->rpass_name_map);

    reset_arena(&rndr->vk_frame_linear);
    reset_arena(&rndr->frame_linear);
    reset_arena(&rndr->frame_stack);

    // Device needs to be idle before finishing with everything
    vkr_device_wait_idle(&rndr->vk.inst.device);

// IMGUI
#ifdef USE_IMGUI
    terminate_imgui(rndr);
#endif

    // Terminate all meshes
    for (int i = 0; i < rndr->geometry.slots.size; ++i) {
        destroy_geometry(get_slot_current_handle(&rndr->geometry, i), rndr);
    }
    terminate_slot_pool(&rndr->geometry);

    // Terminate all images and image views
    for (int i = 0; i < rndr->textures.slots.size; ++i) {
        vkr_terminate_image(&rndr->textures.slots[i].item.im, &rndr->vk);
        vkr_terminate_image_view(rndr->textures.slots[i].item.im_view, &rndr->vk);
    }
    terminate_slot_pool(&rndr->textures);

    for (int i = 0; i < rndr->materials.slots.size; ++i) {
        // Do something
    }
    terminate_slot_pool(&rndr->materials);

    for (int i = 0; i < rndr->techniques.slots.size; ++i) {
        // Do something
    }
    terminate_slot_pool(&rndr->techniques);

    // Terminate all texture samplers
    for (int i = 0; i < rndr->samplers.size; ++i) {
        vkr_terminate_sampler(rndr->samplers[i].vk_hndl, &rndr->vk);
    }
    arr_clear(&rndr->samplers);

    // Remove source geometry buffers
    for (int i = 0; i < rndr->geom_groups.size; ++i) {
        teardown_geometry_stream_group(&rndr->geom_groups[i], &rndr->vk);
    }
    rndr->geom_groups.size = 0;

    // Terminate our default descriptor layout sets
    dlog("Should be terminating %d layouts", rndr->set_layouts.size);
    vkr_terminate_desc_set_layouts(rndr->set_layouts.data, rndr->set_layouts.size, &rndr->vk);
    arr_clear(&rndr->set_layouts);

    // Global pipeline layout
    vkr_terminate_pipeline_layout(rndr->g_layout, &rndr->vk);

    // Terminate all pipelines
    for (auto iter = hmap_begin(&rndr->pline_cache); iter; iter = hmap_next(&rndr->pline_cache, iter)) {
        vkr_terminate_pipeline(iter->val, &rndr->vk);
    }
    hmap_terminate(&rndr->pline_cache);

    // Terminate all render passes
    for (int i = 0; i < rndr->rpasses.size; ++i) {
        vkr_terminate_render_pass(rndr->rpasses[i].vk_hndl, &rndr->vk);
        rndr->rpasses.size = 0;
    }

    // Transient pool
    vkr_terminate_cmd_pool(rndr->transient_pool, &rndr->vk);

    // Don't free the depth image view/image (or other swapchain atts) as they will just have been freed
    vkr_terminate_swapchain_framebuffers(&rndr->vk.inst.device, &rndr->vk);

    terminate_frame_contexts(rndr);
    vkr_terminate(&rndr->vk);
    terminate_arena(&rndr->vk_free_list);
    terminate_arena(&rndr->vk_frame_linear);
    terminate_arena(&rndr->frame_linear);
    terminate_arena(&rndr->frame_stack);
    terminate_arena(&rndr->persist_fl);
}

intern u32 get_format_byte_size(rformat format)
{
    switch (format) {
    // 128-bit formats (16 bytes per pixel)
    case rformat::RGBA32_SFLOAT:
    case rformat::RGBA32_UINT:
    case rformat::RGBA32_SINT:
        return 16;

    // 96-bit formats (12 bytes per pixel)
    case rformat::RGB32_SFLOAT:
    case rformat::RGB32_UINT:
    case rformat::RGB32_SINT:
        return 12;

    // 64-bit formats (8 bytes per pixel)
    case rformat::RGBA16_SFLOAT:
    case rformat::RGBA16_UNORM:
    case rformat::RGBA16_SNORM:
    case rformat::RGBA16_UINT:
    case rformat::RGBA16_SINT:
    case rformat::RG32_SFLOAT:
    case rformat::RG32_UINT:
    case rformat::RG32_SINT:
        return 8;

    // 48-bit formats (6 bytes per pixel)
    case rformat::RGB16_SFLOAT:
    case rformat::RGB16_UNORM:
    case rformat::RGB16_SNORM:
    case rformat::RGB16_UINT:
    case rformat::RGB16_SINT:
        return 6;

    // 32-bit formats (4 bytes per pixel)
    case rformat::RGBA8_SRGB:
    case rformat::RGBA8_UNORM:
    case rformat::RGBA8_SNORM:
    case rformat::RGBA8_UINT:
    case rformat::RGBA8_SINT:
    case rformat::RG16_SFLOAT:
    case rformat::RG16_UNORM:
    case rformat::RG16_SNORM:
    case rformat::RG16_UINT:
    case rformat::RG16_SINT:
    case rformat::R32_SFLOAT:
    case rformat::R32_UINT:
    case rformat::R32_SINT:
        return 4;

    // 24-bit formats (3 bytes per pixel)
    case rformat::RGB8_SRGB:
    case rformat::RGB8_UNORM:
    case rformat::RGB8_SNORM:
    case rformat::RGB8_UINT:
    case rformat::RGB8_SINT:
        return 3;

    // 16-bit formats (2 bytes per pixel)
    case rformat::RG8_SRGB:
    case rformat::RG8_UNORM:
    case rformat::RG8_SNORM:
    case rformat::RG8_UINT:
    case rformat::RG8_SINT:
    case rformat::R16_SFLOAT:
    case rformat::R16_UNORM:
    case rformat::R16_SNORM:
    case rformat::R16_UINT:
    case rformat::R16_SINT:
        return 2;

    // 8-bit formats (1 byte per pixel)
    case rformat::R8_SRGB:
    case rformat::R8_UNORM:
    case rformat::R8_SNORM:
    case rformat::R8_UINT:
    case rformat::R8_SINT:
        return 1;

    // Compressed formats (Handled as special cases)
    // Note: For BC/ASTC, you generally want a get_block_size() function.
    // Returning 0 or an assertion here forces the caller to handle
    // the block-based nature of compressed data.
    case rformat::RGBA8_SRGB_COMPRESSED:
    case rformat::RGBA8_UNORM_COMPRESSED:
    case rformat::RGB8_SRGB_COMPRESSED:
    case rformat::RGB8_UNORM_COMPRESSED:
    case rformat::RG8_UNORM_COMPRESSED:
    case rformat::RG8_SNORM_COMPRESSED:
    case rformat::R8_UNORM_COMPRESSED:
    case rformat::R8_SNORM_COMPRESSED: {
        // If these are BC1-BC3, they are technically 0.5 to 1 byte per pixel
        // on average, but memory must be allocated in blocks.
        asrt_break("Cannot get simple pixel size for compressed format. Use block size.");
        return 0;
    }

    case rformat::INVALID:
    default:
        asrt_break("Invalid format");
        return 0;
    }
}

intern bool fill_geometry_layout_entry(geometry_buffer_layout_entry *layout,
                                       sizet cur_buffer_offset,
                                       const geometry_vert_layout_desc &desc,
                                       const vkr_context *vk)
{
    vkr_buffer_cfg alloc_cfg{};
    alloc_cfg.alloc_flags = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    alloc_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    alloc_cfg.vma_alloc = &vk->inst.device.vma_alloc;
    alloc_cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    layout->vert_layout.bindings.size = desc.streams.size;
    layout->vert_streams.size = desc.streams.size;

    // Virtual block used for this layout entry - we use vert stream 0 as the guide for all other vert streams.. that is
    // it dictates at what range (in vertices) each buffer uses for each mesh.. this is not the most "efficient" thing
    // since other buffers might do better with space usage if they had their own block, but it allows us to bind all
    // vert buffers at once and use them in shaders
    VmaVirtualBlockCreateInfo ci{};

    // Create the vert buffers
    bool failed{false};
    for (u32 stri = 0; stri < desc.streams.size && !failed; ++stri) {
        auto cur_binding = &layout->vert_layout.bindings[stri];
        auto cur_stream_desc = &desc.streams[stri];
        auto cur_buffer = &layout->vert_streams[stri];

        cur_binding->binding = cur_buffer_offset + stri;
        cur_binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        sizet layout_attrib_offset = layout->vert_layout.attribs.size;
        layout->vert_layout.attribs.size += cur_stream_desc->attribs.size;

        for (u32 atti = 0; atti < cur_stream_desc->attribs.size; ++atti) {
            auto cur_attrib_desc = &cur_stream_desc->attribs[atti];
            auto cur_attrib_layout = &layout->vert_layout.attribs[atti + layout_attrib_offset];

            cur_attrib_layout->binding = cur_binding->binding;
            cur_attrib_layout->format = get_vk_format(cur_attrib_desc->fmt);
            cur_attrib_layout->location = cur_attrib_desc->shader_location;
            cur_attrib_layout->offset = cur_binding->stride;

            cur_binding->stride += get_format_byte_size(cur_attrib_desc->fmt);
        }

        strncpy(cur_buffer->dbg_name, cur_stream_desc->dbg_name, SMALL_STR_LEN - 1);

        alloc_cfg.buffer_size = desc.max_vert_count * cur_binding->stride;
        alloc_cfg.user_data = cur_buffer->dbg_name;

        if (stri == 0) {
            ci.size = alloc_cfg.buffer_size;
        }

        int result = vkr_init_buffer(&cur_buffer->buffer, alloc_cfg);
        failed = result != err_code::VKR_NO_ERROR;
        // layout->vert_layout.bindings[stri].stride =
    }

    // Create the virtual block using stream 0 as the guide (ci.size set in the loop above)
    if (!failed) {
        ci.pAllocationCallbacks = &vk->alloc_cbs;
        int result = vmaCreateVirtualBlock(&ci, &layout->vert_block);
        failed = result != VK_SUCCESS;
        if (failed) {
            wlog("Failed to create virtual block - error code: %d", result);
        }
    }
    return !failed;
}

runtime_id create_geometry_stream_group(renderer *rndr, const geometry_group_desc &desc)
{
    asrt(desc.max_ind_count > 0);
    asrt(desc.layouts.size > 0);
    asrt(desc.layouts[0].streams.size > 0);
    asrt(desc.layouts[0].streams[0].attribs.size > 0);
    asrt(rndr->geom_groups.size < rndr->geom_groups.capacity);

    auto geom_id = rndr->geom_groups.size++;
    auto cur_group = &rndr->geom_groups[geom_id];

    // Create all vert layout groups
    cur_group->layouts.size = desc.layouts.size;
    sizet buffer_offset{0};
    bool failed{false};
    for (int i = 0; i < desc.layouts.size && !failed; ++i) {
        failed = !fill_geometry_layout_entry(&cur_group->layouts[i], buffer_offset, desc.layouts[i], &rndr->vk);
        buffer_offset += cur_group->layouts[i].vert_streams.size;
    }

    // Create the indice buffer and virtual block for the indice buffer
    if (!failed) {
        // Set debug name - ind buffer stores entry for whole geometry layout
        strncpy(cur_group->indice_stream.dbg_name, desc.dbg_name, SMALL_STR_LEN - 1);

        vkr_buffer_cfg alloc_cfg{};
        alloc_cfg.alloc_flags = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        alloc_cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;
        alloc_cfg.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        alloc_cfg.buffer_size = desc.max_ind_count * sizeof(ind_t);
        alloc_cfg.user_data = cur_group->indice_stream.dbg_name;
        int result = vkr_init_buffer(&cur_group->indice_stream.buffer, alloc_cfg);
        failed = result != err_code::VKR_NO_ERROR;
        if (!failed) {
            // Create the virtual block for the indice buffer
            VmaVirtualBlockCreateInfo ci{};
            ci.size = alloc_cfg.buffer_size;
            ci.pAllocationCallbacks = &rndr->vk.alloc_cbs;
            result = vmaCreateVirtualBlock(&ci, &cur_group->indices_block);
            failed = result != VK_SUCCESS;
            if (failed) {
                wlog("Failed to create virtual block - error code: %d", result);
            }
        }
    }

    if (failed) {
        teardown_geometry_stream_group(cur_group, &rndr->vk);
        --rndr->geom_groups.size;
        geom_id = INVALID_ID;
    }
    return geom_id;
}

rgeom_handle create_geometry(renderer *rndr, const rgeom_create_info &ci)
{
    // Make sure we have valid data
    asrt(ci.vert_count > 0);
    asrt(ci.vert_data);
    asrt(ci.ind_count > 0);
    asrt(ci.ind_data);
    asrt(ci.subgeom_cnt > 0);
    asrt(ci.subgeoms);
    asrt(ci.group < rndr->geom_groups.size);

    // Verify we have room for a mesh
    auto gp = &rndr->geom_groups[ci.group];
    asrt(ci.layout < gp->layouts.size);
    auto layout = &gp->layouts[ci.layout];

    rgeom_handle geom_hndl = acquire_slot(&rndr->geometry);
    if (!is_valid(geom_hndl)) {
        wlog("Out of geometry slots");
        return {};
    }
    rgeom_info *geom_item = get_slot_item(&rndr->geometry, geom_hndl);
    asrt(geom_item);

    // Set the vert/ind blocks
    geom_item->vert_block = layout->vert_block;
    geom_item->ind_block = gp->indices_block;

    // Set the name if it was filled in
    strncpy(geom_item->name, ci.name ? ci.name : "unnamed", SMALL_STR_LEN - 1);

    // Copy subgeom data
    geom_item->subgeom_vert_ind_counts.size = ci.subgeom_cnt;
    for (u32 i = 0; i < ci.subgeom_cnt; ++i) {
        geom_item->subgeom_vert_ind_counts[i] = ci.subgeoms[i];
    }

    // This info is shared between the vert stream and ind stream virtual alloc
    VmaVirtualAllocationCreateInfo alloc_ci{};
    alloc_ci.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    alloc_ci.pUserData = geom_item->name;

    // Create the virtual allocation using vert stream 0 which dictates the vert offset in to each stream
    alloc_ci.alignment = layout->vert_layout.bindings[0].stride;
    alloc_ci.size = ci.vert_count * alloc_ci.alignment;
    VkDeviceSize vert_stream_byte_offset{};
    s32 result = vmaVirtualAllocate(geom_item->vert_block, &alloc_ci, &geom_item->vert_mem, &vert_stream_byte_offset);
    if (result != err_code::VKR_NO_ERROR) {
        wlog("Vma virtual allocate for vert stream failed with code %d", result);
        release_slot(&rndr->geometry, geom_hndl);
        return {};
    }
    asrt(vert_stream_byte_offset % alloc_ci.alignment == 0);
    geom_item->vert_offset = vert_stream_byte_offset / alloc_ci.alignment;

    // Create the virtual allocation for indices
    alloc_ci.alignment = sizeof(ind_t);
    alloc_ci.size = ci.ind_count * alloc_ci.alignment;
    VkDeviceSize ind_stream_byte_offset{};
    result = vmaVirtualAllocate(geom_item->ind_block, &alloc_ci, &geom_item->ind_mem, &ind_stream_byte_offset);
    if (result != err_code::VKR_NO_ERROR) {
        wlog("Vma virtual allocate indices stream failed with code %d", result);
        vmaVirtualFree(geom_item->vert_block, geom_item->vert_mem);
        release_slot(&rndr->geometry, geom_hndl);
        return {};
    }
    asrt(ind_stream_byte_offset % alloc_ci.alignment == 0);
    geom_item->ind_offset = ind_stream_byte_offset / alloc_ci.alignment;

    // Create staging buffer and get the queue we will use
    VkCommandBuffer tmp_cmd_buf{};
    result = vkr_alloc_cmd_bufs(&tmp_cmd_buf, {.pool = rndr->transient_pool}, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        wlog("Failed to create command buffer - error code: %d", result);
        vmaVirtualFree(geom_item->vert_block, geom_item->vert_mem);
        vmaVirtualFree(geom_item->ind_block, geom_item->ind_mem);
        release_slot(&rndr->geometry, geom_hndl);
        asrt(destroy_geometry(geom_hndl, rndr));
        return {};
    }
    VkQueue tmp_q = rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE];

    // Copy data for the vert buffers
    for (u32 streami = 0; streami < layout->vert_streams.size; ++streami) {
        VkBufferCopy region{};
        region.size = ci.vert_count * layout->vert_layout.bindings[streami].stride;
        region.dstOffset = geom_item->vert_offset * layout->vert_layout.bindings[streami].stride;
        result = vkr_stage_and_upload_buffer_data(
            &layout->vert_streams[streami].buffer, ci.vert_data[streami], &region, 1, tmp_cmd_buf, tmp_q, &rndr->vk);
        if (result != err_code::VKR_NO_ERROR) {
            asrt(destroy_geometry(geom_hndl, rndr));
            vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
            return {};
        }
    }

    VkBufferCopy region{};
    region.size = ci.ind_count * sizeof(ind_t);
    region.dstOffset = geom_item->ind_offset * sizeof(ind_t);
    result = vkr_stage_and_upload_buffer_data(&gp->indice_stream.buffer, ci.ind_data, &region, 1, tmp_cmd_buf, tmp_q, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        asrt(destroy_geometry(geom_hndl, rndr));
        vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
        geom_hndl = {};
    }
    return geom_hndl;
}

geometry_vert_layout_desc *push_geometry_layout(geometry_group_desc *desc, u32 layout_max_vert_count)
{
    u32 ind = (u32)desc->layouts.size++;
    desc->layouts[ind].max_vert_count = layout_max_vert_count;
    return &desc->layouts[ind];
}

vert_stream_desc *push_geometry_stream(geometry_vert_layout_desc *vert_layout, const char *dbg_name)
{
    u32 ind = (u32)vert_layout->streams.size++;
    vert_layout->streams[ind].dbg_name = dbg_name;
    return &vert_layout->streams[ind];
}

void push_geometry_attribute(vert_stream_desc *stream, const vert_attrib_desc &att_desc)
{
    u32 ind = (u32)stream->attribs.size++;
    stream->attribs[ind] = att_desc;
}

} // namespace nslib
