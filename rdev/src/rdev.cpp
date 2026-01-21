#include "asset_common.h"
#include "platform.h"
#include "renderer.h"
#include "input_mapping.h"
#include "sim_region.h"
#include "basic_types.h"
#include "fwd_render.h"
using namespace nslib;

#ifdef USE_IMGUI
    #include "imgui/imgui.h"
#endif

struct app_data
{
    renderer rndr{};
    sim_region rgn{};
    asset_cache cg{};
    f64 accumulater{};

    input_keymap movement_km;
    input_keymap global_km;
    input_keymap_stack stack{};

    u32 cam_id;
    vec2 mpos;
    svec2 movement{};

    u32 cube_1;
    u32 plane_1;
};

intern void setup_camera_controller(platform_ctxt *ctxt, app_data *app)
{
    // Create camera
    auto sz = get_window_pixel_size(ctxt->win_hndl);
    auto cam = add_entity("Editor_Cam", &app->rgn);
    auto cam_comp = add_comp<camera>(cam);
    auto cam_tcomp = add_comp<transform>(cam);

    cam_comp->fov = 60.0f;
    cam_comp->near_far = {0.1f, 1000.0f};
    cam_comp->view = (math::look_at(vec3{0.0f, 10.0f, -5.0f}, vec3{0.0f}, vec3{0.0f, 1.0f, 0.0f}));

    cam_tcomp->cached = math::inverse(cam_comp->view);
    cam_tcomp->orientation = math::orientation(cam_tcomp->cached);
    cam_tcomp->scale = math::scaling_vec(cam_tcomp->cached);
    cam_tcomp->world_pos = math::translation_vec(cam_tcomp->cached);
    app->cam_id = cam->id;

    cam_tcomp->cached = math::model_tform(cam_tcomp->world_pos, cam_tcomp->orientation, cam_tcomp->scale);
    cam_comp->view = math::inverse(cam_tcomp->cached);

    // Add our input trigger functions
    auto cam_turn_func = [](const input_trigger &t, void *data) {
        auto app = (app_data *)data;
        auto cam_ent = get_entity(app->cam_id, &app->rgn);
        auto camc = get_comp<camera>(cam_ent);
        auto camt = get_comp<transform>(cam_ent);

        auto delta = t.ev->mmotion.norm_delta;

        vec3 right = math::right_vec(camt->orientation);
        f32 factor = 2.5;
        vec4 horizontal = {right, -(f32)delta.y * factor};
        vec4 vertical = {{0, 0, 1}, (f32)delta.x * factor};
        camt->orientation = math::orientation(vertical) * math::orientation(horizontal) * camt->orientation;
        camt->cached = math::model_tform(camt->world_pos, camt->orientation, camt->scale);
        camc->view = math::inverse(camt->cached);
    };

    auto move_forward_action = [](const input_trigger &t, void *data) {
        auto app = (app_data *)data;
        app->movement.y += (t.ev->key.action - 1) * (-2) + 1;
    };
    auto move_back_action = [](const input_trigger &t, void *data) {
        auto app = (app_data *)data;
        app->movement.y -= (t.ev->key.action - 1) * (-2) + 1;
    };
    auto move_right_action = [](const input_trigger &t, void *data) {
        auto app = (app_data *)data;
        app->movement.x += (t.ev->key.action - 1) * (-2) + 1;
    };
    auto move_left_action = [](const input_trigger &t, void *data) {
        auto app = (app_data *)data;
        app->movement.x -= (t.ev->key.action - 1) * (-2) + 1;
    };

    set_input_trigger(&app->stack, "cam-turn", {cam_turn_func, app});
    set_input_trigger(&app->stack, "move-forward", {move_forward_action, app});
    set_input_trigger(&app->stack, "move-back", {move_back_action, app});
    set_input_trigger(&app->stack, "move-right", {move_right_action, app});
    set_input_trigger(&app->stack, "move-left", {move_left_action, app});

    set_keymap_entry(&app->global_km, KMCODE_MMOTION, 0, MBUTTON_MASK_MIDDLE, {"cam-turn"});

    set_keymap_entry(&app->movement_km, KMCODE_KEY_W, 0, 0, {"move-forward", INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});
    set_keymap_entry(&app->movement_km, KMCODE_KEY_S, 0, 0, {"move-back", INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});
    set_keymap_entry(&app->movement_km, KMCODE_KEY_D, 0, 0, {"move-right", INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});
    set_keymap_entry(&app->movement_km, KMCODE_KEY_A, 0, 0, {"move-left", INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});

    // Make our movement keymap not care about any modifiers at all - we always move no matter what
    app->movement_km.kmod_mask = KEYMOD_NONE;
    app->movement_km.mbutton_mask = MBUTTON_MASK_NONE;
}

intern void create_entity_grid(sim_region *region, const mesh &cube_msh, const mesh &rect_msh)
{
    // Create a grid of entities with odd ones being cubes and even being rectangles
    int len = 10, width = 10, height = 10;
    auto ent_offset = add_entities(len * width * height, region);

    auto tf_tbl = get_comp_tbl<transform>(&region->cdb);
    for (sizet zind = 0; zind < height; ++zind) {
        for (sizet yind = 0; yind < len; ++yind) {
            for (sizet xind = 0; xind < width; ++xind) {
                sizet ent_ind = zind * (width * len) + yind * width + xind + ent_offset;
                auto ent = &region->ents[ent_ind];
                auto tfcomp = add_comp<transform>(ent->id, tf_tbl);
                auto sc = add_comp<static_model>(ent);
                if (xind % 2) {
                    sc->mesh_id = cube_msh.id;
                    ent->name = to_str("cube-%d", ent_ind);
                }
                else {
                    sc->mesh_id = rect_msh.id;
                    ent->name = to_str("rect-%d", ent_ind);
                }
                tfcomp->world_pos = vec3{xind * 2.0f, yind * 2.0f, zind * 2.0f};
                tfcomp->cached = math::model_tform(tfcomp->world_pos, tfcomp->orientation, tfcomp->scale);
            }
        }
    }
}

void create_meshes(mesh_pool *msh_pool, mesh **rect, mesh **cube)
{
    auto cube_msh = create_asset(msh_pool, "rect");
    auto rect_msh = create_asset(msh_pool, "cube");
    make_rect(rect_msh.item);
    make_cube(cube_msh.item);
    *rect = rect_msh.item;
    *cube = cube_msh.item;
}

void create_textures(texture_pool *tex_pool)
{
    auto daniel_face = create_asset(tex_pool, "daniel-face");
    auto maria_face = create_asset(tex_pool, "maria-face");
    cstr err{nullptr};
    load_texture(maria_face.item, "import/maria.png", &err);
    if (err) {
        wlog("Couldn't load texture: %s", ls(daniel_face.item->name), err);
    }
    load_texture(daniel_face.item, "import/daniel.png", &err);
    if (err) {
        wlog("Couldn't load texture %s: %s", ls(maria_face.item->name), err);
    }
}

void build_render_blueprint(render_blueprint *bp)
{
    // auto pass = create_pass(bp);
}

void build_and_compile_render_blueprint(renderer *rndr) {
    // First, create the needed target resources
    auto rbp = create_render_blueprint("fwd-pbr", rndr);
    create_rbp_target_texture(rbp, RTARGET_SWAPCHAIN_IMAGE);
    
    auto depth = create_rbp_target_texture(rbp, "depth");
    depth->tinfo.img_cfg.dims = {rndr->vk.inst.device.swapchain.extent.width, rndr->vk.inst.device.swapchain.extent.height, 1};
    depth->tinfo.img_cfg.format = vkr_find_best_depth_format(&rndr->vk.inst.pdev_info);
    depth->tinfo.img_cfg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth->tinfo.img_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    depth->tinfo.img_cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;
    depth->tinfo.img_view_cfg.srange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    
    auto pass = add_rbp_pass(rbp, "main");
    pass->use_subpass_bookends = true;

    auto req = add_rbp_pass_res_requirement(pass);
    req->usage = rtarget_res_usage::COLOR_ATTACHMENT;
    req->visibility = VISIBILITY_FRAGMENT;
    req->access_mask = RES_REQUIREMENT_ACCESS_FLAG_WRITE | RES_REQUIREMENT_ACCESS_FLAG_CLEAR;
    req->resid = depth->id;

    req = add_rbp_pass_res_requirement(pass);
    req->usage = rtarget_res_usage::COLOR_ATTACHMENT;
    req->visibility = VISIBILITY_FRAGMENT;
    req->access_mask = RES_REQUIREMENT_ACCESS_FLAG_WRITE | RES_REQUIREMENT_ACCESS_FLAG_CLEAR;
    req->resid = RTARGET_SWAPCHAIN_ID;

    compile_render_blueprint(rbp, rndr);
}

int init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    render_blueprint bp{};

    init_cache_default_types(&app->cg, "asset-cache", get_global_arena());

    // Create meshes
    auto msh_pool = get_pool<mesh>(&app->cg);
    auto tex_pool = get_pool<texture>(&app->cg);
    mesh *rect, *cube;
    create_meshes(msh_pool, &rect, &cube);
    create_textures(tex_pool);

    build_render_blueprint(&bp);

    // Initialize our renderer - fail early if init fails
    int ret = init_renderer(&app->rndr, ctxt->win_hndl, &ctxt->arenas.free_list);
    if (ret != err_code::RENDER_NO_ERROR) {
        return ret;
    }

    auto geom_stream_gp = setup_geometry_stream_group(&app->rndr);
    build_and_compile_render_blueprint(&app->rndr);

    upload_geometry(&app->rndr, geom_stream_gp, msh_pool, &ctxt->arenas.stack);
    upload_textures(&app->rndr, tex_pool, &ctxt->arenas.stack);

    // Create our sim region aka scene
    init_sim_region(&app->rgn, get_global_arena());

    // Create input map
    init_keymap_stack(&app->stack, &ctxt->arenas.free_list);
    init_keymap(&app->movement_km, "movement", &ctxt->arenas.free_list);
    init_keymap(&app->global_km, "global", &ctxt->arenas.free_list);

    push_keymap(&app->stack, &app->movement_km);
    push_keymap(&app->stack, &app->global_km);

    // Create and setup input for camera
    setup_camera_controller(ctxt, app);
    create_entity_grid(&app->rgn, *cube, *rect);
    return ret;
}

void simulate(platform_ctxt *ctxt, app_data *app, f64 dt)
{
    // Move the cam if needed
    auto cam = get_comp<camera>(app->cam_id, &app->rgn.cdb);
    if (app->movement != svec2{}) {
        auto cam_tform = get_comp<transform>(app->cam_id, &app->rgn.cdb);
        auto right = math::right_vec(cam_tform->orientation);
        auto target = math::target_vec(cam_tform->orientation);
        cam_tform->world_pos += (right * app->movement.x + target * app->movement.y) * dt * 10;
        cam_tform->cached = math::model_tform(cam_tform->world_pos, cam_tform->orientation, cam_tform->scale);
        cam->view = math::inverse(cam_tform->cached);
    }
    static double update_tm = 0.0;
    static double render_tm = 0.0;

    auto tform_tbl = get_comp_tbl<transform>(&app->rgn.cdb);
    auto mat_cache = get_pool<material>(&app->cg);
    auto msh_cache = get_pool<mesh>(&app->cg);
    for (sizet i = 0; i < tform_tbl->entries.size; ++i) {
        auto curtf = &tform_tbl->entries[i];
        if (curtf->ent_id != app->cam_id) {
            if (i % 3 == 0) {
                curtf->orientation *= math::orientation(vec4{1.0, 0.0, 0.0, (f32)dt});
            }
            else if (i % 3 == 2) {
                curtf->orientation *= math::orientation(vec4{0.0, 1.0, 0.0, (f32)dt});
            }
            else {
                curtf->orientation *= math::orientation(vec4{0.0, 0.0, 1.0, (f32)dt});
            }
            curtf->cached = math::model_tform(curtf->world_pos, curtf->orientation, curtf->scale);
        }
    }
}

int run_frame(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    auto cam = get_comp<camera>(app->cam_id, &app->rgn.cdb);
    profile_timepoints pt;

    static int ticks = 0;

    // Spin some entities
    ptimer_restart(&pt);
    static double update_tm{};
    static double render_tm{};

    app->accumulater += ctxt->time_pts.dt;

    map_input_frame(&app->stack, &ctxt->feventq);

    while (app->accumulater >= 0.01666) {
        ++ticks;
        simulate(ctxt, app, 0.01666);
        app->accumulater -= 0.01666;
    }

    f64 alpha = app->accumulater / 0.010;

    ptimer_split(&pt);
    update_tm += pt.dt;

    int res = begin_render_frame(&app->rndr, ctxt->finished_frames);

// Gather visible items and do stuff
#ifdef USE_IMGUI
    ImGui::ShowDebugLogWindow();
#endif
    // bool open{true};
    // ImGui::ShowDemoWindow();

    res = end_render_frame(&app->rndr, cam, ctxt->time_pts.dt);

    ptimer_split(&pt);
    render_tm += pt.dt;

    static double counter = 2.0;
    double elapsed = nanos_to_sec(ptimer_elapsed_dt(&ctxt->time_pts));
    if (elapsed > counter) {
        double tot_factor = 100 / (update_tm + render_tm);
        double render_factor = 100 / render_tm;
        double ticks_fps = (double)ticks / elapsed;
        ilog("Average FPS: %.02f  Update:%.02f%%  Render:%.02f%%",
             ctxt->finished_frames / elapsed,
             update_tm * tot_factor,
             render_tm * tot_factor);
        ilog("Simulation FPS: %.02f  Ticks: %d", ticks_fps, ticks);
        counter += 2.0;
        update_tm = 0.0;
        render_tm = 0.0;
    }

    return res;
}

int terminate(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    terminate_renderer(&app->rndr);
    terminate_keymap(&app->global_km);
    terminate_keymap(&app->movement_km);
    terminate_keymap_stack(&app->stack);
    terminate_sim_region(&app->rgn);
    terminate_cache_default_types(&app->cg);
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->flags = PLATFORM_INIT_FLAG_AUDIO | PLATFORM_INIT_FLAG_WINDOW;
    settings->wind.resolution = {1000, 800};
    settings->wind.title = "RDev";
    settings->wind.win_flags = WINDOW_RESIZABLE | WINDOW_INPUT_FOCUS | WINDOW_VULKAN | WINDOW_SHOWN | WINDOW_ALLOW_HIGHDPI;
    settings->default_log_level = LOG_DEBUG;
    settings->user_hooks.init = init;
    settings->user_hooks.run_frame = run_frame;
    settings->user_hooks.terminate = terminate;
    settings->mem.free_list_size = 4 * 1024 * MB_SIZE;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN_STATIC(app_data, configure_platform);
