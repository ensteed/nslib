#include "platform.h"
#include "vkr_context.h"
#include "vkr_texture_pool.h"
#include "renderer.h"
#include "vkr_utils.h"
#include "render_manifest.h"

#ifdef USE_IMGUI
    #include "imgui/imgui.h"
    #include "imgui/imgui_impl_sdl3.h"
    #include "imgui/imgui_impl_vulkan.h"
    #include "SDL3/SDL_events.h"
#endif

// This will emit a log for image barriers which kills perfomance but useful to see sometimes
// #define LOG_IMAGE_MEM_BARRIER
// #define LOG_BUFFER_MEM_BARRIER
// #define LOG_PIPELINE_BARRIER

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

intern void terminate_geometry(renderer *rndr, rgeom_info *gref)
{
    ilog("Terminating geometry %s", gref->name);
    vmaVirtualFree(gref->vert_block, gref->vert_mem);
    vmaVirtualFree(gref->ind_block, gref->ind_mem);
    *gref = {};
}

intern bool fill_geometry_layout_entry(geom_buffer_layout_entry *layout,
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
    // it dictates at what range (in vertices) each buffer uses for each geom.. this is not the most "efficient" thing
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

            auto fmt = get_rformat_info(cur_attrib_desc->fmt);
            cur_binding->stride += fmt.bytes_per_block;
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

intern void terminate_geometry_stream_group(geom_stream_group *gp, const vkr_context *vk)
{
    ilog("Terminating geometry stream group %s (%lu)", gp->indice_stream.name, gp->id);
    for (u32 i = 0; i < gp->layouts.size; ++i) {
        vmaDestroyVirtualBlock(gp->layouts[i].vert_block);
        for (u32 bufi = 0; bufi < gp->layouts[i].vert_streams.size; ++bufi) {
            ilog("Terminating vert stream %s", gp->layouts[i].vert_streams[bufi].name);
            vkr_terminate_buffer(&gp->layouts[i].vert_streams[bufi].buffer, vk);
        }
    }
    vmaDestroyVirtualBlock(gp->indices_block);
    vkr_terminate_buffer(&gp->indice_stream.buffer, vk);
    *gp = {};
}

intern void terminate_framebuffers_with_image(renderer *rndr, VkImageView iv)
{
    ilog("Destroying framebuffers for image view %p", iv);
    for (auto sliter = slot_pool_begin(&rndr->fb_cache.items); is_valid(sliter); sliter = slot_pool_next(&rndr->fb_cache.items, sliter)) {
        // If it had one, we delete it and continue, otherwise we leave it alone and continue
        if (arr_find(&sliter.item->gpu_d.atts, iv)) {
            ilog("-> destroying framebuffer %p", sliter.item->gpu_d.hndl, iv);
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

intern bool init_frame_contexts(renderer *rndr, sizet thread_cnt)
{
    auto dev = &rndr->vk.inst.device;
    rndr->fifs.size = rndr->fifs.capacity;

    // Create frame synchronization objects - start all fences as signalled already
    for (u32 framei = 0; framei < rndr->fifs.size; ++framei) {
        auto cur_fif = &rndr->fifs[framei];

        int result = vkr_init_fence(&cur_fif->in_flight, VK_FENCE_CREATE_SIGNALED_BIT, &rndr->vk);
        if (result != VK_SUCCESS) {
            return false;
        }

        result = vkr_init_semaphore(&cur_fif->image_avail, {}, &rndr->vk);
        if (result != VK_SUCCESS) {
            return false;
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
                return false;
            }

            vkr_alloc_cmd_bufs_cfg buf_cfgs{};
            buf_cfgs.count = 1;
            buf_cfgs.pool = cur_fif->thread_pools[i].pool;
            result = vkr_alloc_cmd_bufs(&cur_fif->thread_pools[i].buf, buf_cfgs, &rndr->vk);
            if (result != err_code::VKR_NO_ERROR) {
                return false;
            }
        }
    }
    ilog("Successfully initialized %lu render frames in flight", rndr->fifs.size);
    return true;
}

intern void terminate_frame_contexts(renderer *rndr)
{
    ilog("Terminating frame contexts");
    auto dev = &rndr->vk.inst.device;
    for (u32 framei = 0; framei < rndr->fifs.size; ++framei) {
        auto cur_fif = &rndr->fifs[framei];
        vkr_terminate_fence(cur_fif->in_flight, &rndr->vk);
        vkr_terminate_semaphore(cur_fif->image_avail, &rndr->vk);
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

intern void terminate_global_descriptor_info(renderer *rndr)
{
    // Terminate our default descriptor layout sets
    ilog("Terminating global desc info");
    vkr_terminate_desc_pool(rndr->desc_info.pool, &rndr->vk);
    vkr_terminate_chunked_buffer(&rndr->desc_info.material_ssbo, &rndr->vk);
    vkr_terminate_chunked_buffer(&rndr->desc_info.instance_ssbo, &rndr->vk);
    vkr_terminate_buffer(&rndr->desc_info.frame_ubo.buffer, &rndr->vk);
    vkr_terminate_buffer(&rndr->desc_info.pass_ssbo.buffer, &rndr->vk);
    vkr_terminate_buffer(&rndr->desc_info.view_ssbo.buffer, &rndr->vk);
    vkr_terminate_buffer(&rndr->desc_info.draw_ssbo.buffer, &rndr->vk);
    vkr_terminate_desc_set_layouts(rndr->desc_info.dset_layouts, RDSET_LAYOUT_COUNT, &rndr->vk);
    vkr_terminate_pipeline_layout(rndr->desc_info.pline_layout, &rndr->vk);
}

intern b32 create_descriptor_set_layouts(renderer *rndr, u32 tex_pool_count)
{
    vkr_descriptor_set_layout_desc dsets[RDSET_LAYOUT_COUNT]{};

    // TODO: Eventually we should make these device local buffers with staging buffers for updates
    VkDescriptorSetLayoutBinding g_set_main_data_bindings[RDSET_MAIN_DATA_BINDING_COUNT]{};
    u32 si = RDSET_LAYOUT_MAIN_DATA;

    // We could use the constant bindings here if we wanted, but this makes it more clear that each binding entry in the
    // array can come in any order.. as long as the binding member is set to the right thing
    u32 bi = 0;

    // Draw buffer built each frame
    g_set_main_data_bindings[bi].binding = RDSET_MAIN_DATA_BINDING_DRAW_SSBO;
    g_set_main_data_bindings[bi].descriptorCount = 1;
    g_set_main_data_bindings[bi].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    g_set_main_data_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ++bi;

    // Per view data buffer build each frame
    g_set_main_data_bindings[bi].binding = RDSET_MAIN_DATA_BINDING_VIEW_SSBO;
    g_set_main_data_bindings[bi].descriptorCount = 1;
    g_set_main_data_bindings[bi].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    g_set_main_data_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ++bi;

    // Per pass data buffer built each frame
    g_set_main_data_bindings[bi].binding = RDSET_MAIN_DATA_BINDING_PASS_SSBO;
    g_set_main_data_bindings[bi].descriptorCount = 1;
    g_set_main_data_bindings[bi].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    g_set_main_data_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ++bi;

    // Frame data filled each frame
    g_set_main_data_bindings[bi].binding = RDSET_MAIN_DATA_BINDING_FRAME_UBO;
    g_set_main_data_bindings[bi].descriptorCount = 1;
    g_set_main_data_bindings[bi].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    g_set_main_data_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ++bi;

    // Instance ssbo
    g_set_main_data_bindings[bi].binding = RDSET_MAIN_DATA_BINDING_INSTANCE_SSBO;
    g_set_main_data_bindings[bi].descriptorCount = 1;
    g_set_main_data_bindings[bi].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    g_set_main_data_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ++bi;

    // Material ssbo
    g_set_main_data_bindings[bi].binding = RDSET_MAIN_DATA_BINDING_MATERIAL_SSBO;
    g_set_main_data_bindings[bi].descriptorCount = 1;
    g_set_main_data_bindings[bi].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    g_set_main_data_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ++bi;

    // Immutable samplers
    g_set_main_data_bindings[bi].binding = RDSET_MAIN_DATA_BINDING_IMMUTABLE_SAMPLERS;
    g_set_main_data_bindings[bi].descriptorCount = RSAMPLER_TYPE_COUNT;
    g_set_main_data_bindings[bi].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    g_set_main_data_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    g_set_main_data_bindings[bi].pImmutableSamplers = rndr->samplers;
    ++bi;

    dsets[si].bindings = g_set_main_data_bindings;
    dsets[si].binding_count = RDSET_MAIN_DATA_BINDING_COUNT;

    VkDescriptorSetLayoutBinding g_set_images_bindings[RDSET_IMAGE_BINDING_COUNT]{};
    // All images
    si = RDSET_LAYOUT_IMAGES;
    bi = 0;
    g_set_images_bindings[bi].binding = RDSET_IMAGE_BINDING_IMAGE_ARRAYS;
    g_set_images_bindings[bi].descriptorCount = tex_pool_count;
    g_set_images_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    g_set_images_bindings[bi].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ++bi;

    dsets[si].bindings = g_set_images_bindings;
    dsets[si].binding_count = RDSET_IMAGE_BINDING_COUNT;

    // Create the global set layouts
    vkr_descriptor_set_layout_cfg cfg{};
    cfg.dset_layouts = dsets;
    cfg.dset_layout_count = ARR_SIZE(dsets);
    s32 result = vkr_init_desc_set_layouts(rndr->desc_info.dset_layouts, cfg, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        return false;
    }
    return true;
}

intern b32 init_global_descriptor_info(renderer *rndr, const rpipeline_layout_cfg &dip)
{
    ilog("Initializing global descriptor info");

    // Not exactly needed for SSBO but its best to align to 16 bytes anyways
    asrt(dip.draw_ssbo.block_size % 16 == 0);
    asrt(dip.view_ssbo.block_size % 16 == 0);
    asrt(dip.pass_ssbo.block_size % 16 == 0);
    asrt(dip.instance_ssbo.block_size % 16 == 0);
    asrt(dip.material_ssbo.block_size % 16 == 0);
    // Absolute requirement for uniform buffers
    asrt(dip.frame_ubo.block_size % 16 == 0);
    // Limit our ranges - we can always increase this number if we needed but right now it's unnecessary
    asrt(dip.push_const_range_count <= MAX_PUSH_CONSTANT_RANGES);

    // Create desc layouts
    create_descriptor_set_layouts(rndr, rndr->textures.pools.size);

    ////////////////////////////
    // Global pipeline layout //
    ////////////////////////////
    vkr_pipeline_layout_cfg pl_cfg{};
    VkPushConstantRange push_const_ranges[MAX_PUSH_CONSTANT_RANGES]{};
    pl_cfg.set_layouts = rndr->desc_info.dset_layouts;
    pl_cfg.set_layout_count = RDSET_LAYOUT_COUNT;
    pl_cfg.push_const_range_count = dip.push_const_range_count;
    for (u32 i = 0; i < dip.push_const_range_count; ++i) {
        push_const_ranges[i].stageFlags = get_vk_shader_stage_flags(dip.push_const_ranges[i].stages);
        push_const_ranges[i].offset = dip.push_const_ranges[i].offset;
        push_const_ranges[i].size = dip.push_const_ranges[i].size;
    }
    pl_cfg.push_const_ranges = push_const_ranges;
    s32 result = vkr_init_pipeline_layout(&rndr->desc_info.pline_layout, pl_cfg, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        terminate_global_descriptor_info(rndr);
        return false;
    }


    // Setup up shared buffer config
    vkr_buffer_cfg b_cfg{};
    b_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    b_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    b_cfg.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    b_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    b_cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;

    //////////////////////
    // Create Draw SSBO //
    //////////////////////
    sizet draw_buf_fif_sz = dip.draw_ssbo.block_size * dip.draw_ssbo.block_count;
    rndr->desc_info.draw_ssbo.block_size = dip.draw_ssbo.block_size;
    b_cfg.buffer_size = draw_buf_fif_sz * MAX_FRAMES_IN_FLIGHT;
    b_cfg.vma_alloc_name = "draw_ssbo";
    result = vkr_init_buffer(&rndr->desc_info.draw_ssbo.buffer, b_cfg);
    if (result != err_code::VKR_NO_ERROR) {
        terminate_global_descriptor_info(rndr);
        return result;
    }

    //////////////////////
    // Create View SSBO //
    //////////////////////
    sizet view_buf_fif_sz = dip.view_ssbo.block_size * dip.view_ssbo.block_count;
    rndr->desc_info.view_ssbo.block_size = dip.view_ssbo.block_size;
    b_cfg.buffer_size = view_buf_fif_sz * MAX_FRAMES_IN_FLIGHT;
    b_cfg.vma_alloc_name = "view_ssbo";
    result = vkr_init_buffer(&rndr->desc_info.view_ssbo.buffer, b_cfg);
    if (result != err_code::VKR_NO_ERROR) {
        terminate_global_descriptor_info(rndr);
        return result;
    }

    //////////////////////
    // Create Pass SSBO //
    //////////////////////
    sizet pass_buf_fif_sz = dip.pass_ssbo.block_size * dip.pass_ssbo.block_count;
    rndr->desc_info.pass_ssbo.block_size = dip.pass_ssbo.block_size;
    b_cfg.buffer_size = pass_buf_fif_sz * MAX_FRAMES_IN_FLIGHT;
    b_cfg.vma_alloc_name = "pass_ssbo";
    result = vkr_init_buffer(&rndr->desc_info.pass_ssbo.buffer, b_cfg);
    if (result != err_code::VKR_NO_ERROR) {
        terminate_global_descriptor_info(rndr);
        return result;
    }

    //////////////////////
    // Create Frame UBO //
    //////////////////////
    // Need to align to UBO min offset alignment because we will have a single buffer for all FIFs with later
    // ones offset by FIF_SIZE * fif_i
    sizet ubo_min_offset = rndr->vk.inst.pdev_info.props.limits.minUniformBufferOffsetAlignment;
    sizet frame_buf_fif_sz = align_up(dip.frame_ubo.block_size * dip.frame_ubo.block_count, ubo_min_offset);
    rndr->desc_info.frame_ubo.block_size = dip.frame_ubo.block_size;
    b_cfg.buffer_size = frame_buf_fif_sz * MAX_FRAMES_IN_FLIGHT;
    b_cfg.vma_alloc_name = "frame_ubo";
    b_cfg.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    result = vkr_init_buffer(&rndr->desc_info.frame_ubo.buffer, b_cfg);
    if (result != err_code::VKR_NO_ERROR) {
        terminate_global_descriptor_info(rndr);
        return result;
    }

    // Set up shared chunked buffer config
    vkr_chunked_buffer_cfg cb_cfg{};
    cb_cfg.buffer_cfg.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    cb_cfg.buffer_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    cb_cfg.buffer_cfg.alloc_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    cb_cfg.buffer_cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;
    cb_cfg.chunk_tracking_arena = &rndr->persist_fl;

    
    //////////////////////////
    // Create Instance SSBO //
    //////////////////////////
    // We allocate a buffer big enough to have a slot for each frame in flight so we can avoid needing to sync stuff..
    // or create a slot in the buffer for every update and retire the old
    sizet instance_buf_fif_sz = dip.instance_ssbo.block_size * dip.instance_ssbo.block_count;
    cb_cfg.buffer_cfg.buffer_size = instance_buf_fif_sz * MAX_FRAMES_IN_FLIGHT;
    cb_cfg.chunk_size = dip.instance_ssbo.block_size;
    cb_cfg.buffer_cfg.vma_alloc_name = "instance_ssbo";
    if (!vkr_init_chunked_buffer(&rndr->desc_info.instance_ssbo, cb_cfg)) {
        terminate_global_descriptor_info(rndr);
        return false;
    }

    //////////////////////////
    // Create material SSBO //
    //////////////////////////
    // For materials we also do a buffer for each frame in flight - at least for now unless it proves too much. But
    // really, mat data is probably pretty small compared to instance data
    sizet mat_buf_fif_sz = dip.material_ssbo.block_size * dip.material_ssbo.block_count;
    cb_cfg.buffer_cfg.buffer_size = mat_buf_fif_sz * MAX_FRAMES_IN_FLIGHT;
    cb_cfg.chunk_size = dip.material_ssbo.block_size;
    cb_cfg.buffer_cfg.vma_alloc_name = "material_ssbo";
    if (!vkr_init_chunked_buffer(&rndr->desc_info.material_ssbo, cb_cfg)) {
        terminate_global_descriptor_info(rndr);
        return false;
    }

    ////////////////////////////
    // Create descriptor pool //
    ////////////////////////////
    // Get a count of the number of descriptors we are making avaialable for each desc type
    vkr_desc_cfg desc_cfg{};
    desc_cfg.max_sets = MAX_FRAMES_IN_FLIGHT + 1;
    // Instance and material ssbos
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] = 5 * MAX_FRAMES_IN_FLIGHT;
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = 1 * MAX_FRAMES_IN_FLIGHT;
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE] = rndr->textures.pools.size;
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_SAMPLER] = RSAMPLER_TYPE_COUNT * MAX_FRAMES_IN_FLIGHT;
    result = vkr_init_desc_pool(&rndr->desc_info.pool, desc_cfg, &rndr->vk);
    if (result != VK_SUCCESS) {
        terminate_global_descriptor_info(rndr);
        return false;
    }

    //////////////////////////////
    // Allocate descriptor sets //
    //////////////////////////////
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT + 1]{};
    for (u32 fif = 0; fif < MAX_FRAMES_IN_FLIGHT; ++fif) {
        layouts[fif] = rndr->desc_info.dset_layouts[0];
    }
    layouts[MAX_FRAMES_IN_FLIGHT] = rndr->desc_info.dset_layouts[1];
    vkr_alloc_desc_sets_cfg alloc_dset_cfg{};
    alloc_dset_cfg.pool = rndr->desc_info.pool;
    alloc_dset_cfg.set_count = desc_cfg.max_sets;
    alloc_dset_cfg.set_layouts = layouts;
    vkr_alloc_desc_sets(rndr->desc_info.main_data, alloc_dset_cfg, &rndr->vk);

    // We don't write out the immutable samplers - they are included in the layout
    VkDescriptorType main_data_write_types[] = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // Draw SSBO
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // View SSBO
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // Pass SSBO
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Frame UBO
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // Instance SSBO
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // Material SSBO
    };

    constexpr u32 MAIN_DATA_WRITE_FIF_STRIDE = ARR_SIZE(main_data_write_types);
    constexpr u32 MAIN_DATA_WRITE_COUNT = MAIN_DATA_WRITE_FIF_STRIDE * MAX_FRAMES_IN_FLIGHT;
    constexpr u32 DESC_WRITE_COUNT = MAIN_DATA_WRITE_COUNT + (u32)RDSET_IMAGE_BINDING_COUNT;
    VkWriteDescriptorSet desc_writes[DESC_WRITE_COUNT]{};

    // First we pile together all of the buffer infos
    VkDescriptorBufferInfo buffer_infos[MAIN_DATA_WRITE_COUNT]{};
    for (u32 fif_i = 0; fif_i < MAX_FRAMES_IN_FLIGHT; ++fif_i) {
        u32 bi_offset = fif_i * MAIN_DATA_WRITE_FIF_STRIDE;
        u32 bi{0};

        // Draw SSBO
        bi = bi_offset + RDSET_MAIN_DATA_BINDING_DRAW_SSBO;
        buffer_infos[bi].buffer = rndr->desc_info.draw_ssbo.buffer.hndl;
        buffer_infos[bi].offset = fif_i * draw_buf_fif_sz;
        buffer_infos[bi].range = draw_buf_fif_sz;

        // View SSBO
        bi = bi_offset + RDSET_MAIN_DATA_BINDING_VIEW_SSBO;
        buffer_infos[bi].buffer = rndr->desc_info.view_ssbo.buffer.hndl;
        buffer_infos[bi].offset = fif_i * view_buf_fif_sz;
        buffer_infos[bi].range = view_buf_fif_sz;

        // Pass SSBO
        bi = bi_offset + RDSET_MAIN_DATA_BINDING_PASS_SSBO;
        buffer_infos[bi].buffer = rndr->desc_info.pass_ssbo.buffer.hndl;
        buffer_infos[bi].offset = fif_i * pass_buf_fif_sz;
        buffer_infos[bi].range = pass_buf_fif_sz;

        // Frame UBO
        bi = bi_offset + RDSET_MAIN_DATA_BINDING_FRAME_UBO;
        buffer_infos[bi].buffer = rndr->desc_info.frame_ubo.buffer.hndl;
        buffer_infos[bi].offset = fif_i * frame_buf_fif_sz;
        buffer_infos[bi].range = frame_buf_fif_sz;

        // Instance SSBO
        bi = bi_offset + RDSET_MAIN_DATA_BINDING_INSTANCE_SSBO;
        buffer_infos[bi].buffer = rndr->desc_info.instance_ssbo.buffer.hndl;
        buffer_infos[bi].offset = fif_i * instance_buf_fif_sz;
        buffer_infos[bi].range = instance_buf_fif_sz;

        // Material SSBO
        bi = bi_offset + RDSET_MAIN_DATA_BINDING_MATERIAL_SSBO;
        buffer_infos[bi].buffer = rndr->desc_info.material_ssbo.buffer.hndl;
        buffer_infos[bi].offset = fif_i * mat_buf_fif_sz;
        buffer_infos[bi].range = mat_buf_fif_sz;
    }

    // Create all of the image infos for each pool in the texture registry
    array<VkDescriptorImageInfo> image_infos;
    arr_init(&image_infos, &rndr->scratch_stack);
    arr_resize(&image_infos, rndr->textures.pools.size);
    for (u32 i = 0; i < image_infos.size; ++i) {
        image_infos[i].imageView = rndr->textures.pools[i].view;
        image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // We will be using immutable samplers for this binding, so we can just set this to null
        image_infos[i].sampler = VK_NULL_HANDLE;
    }

    // Now we create all the descriptor writes referencing the above buffer/image infos..
    for (u32 i = 0; i < DESC_WRITE_COUNT; ++i) {
        auto cur_dw = &desc_writes[i];
        u32 fif = i / MAIN_DATA_WRITE_FIF_STRIDE;
        u32 bi = i % MAIN_DATA_WRITE_FIF_STRIDE;
        bool is_main_data = i < MAIN_DATA_WRITE_COUNT;
        cur_dw->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cur_dw->dstSet = is_main_data ? rndr->desc_info.main_data[fif] : rndr->desc_info.images;
        cur_dw->dstBinding = is_main_data ? bi : 0;
        cur_dw->dstArrayElement = 0;
        cur_dw->descriptorType = is_main_data ? main_data_write_types[bi] : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        cur_dw->descriptorCount = is_main_data ? 1 : rndr->textures.pools.size;
        cur_dw->pBufferInfo = is_main_data ? &buffer_infos[i] : nullptr;
        cur_dw->pImageInfo = !is_main_data ? image_infos.data : nullptr;
    }
    vkUpdateDescriptorSets(rndr->vk.inst.device.hndl, DESC_WRITE_COUNT, desc_writes, 0, nullptr);
    arr_terminate(&image_infos);
    return true;
}

intern b32 init_global_samplers(renderer *rndr)
{
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

    int err = vkr_init_sampler(rndr->samplers, samp_cfg, &rndr->vk);
    if (err != err_code::VKR_NO_ERROR) {
        wlog("Failed to initialize sampler - vk err code: %d", err);
        return false;
    }
    return true;
}

intern void terminate_global_samplers(renderer *rndr)
{
    // Terminate all texture samplers
    for (u32 i = 0; i < RSAMPLER_TYPE_COUNT; ++i) {
        vkr_terminate_sampler(rndr->samplers[i], &rndr->vk);
    }
}

intern void init_geometry_stream_groups(renderer *rndr)
{
    // Geometry index/vertex buffers
    asrt(rndr->geom_groups.size == 0);
    hmap_init(&rndr->geom_group_id_map, hash_type, &rndr->persist_fl);
}

intern void terminate_geometry_stream_groups(renderer *rndr)
{
    ilog("Terminating geometry stream groups");
    // Remove source geometry buffers
    for (u32 i = 0; i < rndr->geom_groups.size; ++i) {
        terminate_geometry_stream_group(&rndr->geom_groups[i], &rndr->vk);
    }
    rndr->geom_groups.size = 0;
    hmap_terminate(&rndr->geom_group_id_map);
}

intern void terminate_shader(renderer *rndr, rshader_info *shdr)
{
    ilog("Terminating shader %s", shdr->name);
    for (u8 t = 0; t < RSHADER_STAGE_TYPE_COUNT; ++t) {
        vkr_terminate_shader_module(shdr->stages[t].sm, &rndr->vk);
    }
    *shdr = {};
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
         get_slot_used_count(cache->items),
         lname,
         cache->items.slots.size,
         cache->items.free_list.size);
    for (auto sliter = slot_pool_begin(&cache->items); is_valid(sliter); sliter = slot_pool_next(&cache->items, sliter)) {
        term_func(&sliter.item->gpu_d);
    }
    terminate_slot_pool(&cache->items);
    hmap_terminate(&cache->key_lut);
}

intern void init_blueprints(renderer *rndr)
{
    hmap_init(&rndr->blueprint_id_map, hash_type, &rndr->persist_fl);
    init_slot_pool(&rndr->blueprints, MAX_BP_COUNT, &rndr->persist_fl);
}

intern void terminate_blueprints(renderer *rndr)
{
    ilog("Terminating render blueprints");
    while (!slot_pool_empty(rndr->blueprints)) {
        destroy_render_blueprint(rndr, slot_pool_begin(&rndr->blueprints).hndl);
    }
    terminate_slot_pool(&rndr->blueprints);
    hmap_terminate(&rndr->blueprint_id_map);
}

intern void terminate_rtechnique(renderer *rndr, rtechnique_info *info)
{
    ilog("Terminating technique %s with %lu pass pipelines", info->name, info->rpass_plines.size);
    *info = {};
}

intern void init_render_resources(renderer *rndr, const renderer_cfg &rcfg)
{
    init_slot_pool(&rndr->shaders, MAX_SHADER_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->techniques, MAX_TECHNIQUE_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->materials, MAX_MATERIAL_COUNT, &rndr->persist_fl);
    init_slot_pool(&rndr->geometry, MAX_GEOM_COUNT, &rndr->persist_fl);
    rtexture_regisitry_cfg cfg{
        .persist_fl = &rndr->persist_fl,
        .scratch_stack = &rndr->scratch_stack,
        .pool_count = rcfg.texture_pool_count,
        .cfgs = rcfg.texture_pool_cfgs,
        .vk = &rndr->vk,
    };
    init_rtexture_registry(&rndr->textures, cfg);
}

intern void terminate_render_resources(renderer *rndr)
{
    ilog("Terminating render resources (%lu geoms, %lu textures, %lu mats, %lu techniques, %lu shdrs)",
         get_slot_used_count(rndr->geometry),
         get_slot_used_count(rndr->textures),
         get_slot_used_count(rndr->materials),
         get_slot_used_count(rndr->techniques),
         get_slot_used_count(rndr->shaders));
    // Geometries
    for (auto iter = slot_pool_begin(&rndr->geometry); is_valid(iter); iter = slot_pool_next(&rndr->geometry, iter)) {
        terminate_geometry(rndr, iter.item);
    }
    terminate_slot_pool(&rndr->geometry);

    // Terminate all images and image views
    terminate_rtexture_registry(&rndr->textures);

    // Materials
    for (auto iter = slot_pool_begin(&rndr->materials); is_valid(iter); iter = slot_pool_next(&rndr->materials, iter)) {
        // Do something
    }
    terminate_slot_pool(&rndr->materials);

    // Techniques
    for (auto iter = slot_pool_begin(&rndr->techniques); is_valid(iter); iter = slot_pool_next(&rndr->techniques, iter)) {
        terminate_rtechnique(rndr, iter.item);
    }
    terminate_slot_pool(&rndr->techniques);

    // Shaders
    for (auto iter = slot_pool_begin(&rndr->shaders); is_valid(iter); iter = slot_pool_next(&rndr->shaders, iter)) {
        terminate_shader(rndr, iter.item);
    }
    terminate_slot_pool(&rndr->shaders);
}

void handle_window_resize(renderer *rndr)
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


idx_t push_geometry_stream_group(renderer *rndr, const geometry_stream_group_desc &desc)
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
        terminate_geometry_stream_group(cur_group, &rndr->vk);
        --rndr->geom_groups.size;
        return INVALID_IDX;
    }
    cur_group->id = hash_type(cur_group->indice_stream.name);
    hmap_insert(&rndr->geom_group_id_map, cur_group->id, geom_id);
    return geom_id;
}

idx_t find_geometry_stream_group(renderer *rndr, rres_id group_id)
{
    auto id_fiter = hmap_find(&rndr->geom_group_id_map, group_id);
    return id_fiter ? id_fiter->val : INVALID_IDX;
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

rgeom_handle create_rgeometry(renderer *rndr, const rgeom_desc &ci)
{
    // Make sure we have valid data
    asrt(ci.vert_count > 0);
    asrt(ci.vert_data);
    asrt(ci.ind_count > 0);
    asrt(ci.ind_data);
    asrt(ci.subgeom_cnt > 0);
    asrt(ci.subgeoms);
    asrt(ci.group < rndr->geom_groups.size);

    // Verify we have room for a geom
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
        terminate_geometry(rndr, geom_ref.item);
        asrt(release_slot(&rndr->geometry, geom_ref.hndl));
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
        terminate_geometry(rndr, geom_ref.item);
        asrt(release_slot(&rndr->geometry, geom_ref.hndl));
        return {};
    }
    asrt(ind_stream_byte_offset % alloc_ci.alignment == 0);
    geom_ref.item->ind_offset = ind_stream_byte_offset / alloc_ci.alignment;

    // Create staging buffer and get the queue we will use
    VkCommandBuffer tmp_cmd_buf{};
    result = vkr_alloc_cmd_bufs(&tmp_cmd_buf, {.pool = rndr->transient_pool}, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        wlog("Failed to create command buffer - error code: %d", result);
        terminate_geometry(rndr, geom_ref.item);
        asrt(release_slot(&rndr->geometry, geom_ref.hndl));
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
            terminate_geometry(rndr, geom_ref.item);
            asrt(release_slot(&rndr->geometry, geom_ref.hndl));
            vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
            return {};
        }
    }

    VkBufferCopy region{};
    region.size = ci.ind_count * sizeof(ind_t);
    region.dstOffset = geom_ref.item->ind_offset * sizeof(ind_t);
    result = vkr_stage_and_upload_buffer_data(&gp->indice_stream.buffer, ci.ind_data, &region, 1, tmp_cmd_buf, tmp_q, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        terminate_geometry(rndr, geom_ref.item);
        asrt(release_slot(&rndr->geometry, geom_ref.hndl));
        vkr_free_cmd_bufs(&tmp_cmd_buf, 1, rndr->transient_pool, &rndr->vk);
        geom_ref = {};
    }
    return geom_ref.hndl;
}

rtexture_handle create_rtexture(renderer *rndr, const rtexture_desc &tdesc)
{
    asrt(rndr);
    return create_rtexture(&rndr->textures, tdesc, (gpu_handle)rndr->transient_pool);
}

rshader_handle create_rshader(renderer *rndr, const rshader_desc &sdr_info)
{
    if (sdr_info.stage_cnt == 0) {
        return {};
    }
    rshader_ref sref = acquire_slot(&rndr->shaders);
    strncpy(sref.item->name, sdr_info.name, SMALL_STR_LEN - 1);
    for (u8 i = 0; i < sdr_info.stage_cnt; ++i) {
        auto cur_desc = &sdr_info.stages[i];
        auto cur_st = &sref.item->stages[cur_desc->stype];

        s32 result = vkr_init_shader_module(&cur_st->sm, cur_desc->src, cur_desc->src_byte_size, &rndr->vk);
        if (result != err_code::VKR_NO_ERROR) {
            terminate_shader(rndr, sref.item);
            release_slot(&rndr->shaders, sref.hndl);
            return {};
        }

        strncpy(cur_st->entry_point, cur_desc->entry_point, SMALL_STR_LEN - 1);

        // Might want to add stuff here in the future - for now, null
        cur_st->specialized_info = nullptr;
    }
    return sref.hndl;
}

rtechnique_handle create_rtechnique(renderer *rndr, const rtechnique_desc &tdesc)
{
    if (tdesc.pass_count == 0) {
        return {};
    }
    rtechnique_ref rtech = acquire_slot(&rndr->techniques);
    strncpy(rtech.item->name, tdesc.name, SMALL_STR_LEN - 1);

    for (u32 i = 0; i < tdesc.pass_count; ++i) {
        auto cur_desc = &tdesc.passes[i];
        auto rbp_bp = get_render_blueprint(rndr, cur_desc->bp_info.bp);
        asrt(rbp_bp);

        asrt(cur_desc->bp_info.pid < rbp_bp->passes.size);
        auto rbp_pass = &rbp_bp->passes[cur_desc->bp_info.pid];
        asrt(cur_desc->bp_info.spi < rbp_pass->subpasses.size);

        auto shdr = get_slot_item(&rndr->shaders, cur_desc->shader);
        asrt(shdr);

        // Stream group stuff
        idx_t geom_gp = find_geometry_stream_group(rndr, rbp_pass->geom_streams_group);
        geom_stream_group *gsg = get_idxn_arr_item(rndr->geom_groups, geom_gp);
        asrt(gsg);
        asrt(cur_desc->geom_buffer_layout < gsg->layouts.size);
        auto vert_layout = get_idxn_arr_item(gsg->layouts, cur_desc->geom_buffer_layout);
        asrt(vert_layout);

        vkr_pipeline_cfg cfg{};
        cfg.rpass = (VkRenderPass)rbp_pass->vk_handle;
        cfg.subpass = cur_desc->bp_info.spi;
        cfg.vert_desc = vert_layout->vert_layout;
        cfg.layout_hndl = rndr->desc_info.pline_layout;

        ////////////////////
        // Shader Modules //
        ////////////////////
        vkr_pipeline_cfg_shader_stage stages[RSHADER_STAGE_TYPE_COUNT];
        cfg.stage_cnt = 0;
        for (u32 i = 0; i < RSHADER_STAGE_TYPE_COUNT; ++i) {
            if (shdr->stages[i].sm != VK_NULL_HANDLE) {
                auto stype = (rshader_stage_type)i;
                ++cfg.stage_cnt;
                stages[i].stage = get_vk_shader_stage_flag_bit(stype);
                stages[i].entry_point = shdr->stages[i].entry_point;
                stages[i].module = shdr->stages[i].sm;
                stages[i].specialized_info = shdr->stages[i].specialized_info;
            }
        }
        cfg.stages = stages;

        ////////////////////
        // Dynamic States //
        ////////////////////
        VkDynamicState dyn_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_DEPTH_BIAS,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,
            VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
            VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,
            VK_DYNAMIC_STATE_CULL_MODE,
            VK_DYNAMIC_STATE_FRONT_FACE,
            VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
            VK_DYNAMIC_STATE_STENCIL_OP,
        };
        cfg.dynamic_states = dyn_states;
        cfg.dynamic_state_count = ARR_SIZE(dyn_states);

        //////////////
        // Viewports //
        //////////////
        // Dynamic - don't care
        cfg.viewports = nullptr;
        // Fixed
        cfg.vp_count = 1;

        /////////////
        // Scissor //
        /////////////
        // Dynamic - don't care
        cfg.scissors = nullptr;
        // Fixed
        cfg.scissor_count = 1;

        ////////////////////
        // Input Assembly //
        ////////////////////
        cfg.input_assembly.primitive_restart_enable = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_PRIMITIVE_RESTART_ENABLED);
        cfg.input_assembly.primitive_topology = get_vk_prim_topoloty(cur_desc->topology);

        /////////////////
        // Tesselation //
        /////////////////
        cfg.tessellation.patch_control_points = cur_desc->tess_patch_control_points;

        ///////////////////
        // Rasterization //
        ///////////////////
        cfg.raster.depth_clamp_enable = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_CLAMP_DEPTH);
        cfg.raster.rasterizer_discard_enable = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_DISCARD_RASTERIZER);
        cfg.raster.polygon_mode = get_vk_polygon_mode(cur_desc->poly_mode);
        // Fixed
        cfg.raster.line_width = 1.0f;
        cfg.raster.depth_bias_enable = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_DEPTH_BIAS);
        // Dynamic - don't care
        cfg.raster.cull_mode = VK_CULL_MODE_BACK_BIT;
        // Dynamic - don't care
        cfg.raster.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        // All of these valuess are dynamic - don't care
        cfg.raster.depth_bias_constant_factor = 0.0f;
        cfg.raster.depth_bias_slope_factor = 0.0f;
        cfg.raster.depth_bias_clamp = 0.0f;

        ///////////////
        // Blending  //
        ///////////////
        // Dynmic - don't care
        cfg.col_blend.blend_constants = {1.0f};
        cfg.col_blend.logic_op = get_vk_logic_op(cur_desc->logic_op);
        cfg.col_blend.logic_op_enabled = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_BLEND_LOGIC_OP);

        // We are setting all color attachments to have same blending - not all GPUs support different blending per att, so this is easier
        // to manage and more compatible.
        cfg.col_blend.attachments.size = get_rbp_slot_count(*rbp_pass, RBP_RES_USAGE_FLAG_COLOR_ATTACHMENT);
        for (u32 i = 0; i < cfg.col_blend.attachments.size; ++i) {
            auto cfg_att = &cfg.col_blend.attachments[i];
            auto desc_att = &cur_desc->atts_blending[i];
            cfg_att->blendEnable = desc_att->blend_enable;
            // Directly converts - just typedeffed u32
            cfg_att->colorWriteMask = desc_att->write_mask;

            // Color op
            cfg_att->colorBlendOp = get_vk_blend_op(desc_att->color.op);
            cfg_att->srcColorBlendFactor = get_vk_blend_factor(desc_att->color.src);
            cfg_att->dstColorBlendFactor = get_vk_blend_factor(desc_att->color.dst);

            // Alpha op
            cfg_att->alphaBlendOp = get_vk_blend_op(desc_att->alpha.op);
            cfg_att->srcAlphaBlendFactor = get_vk_blend_factor(desc_att->alpha.src);
            cfg_att->dstAlphaBlendFactor = get_vk_blend_factor(desc_att->alpha.dst);
        }

        ///////////////////
        // Depth Stencil //
        ///////////////////
        cfg.depth_stencil.depth_test_enable = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_DEPTH_TEST);
        cfg.depth_stencil.depth_write_enable = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_DEPTH_WRITE);
        cfg.depth_stencil.depth_compare_op = get_vk_compare_op(cur_desc->depth_compare_op);
        cfg.depth_stencil.depth_bounds_test_enable = test_flags(cur_desc->tmask, RTECHNIQUE_DESC_FLAG_DEPTH_BOUNDS_TEST);
        cfg.depth_stencil.min_depth_bounds = cur_desc->depth_bounds.x;
        cfg.depth_stencil.max_depth_bounds = cur_desc->depth_bounds.y;
        // Dynamic - don't care
        cfg.depth_stencil.stencil_test_enable = false;
        // Dynamic - don't care
        cfg.depth_stencil.front = {};
        // Dynamic - don't care
        cfg.depth_stencil.back = {};

        /////////////////////
        // Create pipeline //
        /////////////////////
        key_t key = ((u64)rtech.hndl.si << 32) | ((u64)cur_desc->bp_info.pid << 16) | (u64)cur_desc->bp_info.spi;        
        ilog("Creating new pipeline for key %lu", key);
        auto new_slot = acquire_slot(&rndr->pline_cache.items);
        asrt(is_valid(new_slot) && "Out of pipeline slots");
        int result = vkr_init_pipeline((VkPipeline*)&new_slot.item->gpu_d, cfg, &rndr->vk);
        asrt(result == err_code::VKR_NO_ERROR);
        asrt(hmap_insert(&rndr->pline_cache.key_lut, key, new_slot.hndl));

        // Set the technique values
        rtech.item->rpass_plines[i].bp_pass = cur_desc->bp_info.pid;
        rtech.item->rpass_plines[i].subpass = cur_desc->bp_info.spi;
        rtech.item->rpass_plines[i].pline = new_slot.hndl;
        
        ++rtech.item->rpass_plines.size;
    }
    return rtech.hndl;
}

rmaterial_handle create_rmaterial(renderer *rndr, const rmaterial_desc &ctinfo)
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
    tref.item->iv_cfg.srange.baseArrayLayer = 0;
    tref.item->iv_cfg.srange.layerCount = 1;
    tref.item->iv_cfg.srange.baseMipLevel = 0;
    tref.item->iv_cfg.srange.levelCount = 1;
    tref.item->iv_cfg.srange.aspectMask = is_color ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto cur_i = &tref.item->frames[i];

        // Set config pointers
        cur_i->cfg = &tref.item->cfg;
        cur_i->iv_cfg = &tref.item->iv_cfg;

        int result = vkr_init_image(&cur_i->image, *cur_i->cfg);
        asrt(result == err_code::VKR_NO_ERROR);

        // Update image pointer, set config
        tref.item->iv_cfg.image = &cur_i->image;
        result = vkr_init_image_view(&cur_i->view, *cur_i->iv_cfg, &rndr->vk);
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
    btref.item->cfg = ci.cfg;
    btref.item->cfg.user_data = btref.item->name;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto cur_b = &btref.item->frames[i];
        // Update config pointer
        cur_b->cfg = &btref.item->cfg;
        int result = vkr_init_buffer(&cur_b->buffer, *cur_b->cfg);
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

bool init_renderer(renderer *rndr, const renderer_cfg &p)
{
    asrt(p.upsream->alloc_type != mem_alloc_type::POOL); // Cannot use pool arena here
    init_fl_arena(&rndr->persist_fl, p.persist_fl_size, p.upsream, "rndr-persist-fl");
    init_stack_arena(&rndr->scratch_stack, p.scratch_stack_size, p.upsream, "rndr-sratch-stack");
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
                 .li_flags = VKR_LOG_INFO_NONE,
                 .extra_instance_extension_names = ADDITIONAL_INST_EXTENSIONS,
                 .extra_instance_extension_count = ADDITIONAL_INST_EXTENSION_COUNT,
                 .device_extension_names = DEVICE_EXTENSIONS,
                 .device_extension_count = DEVICE_EXTENSION_COUNT,
                 .validation_layer_names = VALIDATION_LAYERS,
                 .validation_layer_count = VALIDATION_LAYER_COUNT};
    s32 result = vkr_init(&vkii, &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) {
        return false;
    }

    // Blueprints
    init_blueprints(rndr);

    // Pipeline cache (no pipelines yet)
    init_gpu_resource_cache(rndr, &rndr->pline_cache, 32);

    // Framebuffer cache (no framebuffers yet)
    init_gpu_resource_cache(rndr, &rndr->fb_cache, 16);

    // Geometry stream groups
    init_geometry_stream_groups(rndr);

    // Render resources
    init_render_resources(rndr, p);

    // Resource targets
    init_resource_target_registry(rndr);

    // Create transient command pool
    result = vkr_init_cmd_pool(&rndr->transient_pool,
                               rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].fam_ind,
                               VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                               &rndr->vk);
    if (result != err_code::VKR_NO_ERROR) return false;

    // Setup frames in flight
    if (!init_frame_contexts(rndr, p.thread_count)) return false;

    // Samplers - must come before descriptor set layouts
    if (!init_global_samplers(rndr)) return false;

    // Descriptor set layouts
    if (!init_global_descriptor_info(rndr, p.desc)) return false;

    // Start timeer
    ptimer_restart(&rndr->pt);

    // Setup our indice and vert buffer sbuffer
    return true;
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

    // Global descriptor set layouts
    terminate_global_descriptor_info(rndr);

    // Global samplers - must come after descriptor set layouts
    terminate_global_samplers(rndr);

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
    terminate_arena(&rndr->scratch_stack);
    terminate_arena(&rndr->persist_fl);
}

} // namespace nslib
