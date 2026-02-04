#include "platform.h"
#include "vkr_context.h"
#include "renderer.h"
#include "vkr_utils.h"
#include "render_manifest.h"

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

intern constexpr f32 WINDOW_RESIZE_DEBOUNCE_DURATION = 0.05;
intern VkPipelineLayout G_FRAME_PL_LAYOUT{};

intern void imgui_mem_free(void *ptr, void *usr)
{
    mem_free(ptr, (mem_arena *)usr);
}

intern void *imgui_mem_alloc(sizet sz, void *usr)
{
    return mem_alloc(sz, (mem_arena *)usr, SIMD_MIN_ALIGNMENT);
}

intern void check_vk_result(VkResult result)
{
    asrt(result == VK_SUCCESS);
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

void init_imgui(renderer *rndr, const rbp_pass &pass)
{
    auto dev = &rndr->vk.inst.device;
    // 263 KB seems to be about the min required - we'll give it a MB
    init_fl_arena(&rndr->imgui.fl, MB_SIZE, &rndr->persist_fl, "imgui");

    // Use the main forward pass for imgui.. this might only change if we use deferred shading.. but i think the imgui
    // created pipeling only requires a color attachment
    rndr->imgui.rpass = (VkRenderPass)pass.vk_handle;

    ImGui::SetAllocatorFunctions(imgui_mem_alloc, imgui_mem_free, &rndr->imgui.fl);
    rndr->imgui.ctxt = ImGui::CreateContext();
    ImGui::StyleColorsDark();
    auto &io = ImGui::GetIO();
    io.FontGlobalScale = get_window_display_scale(rndr->vk.cfg.window);

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

    set_platform_sdl_event_hook(rndr->vk.cfg.window, {.cb = sdl_event_func});
}

void terminate_imgui(renderer *rndr)
{
    if (rndr->imgui.fl.start) {
        ilog("Shutting down imgui");
        ImGui_ImplVulkan_Shutdown();
        vkr_terminate_desc_pool(rndr->imgui.pool, &rndr->vk);
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(rndr->imgui.ctxt);
        terminate_arena(&rndr->imgui.fl);
    }
    else {
        ilog("Skipping imgui shutdown as already shut down");
    }
}
#endif

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

intern bool destroy_geometry(rgeom_ref gref, renderer *rndr)
{
    vmaVirtualFree(gref.item->vert_block, gref.item->vert_mem);
    vmaVirtualFree(gref.item->ind_block, gref.item->ind_mem);
    *gref.item = {};
    return release_slot(&rndr->geometry, gref.hndl);
}

intern int record_command_buffer(renderer *rndr, vkr_framebuffer *, frame_context *cur_frame)
{
    PROFILE_SCOPE("record_command_buffer");
    auto dev = &rndr->vk.inst.device;

    // int err = vkr_begin_cmd_buf(cur_frame->cmd_buffer, {});
    // if (err != err_code::VKR_NO_ERROR) {
    //     return err;
    // }

    // VkClearValue att_clear_vals[] = {{.color{{0.05f, 0.05f, 0.05f, 1.0f}}}, {.depthStencil{1.0f, 0}}};

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
    // for (int rpind = 0; rpind < rndr->rpasses.size; ++rpind) {
    //     auto rpass = &rndr->rpasses[rpind];
    //     vkr_cmd_begin_rpass(cur_frame->cmd_buffer, rpass->vk_hndl, fb, att_clear_vals, 2);

    //     VkViewport viewport{};
    //     viewport.x = 0.0f;
    //     viewport.y = 0.0f;
    //     viewport.width = (float)fb->size.w;
    //     viewport.height = (float)fb->size.h;
    //     viewport.minDepth = 0.0f;
    //     viewport.maxDepth = 1.0f;
    //     vkCmdSetViewport(cur_frame->cmd_buffer, 0, 1, &viewport);

    //     VkRect2D scissor{};
    //     scissor.offset = {0, 0};
    //     scissor.extent = {fb->size.w, fb->size.h};
    //     vkCmdSetScissor(cur_frame->cmd_buffer, 0, 1, &scissor);

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
    // #ifdef USE_IMGUI
    //     if (rpass->vk_hndl == rndr->imgui.rpass) {
    //         auto img_data = ImGui::GetDrawData();
    //         ImGui_ImplVulkan_RenderDrawData(img_data, cur_frame->cmd_buffer);
    //     }
    // #endif

    //     vkr_cmd_end_rpass(cur_frame->cmd_buffer);
    // }

    return 0; // vkr_end_cmd_buf(cur_frame->cmd_buffer);
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
    case rformat::BGRA8_SRGB:
    case rformat::BGRA8_UNORM:
    case rformat::BGRA8_SNORM:
    case rformat::BGRA8_UINT:
    case rformat::BGRA8_SINT:
    case rformat::ABGR8_SRGB:
    case rformat::ABGR8_UNORM:
    case rformat::ABGR8_SNORM:
    case rformat::ABGR8_UINT:
    case rformat::ABGR8_SINT:
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
    case rformat::BGR8_SRGB:
    case rformat::BGR8_UNORM:
    case rformat::BGR8_SNORM:
    case rformat::BGR8_UINT:
    case rformat::BGR8_SINT:
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

        strncpy(cur_buffer->name, cur_stream_desc->dbg_name, SMALL_STR_LEN - 1);

        alloc_cfg.buffer_size = desc.max_vert_count * cur_binding->stride;
        alloc_cfg.user_data = cur_buffer->name;

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

intern void release_geometry_stream_group(geom_streams_group *gp, const vkr_context *vk)
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

intern void terminate_framebuffers_with_image(renderer *rndr, VkImageView iv)
{
    for (auto sliter = slot_pool_begin(&rndr->fb_cache.items); is_valid(sliter); sliter = slot_pool_next(&rndr->fb_cache.items, sliter)) {
        // If it had one, we delete it and continue, otherwise we leave it alone and continue
        if (arr_find(&sliter.item->gpu_d.kd.atts, iv)) {
            ilog("Destroying framebuffer %p for image view %p", sliter.item->gpu_d.hndl, iv);
            vkr_terminate_framebuffer(&sliter.item->gpu_d, &rndr->vk);
            hmap_remove(&rndr->fb_cache.key_lut, sliter.item->key);
            release_slot(&rndr->fb_cache.items, sliter.hndl);
        }
    }
}

intern void terminate_swapchain_framebuffers(renderer *rndr)
{
    auto vk_sw = &rndr->vk.inst.device.swapchain;
    for (u32 swap_i = 0; swap_i < vk_sw->image_views.size; ++swap_i) {
        terminate_framebuffers_with_image(rndr, vk_sw->image_views[swap_i]);
    }
}

intern void handle_window_resize(renderer *rndr)
{
    ilog("Recreating swapchain");
    // Recreating the swapchain will wait on all semaphores and fences before continuing
    auto dev = &rndr->vk.inst.device;
    vkr_device_wait_idle(dev);
    terminate_swapchain_framebuffers(rndr);
    vkr_terminate_swapchain(&dev->swapchain, &rndr->vk);
    vkr_terminate_surface(&rndr->vk, rndr->vk.inst.surface);
    vkr_init_surface(&rndr->vk, &rndr->vk.inst.surface);
    vkr_init_swapchain(&dev->swapchain, &rndr->vk);

    for (auto rt_iter = slot_pool_begin(&rndr->rtargets.textures); is_valid(rt_iter);
         rt_iter = slot_pool_next(&rndr->rtargets.textures, rt_iter)) {
        if (rt_iter.item->id != SWAPCHAIN_ID && test_flags(rt_iter.item->flags, RTARGET_TEXTURE_FLAG_RESIZE_WITH_WINDOW)) {
            rt_iter.item->cfg.dims = {dev->swapchain.extent.width, dev->swapchain.extent.height, 1};
            ilog("Resizing %s to {%u %u}", rt_iter.item->name, rt_iter.item->cfg.dims.x, rt_iter.item->cfg.dims.y);
            asrt(!rt_iter.item->iv_cfg.image);
            for (u32 fif = 0; fif < MAX_FRAMES_IN_FLIGHT; ++fif) {
                auto cur_i = &rt_iter.item->frames[fif];

                // Terminate any framebuffers associated with this image view
                // If the image is in a framebuffer with the swapchain image then it will have already have been
                // destroyed during the terminate swapchain framebuffers
                terminate_framebuffers_with_image(rndr, cur_i->view);

                vkr_terminate_image(&cur_i->image, &rndr->vk);
                vkr_terminate_image_view(cur_i->view, &rndr->vk);
                cur_i->image = {};
                cur_i->view = {};
                int result = vkr_init_image(&cur_i->image, rt_iter.item->cfg);
                asrt(result == err_code::VKR_NO_ERROR);

                // Update the image ptr
                rt_iter.item->iv_cfg.image = &cur_i->image;
                result = vkr_init_image_view(&cur_i->view, rt_iter.item->iv_cfg, &rndr->vk);
                asrt(result == err_code::VKR_NO_ERROR);
            }

            // Restore the image pointer to null
            rt_iter.item->iv_cfg.image = nullptr;
        }
    }
}

intern int init_frame_contexts(renderer *rndr, sizet thread_cnt)
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
        arr_init(&cur_fif->thread_pools, &rndr->persist_fl, thread_cnt);
        arr_resize(&cur_fif->thread_pools, thread_cnt);
        for (u32 i = 0; i < cur_fif->thread_pools.size; ++i) {
            result = vkr_init_cmd_pool(&cur_fif->thread_pools[i].pool,
                                       dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].fam_ind,
                                       VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                       &rndr->vk);
            if (result != err_code::VKR_NO_ERROR) {
                return result;
            }

            vkr_alloc_cmd_bufs_cfg buf_cfgs{};
            buf_cfgs.count = 1;
            buf_cfgs.pool = cur_fif->thread_pools[i].pool;
            result = vkr_alloc_cmd_bufs(&cur_fif->thread_pools[i].buf, buf_cfgs, &rndr->vk);
            if (result != err_code::VKR_NO_ERROR) {
                return result;
            }
        }
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
        for (u32 i = 0; i < cur_fif->thread_pools.size; ++i) {
            vkr_terminate_cmd_pool(cur_fif->thread_pools[i].pool, &rndr->vk);
        }
        arr_terminate(&cur_fif->thread_pools);
    }
    arr_clear(&rndr->fifs);
}

intern void init_resource_target_registry(renderer *rndr)
{
    ilog("Initializing render memory");
    init_slot_pool(&rndr->rtargets.textures, MAX_TEXTURE_TARGET_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->rtargets.buffers, MAX_BUFFER_TARGET_COUNT, &rndr->persist_fl);
    // Load factor is .75 so two times size should make so table is never rehashed and still performant
    hmap_init(&rndr->rtargets.texture_id_map, hash_type, &rndr->persist_fl, MAX_TEXTURE_TARGET_COUNT * 2);
    hmap_init(&rndr->rtargets.buffer_id_map, hash_type, &rndr->persist_fl, MAX_BUFFER_TARGET_COUNT * 2);

    // We reserve the first render texture target as the swapchain image - and update the current fif texture to
    // reference the swapchain image at the start of every frame once we acquire it
    auto swapchain = acquire_slot(&rndr->rtargets.textures);
    strcpy(swapchain.item->name, "swapchain");
    swapchain.item->id = hash_type("swapchain");
    hmap_insert(&rndr->rtargets.texture_id_map, swapchain.item->id, swapchain.hndl);
}

// We assume device has already been waited here
intern void terminate_resource_target_registry(renderer *rndr)
{
    ilog("Terminating render targets");
    auto vk = &rndr->vk;
    for (u32 fif = 0; fif < MAX_FRAMES_IN_FLIGHT; ++fif) {
        for (auto iter = slot_pool_begin(&rndr->rtargets.textures); is_valid(iter); iter = slot_pool_next(&rndr->rtargets.textures, iter)) {
            if (iter.item->id != SWAPCHAIN_ID) {
                ilog("Terminating %s for FIF %d", iter.item->name, fif);
                vkr_terminate_image_view(iter.item->frames[fif].view, vk);
                vkr_terminate_image(&iter.item->frames[fif].image, vk);
                iter.item->frames[fif] = {};
            }
            else {
                ilog("Skipping %s for FIF %d", iter.item->name, fif);
            }
        }
        for (auto iter = slot_pool_begin(&rndr->rtargets.buffers); is_valid(iter); iter = slot_pool_next(&rndr->rtargets.buffers, iter)) {
            ilog("Terminating %s for FIF %d", iter.item->name, fif);
            vkr_terminate_buffer(&iter.item->frames[fif].buffer, vk);
            iter.item->frames[fif] = {};
        }
    }
    terminate_slot_pool(&rndr->rtargets.textures);
    hmap_terminate(&rndr->rtargets.texture_id_map);
    terminate_slot_pool(&rndr->rtargets.buffers);
    hmap_terminate(&rndr->rtargets.buffer_id_map);
}

intern int init_global_descriptor_set_layouts(renderer *rndr)
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

intern void terminate_global_descriptor_set_layouts(renderer *rndr)
{
    // Terminate our default descriptor layout sets
    dlog("Should be terminating %d layouts", rndr->set_layouts.size);
    vkr_terminate_desc_set_layouts(rndr->set_layouts.data, rndr->set_layouts.size, &rndr->vk);
    arr_clear(&rndr->set_layouts);
}

intern int init_global_samplers(renderer *rndr)
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

intern void terminate_global_samplers(renderer *rndr)
{
    // Terminate all texture samplers
    for (u32 i = 0; i < rndr->samplers.size; ++i) {
        vkr_terminate_sampler(rndr->samplers[i].vk_hndl, &rndr->vk);
    }
    arr_clear(&rndr->samplers);
}

intern void init_geometry_stream_groups(renderer *rndr)
{
    // Geometry index/vertex buffers
    asrt(rndr->geom_groups.size == 0);
    hmap_init(&rndr->geom_group_id_map, hash_type, &rndr->persist_fl);
}

intern void terminate_geometry_stream_groups(renderer *rndr)
{
    // Remove source geometry buffers
    for (u32 i = 0; i < rndr->geom_groups.size; ++i) {
        release_geometry_stream_group(&rndr->geom_groups[i], &rndr->vk);
    }
    rndr->geom_groups.size = 0;
    hmap_terminate(&rndr->geom_group_id_map);
}

intern void init_render_resources(renderer *rndr)
{
    init_slot_pool(&rndr->techniques, MAX_TECHNIQUE_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->materials, MAX_MATERIAL_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->textures, MAX_TEXTURE_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->geometry, MAX_MESH_COUNT, &rndr->persist_fl);
}

intern void terminate_render_resources(renderer *rndr)
{
    // Terminate all meshes
    for (auto iter = slot_pool_begin(&rndr->geometry); is_valid(iter); iter = slot_pool_next(&rndr->geometry, iter)) {
        destroy_geometry(iter, rndr);
    }
    terminate_slot_pool(&rndr->geometry);

    // Terminate all images and image views
    for (auto iter = slot_pool_begin(&rndr->textures); is_valid(iter); iter = slot_pool_next(&rndr->textures, iter)) {
        vkr_terminate_image(&iter.item->im, &rndr->vk);
        vkr_terminate_image_view(iter.item->im_view, &rndr->vk);
    }
    terminate_slot_pool(&rndr->textures);

    // Materials
    for (auto iter = slot_pool_begin(&rndr->materials); is_valid(iter); iter = slot_pool_next(&rndr->materials, iter)) {
        // Do something
    }
    terminate_slot_pool(&rndr->materials);

    // Techniques
    for (auto iter = slot_pool_begin(&rndr->techniques); is_valid(iter); iter = slot_pool_next(&rndr->techniques, iter)) {
        // Do something
    }
    terminate_slot_pool(&rndr->techniques);
}

intern int init_global_pipeline_layout(renderer *rndr)
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

intern void terminate_global_pipeline_layout(renderer *rndr)
{
    vkr_terminate_pipeline_layout(rndr->g_layout, &rndr->vk);
}

template<typename T>
void init_gpu_resource_cache(renderer *rndr, gpu_resource_cache<T> *cache, u32 elements)
{
    init_slot_pool(&cache->items, elements, &rndr->persist_fl);
    hmap_init(&cache->key_lut, hash_type, &rndr->persist_fl, elements * 2);
}

template<typename T, typename TermFunc>
intern void terminate_gpu_resource_cache(renderer *rndr, gpu_resource_cache<T> *cache, TermFunc term_func, const char *lname)
{
    ilog("Terminating %u used %s (%u total size with %u free slots)",
         cache->items.used_count,
         lname,
         cache->items.slots.size,
         cache->items.free_list.size);
    for (auto sliter = slot_pool_begin(&cache->items); is_valid(sliter); sliter = slot_pool_next(&cache->items, sliter)) {
        term_func(&sliter.item->gpu_d);
    }
    terminate_slot_pool(&cache->items);
    hmap_terminate(&cache->key_lut);
}

intern const vkr_framebuffer *get_or_create_framebuffer(framebuffer_cache *cache, const vkr_framebuffer_key_data &kd, const vkr_context *vk)
{
    u64 hash = hash_type((const char *)&kd, sizeof(vkr_framebuffer_key_data));
    auto fiter = hmap_find(&cache->key_lut, hash);
    if (fiter) {
        auto slitem = get_slot_item(&cache->items, fiter->val);
        asrt(slitem);
        return &slitem->gpu_d;
    }
    ilog("Creating new framebuffer for unique hash %lu", hash);
    auto new_slot = acquire_slot(&cache->items);
    asrt(is_valid(new_slot) && "Out of framebuffer slots");
    vkr_framebuffer_cfg cfg{.kd = kd};
    int result = vkr_init_framebuffer(&new_slot.item->gpu_d, cfg, vk);
    asrt(result == err_code::VKR_NO_ERROR);
    hmap_insert(&cache->key_lut, hash, new_slot.hndl);
    return &new_slot.item->gpu_d;
}

intern void init_blueprints(renderer *rndr)
{
    hmap_init(&rndr->blueprint_id_map, hash_type, &rndr->persist_fl);
    init_slot_pool(&rndr->blueprints, MAX_BP_COUNT, &rndr->persist_fl);
}

intern void terminate_blueprints(renderer *rndr)
{
    while (!slot_pool_empty(&rndr->blueprints)) {
        destroy_render_blueprint(rndr, slot_pool_begin(&rndr->blueprints).hndl);
    }
    terminate_slot_pool(&rndr->blueprints);
    hmap_terminate(&rndr->blueprint_id_map);
}

int init_renderer(renderer *rndr, const init_renderer_params &p)
{
    asrt(p.upsream->alloc_type != mem_alloc_type::POOL); // Cannot use pool arena here
    init_fl_arena(&rndr->persist_fl, p.persist_fl_size, p.upsream, "rndr-persist-fl");
    init_lin_arena(&rndr->persist_stack, p.persist_stack_size, p.upsream, "rndr-persist-stack");
    init_lin_arena(&rndr->frame_linear, p.frame_linear_size, p.upsream, "rndr-frame-linear");

    init_fl_arena(&rndr->vk_free_list, 50 * MB_SIZE, &rndr->persist_fl, "rndr-vk-fl");
    init_lin_arena(&rndr->vk_frame_linear, 10 * MB_SIZE, &rndr->persist_fl, "rndr-vk-frame");

    // Vulkan
    vkr_cfg vkii{.app_name = "rdev",
                 .vi{1, 0, 0},
                 .arenas{.persistent_arena = &rndr->vk_free_list, .command_arena = &rndr->vk_frame_linear},
                 .log_verbosity = LOG_DEBUG,
                 .window = p.win_hndl,
                 .inst_create_flags = INST_CREATE_FLAGS,
                 .extra_instance_extension_names = ADDITIONAL_INST_EXTENSIONS,
                 .extra_instance_extension_count = ADDITIONAL_INST_EXTENSION_COUNT,
                 .device_extension_names = DEVICE_EXTENSIONS,
                 .device_extension_count = DEVICE_EXTENSION_COUNT,
                 .validation_layer_names = VALIDATION_LAYERS,
                 .validation_layer_count = VALIDATION_LAYER_COUNT};
    s32 result = vkr_init(&vkii, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        return err_code::RENDER_INIT_FAIL;
    }

    // Swapchain image handling...

    // Blueprints
    init_blueprints(rndr);

    // Pipeline cache (no pipelines yet)
    init_gpu_resource_cache(rndr, &rndr->pline_cache, 32);

    // Framebuffer cache (no framebuffers yet)
    init_gpu_resource_cache(rndr, &rndr->fb_cache, 16);

    // Geometry stream groups
    init_geometry_stream_groups(rndr);

    // Render resources
    init_render_resources(rndr);

    // Resource targets
    init_resource_target_registry(rndr);

    // Create transient command pool
    vkr_init_cmd_pool(&rndr->transient_pool,
                      rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].fam_ind,
                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                      &rndr->vk);

    // Setup frames in flight
    result = init_frame_contexts(rndr, p.thread_count);
    if (result != err_code::VKR_NO_ERROR) {
        elog("Failed to setup frame contexts");
        return result;
    }

    // Descriptor set layouts
    result = init_global_descriptor_set_layouts(rndr);
    if (result != err_code::VKR_NO_ERROR) {
        elog("Failed to setup global descriptor set layouts");
        return result;
    }

    // Pipeline layout
    result = init_global_pipeline_layout(rndr);
    if (result != err_code::VKR_NO_ERROR) {
        elog("Failed to setup global pipeline layout");
        return result;
    }

    // Samplers
    result = init_global_samplers(rndr);
    if (result != err_code::VKR_NO_ERROR) {
        elog("Failed to setup global samplers");
        return result;
    }

    // Start timeer
    ptimer_restart(&rndr->pt);

    // Setup our indice and vert buffer sbuffer
    return err_code::RENDER_NO_ERROR;
}

void terminate_renderer(renderer *rndr)
{
    ilog("Terminating");
    reset_arena(&rndr->vk_frame_linear);
    reset_arena(&rndr->frame_linear);

    // Device needs to be idle before finishing with everything
    vkr_device_wait_idle(&rndr->vk.inst.device);

// IMGUI
#ifdef USE_IMGUI
    terminate_imgui(rndr);
#endif

    // Global samplers
    terminate_global_samplers(rndr);

    // Global pipeline layout
    terminate_global_pipeline_layout(rndr);

    // Global descriptor set layouts
    terminate_global_descriptor_set_layouts(rndr);

    // Frame context
    terminate_frame_contexts(rndr);

    // Transient pool
    vkr_terminate_cmd_pool(rndr->transient_pool, &rndr->vk);

    // Resource targets
    terminate_resource_target_registry(rndr);

    // All textures/geometries/techniques/materials/textures
    terminate_render_resources(rndr);

    // Geometry vert/index buffers
    terminate_geometry_stream_groups(rndr);

    // Framebuffer cache and all created framebuffers
    terminate_gpu_resource_cache(
        rndr, &rndr->fb_cache, [rndr](vkr_framebuffer *fb) { vkr_terminate_framebuffer(fb, &rndr->vk); }, "framebuffers");

    // Pipeline cache and all created pipelines
    terminate_gpu_resource_cache(
        rndr, &rndr->pline_cache, [rndr](gpu_handle *pl) { vkr_terminate_pipeline(*((VkPipeline *)pl), &rndr->vk); }, "pipelines");

    // Blueprints
    terminate_blueprints(rndr);

    // Vulkan
    vkr_terminate(&rndr->vk);

    terminate_arena(&rndr->vk_free_list);
    terminate_arena(&rndr->vk_frame_linear);

    // Preserve this order just in case the passed in arena was a stack arena
    terminate_arena(&rndr->frame_linear);
    terminate_arena(&rndr->persist_stack);
    terminate_arena(&rndr->persist_fl);
}

u32 push_geometry_stream_group(renderer *rndr, const geometry_stream_group_desc &desc)
{
    asrt(desc.max_ind_count > 0);
    asrt(desc.layouts.size > 0);
    asrt(desc.layouts[0].streams.size > 0);
    asrt(desc.layouts[0].streams[0].attribs.size > 0);
    asrt(rndr->geom_groups.size <= rndr->geom_groups.capacity);
    u32 geom_id = (u32)rndr->geom_groups.size++;
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
        strncpy(cur_group->indice_stream.name, desc.name, SMALL_STR_LEN - 1);

        vkr_buffer_cfg alloc_cfg{};
        alloc_cfg.alloc_flags = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        alloc_cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;
        alloc_cfg.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        alloc_cfg.buffer_size = desc.max_ind_count * sizeof(ind_t);
        alloc_cfg.user_data = cur_group->indice_stream.name;
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
        release_geometry_stream_group(cur_group, &rndr->vk);
        --rndr->geom_groups.size;
        return INVALID_ID;
    }
    cur_group->id = hash_type(cur_group->indice_stream.name);
    hmap_insert(&rndr->geom_group_id_map, cur_group->id, geom_id);
    return geom_id;
}

geometry_vert_layout_desc *push_geometry_layout(geometry_stream_group_desc *desc, u32 layout_max_vert_count)
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

rgeom_handle create_geometry(renderer *rndr, const rgeom_desc &ci)
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

    rgeom_ref geom_ref = acquire_slot(&rndr->geometry);
    if (!is_valid(geom_ref)) {
        wlog("Out of geometry slots");
        return {};
    }

    // Set the vert/ind blocks
    geom_ref.item->vert_block = layout->vert_block;
    geom_ref.item->ind_block = gp->indices_block;

    // Set the name if it was filled in
    strncpy(geom_ref.item->name, ci.name ? ci.name : "unnamed", SMALL_STR_LEN - 1);

    // Copy subgeom data
    geom_ref.item->subgeom_vert_ind_counts.size = ci.subgeom_cnt;
    for (u32 i = 0; i < ci.subgeom_cnt; ++i) {
        geom_ref.item->subgeom_vert_ind_counts[i] = ci.subgeoms[i];
    }

    // This info is shared between the vert stream and ind stream virtual alloc
    VmaVirtualAllocationCreateInfo alloc_ci{};
    alloc_ci.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    alloc_ci.pUserData = geom_ref.item->name;

    // Create the virtual allocation using vert stream 0 which dictates the vert offset in to each stream
    alloc_ci.alignment = layout->vert_layout.bindings[0].stride;
    alloc_ci.size = ci.vert_count * alloc_ci.alignment;
    VkDeviceSize vert_stream_byte_offset{};
    s32 result = vmaVirtualAllocate(geom_ref.item->vert_block, &alloc_ci, &geom_ref.item->vert_mem, &vert_stream_byte_offset);
    if (result != err_code::VKR_NO_ERROR) {
        wlog("Vma virtual allocate for vert stream failed with code %d", result);
        asrt(destroy_geometry(geom_ref, rndr));
        return {};
    }
    asrt(vert_stream_byte_offset % alloc_ci.alignment == 0);
    geom_ref.item->vert_offset = vert_stream_byte_offset / alloc_ci.alignment;

    // Create the virtual allocation for indices
    alloc_ci.alignment = sizeof(ind_t);
    alloc_ci.size = ci.ind_count * alloc_ci.alignment;
    VkDeviceSize ind_stream_byte_offset{};
    result = vmaVirtualAllocate(geom_ref.item->ind_block, &alloc_ci, &geom_ref.item->ind_mem, &ind_stream_byte_offset);
    if (result != err_code::VKR_NO_ERROR) {
        wlog("Vma virtual allocate indices stream failed with code %d", result);
        asrt(destroy_geometry(geom_ref, rndr));
        return {};
    }
    asrt(ind_stream_byte_offset % alloc_ci.alignment == 0);
    geom_ref.item->ind_offset = ind_stream_byte_offset / alloc_ci.alignment;

    // Create staging buffer and get the queue we will use
    VkCommandBuffer tmp_cmd_buf{};
    result = vkr_alloc_cmd_bufs(&tmp_cmd_buf, {.pool = rndr->transient_pool}, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        wlog("Failed to create command buffer - error code: %d", result);
        asrt(destroy_geometry(geom_ref, rndr));
        return {};
    }
    VkQueue tmp_q = rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE];

    // Copy data for the vert buffers
    for (u32 streami = 0; streami < layout->vert_streams.size; ++streami) {
        VkBufferCopy region{};
        region.size = ci.vert_count * layout->vert_layout.bindings[streami].stride;
        region.dstOffset = geom_ref.item->vert_offset * layout->vert_layout.bindings[streami].stride;
        result = vkr_stage_and_upload_buffer_data(
            &layout->vert_streams[streami].buffer, ci.vert_data[streami], &region, 1, tmp_cmd_buf, tmp_q, &rndr->vk);
        if (result != err_code::VKR_NO_ERROR) {
            asrt(destroy_geometry(geom_ref, rndr));
            vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
            return {};
        }
    }

    VkBufferCopy region{};
    region.size = ci.ind_count * sizeof(ind_t);
    region.dstOffset = geom_ref.item->ind_offset * sizeof(ind_t);
    result = vkr_stage_and_upload_buffer_data(&gp->indice_stream.buffer, ci.ind_data, &region, 1, tmp_cmd_buf, tmp_q, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        asrt(destroy_geometry(geom_ref, rndr));
        vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
        geom_ref = {};
    }
    return geom_ref.hndl;
}

rtexture_handle create_texture(renderer *rndr, const rtexture_desc &ctinfo)
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

    rtexture_ref tref = acquire_slot(&rndr->textures);
    if (!is_valid(tref)) {
        vkr_terminate_image(&ti.im, &rndr->vk);
        vkr_terminate_image_view(ti.im_view, &rndr->vk);
        return {};
    }
    asrt(tref.item);
    *tref.item = ti;
    return tref.hndl;
}

rtechnique_handle create_rtechnique(renderer *rndr, const rtechnique_desc &ctinfo)
{
    return {};
}

rmaterial_handle create_material(renderer *rndr, const rmaterial_desc &ctinfo)
{
    return {};
}

rformat get_swapchain_format(renderer *rnd)
{
    return get_rformat(rnd->vk.inst.device.swapchain.format);
}

rtexture_target_handle create_rtexture_target(renderer *rndr, const rtexture_target_desc &ci)
{
    rtexture_target_ref tref = acquire_slot(&rndr->rtargets.textures);
    if (!is_valid(tref)) {
        return {};
    }

    strncpy(tref.item->name, ci.name, SMALL_STR_LEN - 1);
    tref.item->id = hash_type(tref.item->name);
    tref.item->flags = ci.flags;

    // If parameter not here its cause I want to leave at default on purpose
    tref.item->cfg.format = get_vk_format(ci.format);
    bool is_color = (ci.type == RTARGET_TEXTURE_TYPE_COLOR || ci.type == RTARGET_TEXTURE_TYPE_CUBE_COLOR);
    tref.item->cfg.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | (is_color ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    tref.item->cfg.im_create_flags = ci.type > RTARGET_TEXTURE_TYPE_DEPTH ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    tref.item->cfg.dims = {ci.dims == svec2{} ? get_window_pixel_size(rndr->vk.cfg.window) : ci.dims, 1u};
    tref.item->cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    tref.item->cfg.alloc_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    tref.item->cfg.array_layers = ci.type > RTARGET_TEXTURE_TYPE_DEPTH ? 6u : 1u;
    tref.item->cfg.priority = 1.0f;
    tref.item->cfg.user_data = tref.item->name;
    tref.item->cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;

    tref.item->iv_cfg.view_type = ci.type > RTARGET_TEXTURE_TYPE_DEPTH ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    tref.item->iv_cfg.srange.aspectMask = is_color ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        int result = vkr_init_image(&tref.item->frames[i].image, tref.item->cfg);
        asrt(result == err_code::VKR_NO_ERROR);
        tref.item->iv_cfg.image = &tref.item->frames[i].image;
        result = vkr_init_image_view(&tref.item->frames[i].view, tref.item->iv_cfg, &rndr->vk);
        asrt(result == err_code::VKR_NO_ERROR);
    }

    // So we don't forget to update this to each per fif image on doing any resizes
    tref.item->iv_cfg.image = nullptr;

    hmap_insert(&rndr->rtargets.texture_id_map, tref.item->id, tref.hndl);
    return tref.hndl;
}

rtexture_target_handle find_rtexture_target(renderer *rndr, rres_id id)
{
    auto fiter = hmap_find(&rndr->rtargets.texture_id_map, id);
    return fiter ? fiter->val : rtexture_target_handle{};
}

rtexture_target *get_rtexture_target(renderer *rndr, rtexture_target_handle hndl)
{
    return get_slot_item(&rndr->rtargets.textures, hndl);
}

rbuffer_target_handle create_rbuffer_target(renderer *rndr, const rbuffer_target_desc &ci)
{
    rbuffer_target_ref btref = acquire_slot(&rndr->rtargets.buffers);
    if (!is_valid(btref)) {
        return {};
    }
    strncpy(btref.item->name, ci.name, SMALL_STR_LEN - 1);
    btref.item->id = hash_type(btref.item->name);
    // TODO: Don't actually just have cfg in the ci - edit this to only include actual needed things - for now we just
    // use it
    auto cfg = ci.cfg;
    cfg.user_data = btref.item->name;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        int result = vkr_init_buffer(&btref.item->frames[i].buffer, cfg);
        asrt(result == err_code::VKR_NO_ERROR);
    }
    hmap_insert(&rndr->rtargets.buffer_id_map, btref.item->id, btref.hndl);
    return btref.hndl;
}

rbuffer_target *get_rbuffer_target(renderer *rndr, rbuffer_target_handle hndl)
{
    return get_slot_item(&rndr->rtargets.buffers, hndl);
}

rbuffer_target_handle find_rtarget_buffer(renderer *rndr, rres_id id)
{
    auto fiter = hmap_find(&rndr->rtargets.buffer_id_map, id);
    return fiter ? fiter->val : rbuffer_target_handle{};
}

// We can let this "leak" as it doesn't leak due to using frame linear allocator
intern rmanifest *create_manifest(renderer *rndr)
{
    rmanifest *m = mem_calloc<rmanifest>(1, &rndr->frame_linear);
    arr_init(&m->jobs, &rndr->frame_linear, 24);
    arr_init(&m->passes, &rndr->frame_linear, 12);
    arr_init(&m->views, &rndr->frame_linear, 12);

    // Initialize our manifest textures and buffers with the global ones (well, globabl to the renderer)
    arr_init(&m->textures, &rndr->frame_linear);
    arr_resize(&m->textures, rndr->rtargets.textures.slots.size);
    for (u32 i = 0; i < m->textures.size; ++i) {
        m->textures[i] = rndr->rtargets.textures.slots[i].item;
    }

    // Buffers
    arr_init(&m->buffers, &rndr->frame_linear);
    arr_resize(&m->buffers, rndr->rtargets.buffers.slots.size);
    for (u32 i = 0; i < m->buffers.size; ++i) {
        m->buffers[i] = rndr->rtargets.buffers.slots[i].item;
    }

    return m;
}

intern u32 get_fif_ind(renderer *rndr)
{
    return rndr->finished_frames % MAX_FRAMES_IN_FLIGHT;
}

intern bool window_resize_continue_check(renderer *rndr, frame_context *cur_fif)
{
    if (window_resized_this_frame(rndr->vk.cfg.window)) {
        cur_fif->swapchain_resize = WINDOW_RESIZE_DEBOUNCE_DURATION;
    }

    if (cur_fif->swapchain_resize > 0.0f) {
        cur_fif->swapchain_resize -= rndr->pt.dt;
        if (cur_fif->swapchain_resize > 0.0f) {
            return false;
        }
        handle_window_resize(rndr);
        cur_fif->swapchain_resize = false;
    }
    return true;
}

rmanifest *begin_render_frame(renderer *rndr, render_blueprint_handle bp)
{
    PROFILE_SCOPE("begin_render_frame");
    ptimer_split(&rndr->pt);
    auto dev = &rndr->vk.inst.device;

    // Update finished frames which is used to get the current frame
    u32 fif = get_fif_ind(rndr);
    auto *cur_fif = &rndr->fifs[fif];

    // Window resize
    if (!window_resize_continue_check(rndr, cur_fif)) {
        return nullptr;
    }

    // We wait until this FIF's fence has been triggered before rendering the frame. FIF fences are created in a
    // triggered state so there will be no waiting on the first time. We then reset the fence (aka set it to
    // untriggered) and it is passed to the vkQueueSubmit call to trigger it again. So if not the first time rendering
    // this FIF, we are waiting for the vkQueueSubmit from the previous time this FIF was rendered to complete
    int vk_res = vkWaitForFences(dev->hndl, 1, &cur_fif->in_flight, VK_TRUE, UINT64_MAX);
    asrt(vk_res == VK_SUCCESS);

    /////////////////////////////////
    // Acquire Swapchain Image Ind //
    /////////////////////////////////
    // Acquire the image, signal the image_avail semaphore once the image has been acquired. We get the index back, but
    // that doesn't mean the image is ready. The image is only ready (on the GPU side) once the image avail semaphore is triggered
    vk_res =
        vkAcquireNextImageKHR(dev->hndl, dev->swapchain.swapchain, UINT64_MAX, cur_fif->image_avail, VK_NULL_HANDLE, &cur_fif->cur_im_ind);

    // If the image is out of date we need to recreate the swapchain and our caller needs to exit early as
    // well. It seems that on some platforms, if the result from above is out of date or suboptimal, the semaphore
    // associated with it will never get triggered. So if we were to continue and just resize at the end of frame it
    // wouldn't work because the queue submit would never fire as it depends on this image available semaphore.
    // At least.. i think?
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR) {
        cur_fif->swapchain_resize = WINDOW_RESIZE_DEBOUNCE_DURATION;
        return nullptr;
    }
    asrt(vk_res == VK_SUCCESS || vk_res == VK_SUBOPTIMAL_KHR);

    reset_arena(&rndr->vk_frame_linear);
    reset_arena(&rndr->frame_linear);

    // Reset command pool
    for (u32 ti = 0; ti < cur_fif->thread_pools.size; ++ti) {
        vk_res = vkResetCommandPool(dev->hndl, cur_fif->thread_pools[ti].pool, {});
        asrt(vk_res == VK_SUCCESS);
    }

    /////////////////////
    // Reset FIF Fence //
    /////////////////////
    // Here we reset the fence for the current frame fence as we know we are going to call queue submit which is the
    // only thing that will trigger the fence - so this is why this reset needs to come here (rather than right after
    // waiting) because if we return early due to swapchain resize, and we had reset the fence, then the next time our
    // frame came around we would just be stuck waiting forever
    vk_res = vkResetFences(dev->hndl, 1, &cur_fif->in_flight);
    asrt(vk_res == VK_SUCCESS);

    // Update our special swapchain handle
    auto sw = &rndr->vk.inst.device.swapchain;
    auto sw_hndl = find_rtexture_target(rndr, SWAPCHAIN_ID);
    auto swapchain = get_rtexture_target(rndr, sw_hndl);
    swapchain->frames[fif].view = sw->image_views[cur_fif->cur_im_ind];
    swapchain->frames[fif].image = sw->images[cur_fif->cur_im_ind];
    swapchain->frames[fif].state = {};

// Start GUI frame
#ifdef USE_IMGUI
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#endif

    rmanifest *m = create_manifest(rndr);
    m->rndr = rndr;
    m->rbp = bp;

    return m;
}

#if defined USE_IMGUI
void draw_imgui(const render_job_cb_params &p, void *)
{
    auto img_data = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(img_data, (VkCommandBuffer)p.cmd_buf);
}
#endif

void draw_geometry(const render_job_cb_params &, void *);

// Create a framebuffer key from the passed in pass attachments and slot assignments. This key is used to get or create
// framebuffers with this specific set of image views, allowing us to lazily create them
intern void setup_framebuffer_key_and_clear_vals(vkr_framebuffer_key_data *fb_kd,
                                                 VkClearValue *cv,
                                                 sizet *cv_size,
                                                 VkRenderPass vk_rpass,
                                                 const mpass &mp,
                                                 const rbp_pass &rbp_pass,
                                                 const rmanifest &m,
                                                 u32 fif)
{
    u32 att_cnt{};
    fb_kd->layers = 1;
    fb_kd->rpass = vk_rpass;
    for (u32 si = 0; si < mp.slot_assignments.size; ++si) {
        auto cur_sl = &mp.slot_assignments[si];
        bool t_cond = cur_sl->type == mslot_target_type::TEXTURE && is_valid(cur_sl->t);
        bool b_cond = cur_sl->type == mslot_target_type::BUFFER && is_valid(cur_sl->b);
        asrt(t_cond || b_cond);

        // If is attachment, we add to framebuffer
        u32 att_ind = rbp_pass.slots[si].att_ind;
        if (is_valid(att_ind)) {
            asrt(att_ind < fb_kd->atts.capacity);
            const rtexture_target *cur_t = &m.textures[cur_sl->t.index];
            VkImageView tview = cur_t->frames[fif].view;

            // Can't use the value from cfg - swapchain images don't have correct data in the cfg field as they were
            // never actually created..
            uvec2 tex_dims = cur_t->frames[fif].image.dims.xy;

            // The frame buffer can only be as big as the smallest texture - so that's what we set it to
            if (fb_kd->dims.x == 0 || tex_dims.x < fb_kd->dims.x) {
                fb_kd->dims.x = tex_dims.x;
            }
            if (fb_kd->dims.y == 0 || tex_dims.y < fb_kd->dims.y) {
                fb_kd->dims.y = tex_dims.y;
            }

            // Resize only if cur att ind is less than or equal our size - we have no guarentees
            // that the slot order will necessarily match the attachment order
            if (att_ind >= fb_kd->atts.size) {
                arr_resize(&fb_kd->atts, att_ind + 1);
            }
            fb_kd->atts[att_ind] = tview;

            // Set clear value
            rformat tex_format = get_rformat(cur_t->frames[fif].image.format);
            cv[*cv_size] = get_vk_clear_value(cur_sl->clear_val, tex_format);
            ++(*cv_size);

            ++att_cnt;
        }
    }
    asrt(fb_kd->atts.size == att_cnt);
}

struct rbp_slot_usage_info
{
    const rbp_resource_requirement *first{};
    const rbp_resource_requirement *last{};
};

intern void gather_pass_slot_usage_info(rbp_slot_usage_info *infos, const rbp_pass &rbp_pass)
{
    // Record first/last requirement per slot so we can place a single pre-pass barrier and finalize state post-pass.
    for (u32 subi = 0; subi < rbp_pass.subpasses.size; ++subi) {
        const rbp_subpass *sub = &rbp_pass.subpasses[subi];
        for (u32 resi = 0; resi < sub->resources.size; ++resi) {
            const rbp_resource_requirement *req = &sub->resources[resi];
            asrt(req->slot_ind < rbp_pass.slots.size);
            rbp_slot_usage_info *info = &infos[req->slot_ind];
            if (!info->first) {
                info->first = req;
            }
            info->last = req;
        }
    }
}

intern rtexture_state get_pre_pass_texture_state(const rbp_pass &rbp_pass, const rbp_resource_requirement &req)
{
    rtexture_state desired{};
    if (is_usage_attachment(rbp_pass.slots[req.slot_ind].usage)) {
        // Attachments can be cleared by the render pass; in that case we can treat old contents as undefined.
        VkImageLayout init_layout = get_baked_initial_vk_layout(rbp_pass, req);
        if (init_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
            desired.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            desired.access = VK_ACCESS_NONE;
            desired.stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        else {
            desired.layout = init_layout;
            desired.access = get_vk_access_from_requirement(rbp_pass, req);
            desired.stage = get_vk_stage_from_requirement(rbp_pass, req);
        }
    }
    else {
        desired.layout = get_vk_layout_from_requirement(rbp_pass, req, false);
        desired.access = get_vk_access_from_requirement(rbp_pass, req);
        desired.stage = get_vk_stage_from_requirement(rbp_pass, req);
    }
    return desired;
}

intern rtexture_state get_post_pass_texture_state(const rbp_pass &rbp_pass, const rbp_resource_requirement &req)
{
    rtexture_state desired{};
    bool is_attachment = is_usage_attachment(rbp_pass.slots[req.slot_ind].usage);
    desired.layout = get_vk_layout_from_requirement(rbp_pass, req, is_attachment);
    desired.access = get_vk_access_from_requirement(rbp_pass, req);
    desired.stage = get_vk_stage_from_requirement(rbp_pass, req);
    return desired;
}

intern rbuffer_state get_pass_buffer_state(const rbp_pass &rbp_pass, const rbp_resource_requirement &req)
{
    rbuffer_state desired{};
    desired.access = get_vk_access_from_requirement(rbp_pass, req);
    desired.stage = get_vk_stage_from_requirement(rbp_pass, req);
    return desired;
}

intern void emit_manifest_pass_barriers(rmanifest *m, const rbp_pass &rbp_pass, const mpass &mp, VkCommandBuffer buf, u32 fif)
{
    rbp_slot_usage_info slot_usage[MAX_BP_PASS_SLOT_COUNT]{};
    gather_pass_slot_usage_info(slot_usage, rbp_pass);

    static_array<VkImageMemoryBarrier, MAX_BP_PASS_SLOT_COUNT> image_barriers{};
    static_array<VkBufferMemoryBarrier, MAX_BP_PASS_SLOT_COUNT> buffer_barriers{};
    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;

    for (u32 slot_ind = 0; slot_ind < rbp_pass.slots.size; ++slot_ind) {
        const rbp_resource_requirement *first = slot_usage[slot_ind].first;
        if (!first) {
            continue;
        }

        const mpass_slot_assignment &assignment = mp.slot_assignments[slot_ind];
        if (assignment.type == mslot_target_type::TEXTURE) {
            asrt(is_valid(assignment.t));
            asrt(assignment.t.index < m->textures.size);
            rtexture_state desired = get_pre_pass_texture_state(rbp_pass, *first);
            auto cur_t = &m->textures[assignment.t.index];
            rtexture_state *cur_state = &cur_t->frames[fif].state;

            bool use_bookends = rbp_pass.use_subpass_bookends && is_usage_attachment(rbp_pass.slots[slot_ind].usage);
            bool layout_mismatch = cur_state->layout != desired.layout;
            bool access_stage_mismatch = cur_state->access != desired.access || cur_state->stage != desired.stage;
            // Bookend dependencies cover external memory visibility for attachments. We still need a barrier
            // for layout transitions when the current layout doesn't match the render pass initial layout.
            if (layout_mismatch || (!use_bookends && access_stage_mismatch)) {
                ilog("%s: ca:%d da:%d  cs:%d ds:%d", cur_t->name, cur_state->access, desired.access, cur_state->stage, desired.stage);

                // Single barrier per resource into the first required state for this pass.
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = cur_state->layout;
                barrier.newLayout = desired.layout;
                barrier.srcAccessMask = cur_state->access;
                barrier.dstAccessMask = desired.access;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = m->textures[assignment.t.index].frames[fif].image.hndl;
                barrier.subresourceRange = m->textures[assignment.t.index].iv_cfg.srange;
                arr_push_back(&image_barriers, barrier);

                src_stage_mask |= normalize_vk_stage_mask(cur_state->stage);
                dst_stage_mask |= normalize_vk_stage_mask(desired.stage);
            }
            // Keep the manifest in sync so later passes see the in-pass state even if no barrier was needed.
            *cur_state = desired;
        }
        else if (assignment.type == mslot_target_type::BUFFER) {
            asrt(is_valid(assignment.b));
            asrt(assignment.b.index < m->buffers.size);
            rbuffer_state desired = get_pass_buffer_state(rbp_pass, *first);
            rbuffer_state *cur_state = &m->buffers[assignment.b.index].frames[fif].state;

            if (cur_state->access != desired.access || cur_state->stage != desired.stage) {
                // Single barrier per resource into the first required state for this pass.
                VkBufferMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.srcAccessMask = cur_state->access;
                barrier.dstAccessMask = desired.access;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = m->buffers[assignment.b.index].frames[fif].buffer.hndl;
                barrier.offset = 0;
                barrier.size = VK_WHOLE_SIZE;
                arr_push_back(&buffer_barriers, barrier);

                src_stage_mask |= normalize_vk_stage_mask(cur_state->stage);
                dst_stage_mask |= normalize_vk_stage_mask(desired.stage);
            }
            // Keep the manifest in sync so later passes see the in-pass state even if no barrier was needed.
            *cur_state = desired;
        }
    }

    if (image_barriers.size > 0 || buffer_barriers.size > 0) {
        // Collapse all barriers into a single pipeline barrier for this pass.
        if (src_stage_mask == 0) {
            src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        if (dst_stage_mask == 0) {
            dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        vkCmdPipelineBarrier(buf,
                             src_stage_mask,
                             dst_stage_mask,
                             0,
                             0,
                             nullptr,
                             (u32)buffer_barriers.size,
                             buffer_barriers.data,
                             (u32)image_barriers.size,
                             image_barriers.data);
    }
}

intern void update_manifest_pass_states(rmanifest *m, const rbp_pass &rbp_pass, const mpass &mp, u32 fif)
{
    rbp_slot_usage_info slot_usage[MAX_BP_PASS_SLOT_COUNT]{};
    gather_pass_slot_usage_info(slot_usage, rbp_pass);

    for (u32 slot_ind = 0; slot_ind < rbp_pass.slots.size; ++slot_ind) {
        const rbp_resource_requirement *last = slot_usage[slot_ind].last;
        if (!last) {
            continue;
        }
        const mpass_slot_assignment &assignment = mp.slot_assignments[slot_ind];
        if (assignment.type == mslot_target_type::TEXTURE) {
            // Final state after the pass completes (attachments use final layout).
            asrt(is_valid(assignment.t));
            asrt(assignment.t.index < m->textures.size);
            m->textures[assignment.t.index].frames[fif].state = get_post_pass_texture_state(rbp_pass, *last);
        }
        else if (assignment.type == mslot_target_type::BUFFER) {
            // Final buffer access/stage after the pass completes.
            asrt(is_valid(assignment.b));
            asrt(assignment.b.index < m->buffers.size);
            m->buffers[assignment.b.index].frames[fif].state = get_pass_buffer_state(rbp_pass, *last);
        }
    }
}

intern void execute_manifest(rmanifest *m, VkCommandBuffer buf, u32 fif)
{
    for (u32 rji = 0; rji < m->jobs.size; ++rji) {
        auto rbp = get_render_blueprint(m->rndr, m->rbp);
        auto cur_rj = &m->jobs[rji];
        auto mp = &m->passes[cur_rj->pid];
        auto rbp_pass = &rbp->passes[mp->rbp_pid];
        auto vk_rpass = (VkRenderPass)rbp_pass->vk_handle;
        auto mview = &m->views[cur_rj->vid];

        // Must have all slots assigned
        asrt(rbp_pass->slots.size == mp->slot_assignments.size);

        // Create all needed barriers for the current pass resources (according to what we have for the current state)
        emit_manifest_pass_barriers(m, *rbp_pass, *mp, buf, fif);

        // Setup framebuffer and clear vals by looping over slots
        static_array<VkClearValue, MAX_BP_PASS_SLOT_COUNT> att_clear_vals{};
        vkr_framebuffer_key_data fb_kd{};
        setup_framebuffer_key_and_clear_vals(&fb_kd, att_clear_vals.data, &att_clear_vals.size, vk_rpass, *mp, *rbp_pass, *m, fif);
        auto fb = get_or_create_framebuffer(&m->rndr->fb_cache, fb_kd, &m->rndr->vk);

        // RENDER AREA
        VkRect2D ra = (mp->ra_size_mode == rect_size_mode::NORMALIZED) ? get_vk_rect_from_normalized(mp->norm_render_area, fb->kd.dims)
                                                                       : get_vk_rect(mp->render_area);
        vkr_cmd_begin_rpass(buf, vk_rpass, fb, ra, att_clear_vals.data, att_clear_vals.size);

        // VIEWPORT
        VkViewport viewport = (mview->vp_size_mode == rect_size_mode::NORMALIZED)
                                  ? get_vk_viewport(mview->vp, mview->vp_depth_min_max, fb->kd.dims)
                                  : get_vk_viewport(mview->vp, mview->vp_depth_min_max);
        vkCmdSetViewport(buf, 0, 1, &viewport);

        // SCISSOR
        VkRect2D scissor = (mview->scissor_size_mode == rect_size_mode::NORMALIZED)
                               ? get_vk_rect_from_normalized(mview->norm_scissor, fb->kd.dims)
                               : get_vk_rect(mview->scissor);
        vkCmdSetScissor(buf, 0, 1, &scissor);

        if (cur_rj->cb) {
            render_job_cb_params p{};
            p.cmd_buf = (u64)buf;
            p.draw_calls = &cur_rj->draw_calls;
            cur_rj->cb(p, cur_rj->cb_user);
        }
        else {
            wlog("No draw function assigned for render-job %d (pass-id:%u view-id:%u rbp-pass:%s vkpass:%p blueprint:%s)",
                 rji,
                 cur_rj->pid,
                 cur_rj->vid,
                 rbp_pass->name,
                 rbp_pass->vk_handle,
                 rbp->name);
        }
        vkr_cmd_end_rpass(buf);

        // Update our working copy manifest states - we will copy these over to our renderer states once done executing
        // the manifest
        update_manifest_pass_states(m, *rbp_pass, *mp, fif);
    }
}

int end_render_frame(rmanifest *m)
{
    PROFILE_SCOPE("end_render_frame");
    asrt(m);
    asrt(is_valid(m->rbp));
    auto dev = &m->rndr->vk.inst.device;
    u32 fif = get_fif_ind(m->rndr);
    auto *cur_frame = &m->rndr->fifs[fif];

// Finalize IM GUI data - not dependent on our render stuff currently
#ifdef USE_IMGUI
    ImGui::Render();
#endif

    // The command buf index struct has an ind struct into the pool the cmd buf comes from, and then an ind into the buffer
    // The ind into the pool has an ind into the queue family (as that contains our array of command pools) and then and
    // ind to the command pool
    // auto fb = &dev->swapchain.fbs[cur_frame->cur_im_ind];
    // asrt(fb && "Invalid framebuffer");

    ////////////////////////////
    // Record Command Buffers //
    ////////////////////////////
    // Just use buf 0 for now
    auto buf = cur_frame->thread_pools[0].buf;
    int err = vkr_begin_cmd_buf(buf, {});
    execute_manifest(m, buf, fif);
    vkr_end_cmd_buf(buf);

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
    submit_info.pCommandBuffers = &buf;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m->rndr->vk.inst.device.swapchain.renders_finished[cur_frame->cur_im_ind];
    s32 vk_res = vkQueueSubmit(dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE], 1, &submit_info, cur_frame->in_flight);
    asrt(vk_res == VK_SUCCESS);

    ///////////////////
    // Present Image //
    ///////////////////
    // Once the rendering signal has fired, present the image (show it on screen)
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m->rndr->vk.inst.device.swapchain.renders_finished[cur_frame->cur_im_ind];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &dev->swapchain.swapchain;
    present_info.pImageIndices = &cur_frame->cur_im_ind;
    present_info.pResults = nullptr; // Optional - check for individual swaps
    vk_res = vkQueuePresentKHR(dev->qfams[VKR_QUEUE_FAM_TYPE_PRESENT].qs[VKR_RENDER_QUEUE], &present_info);

    // This purely helps with smoothness - it works fine without recreating the swapchain here and instead doing it on
    // the next frame, but it seems to resize more smoothly doing it here
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR || vk_res == VK_SUBOPTIMAL_KHR) {
        cur_frame->swapchain_resize = WINDOW_RESIZE_DEBOUNCE_DURATION;
    }
    else {
        asrt(vk_res == VK_SUCCESS);
        ++m->rndr->finished_frames;
    }
    return err_code::RENDER_NO_ERROR;
}

} // namespace nslib
