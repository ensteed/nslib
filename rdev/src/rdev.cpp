#include "asset_common.h"
#include "json_archive.h"
#include "platform.h"
#include "renderer.h"
#include "input_mapping.h"
#include "sim_region.h"
#include "basic_types.h"
#include "engine_rendering.h"
#include "render_manifest.h"
#include "vkr_texture_pool.h"
#include "profiling.h"
using namespace nslib;

#ifdef USE_IMGUI
    #include "imgui/imgui.h"
#endif

// intern constexpr const char * MAIN_PASS_COLOR_NAME = "main-pass-color";
// intern const rres_id MAIN_PASS_COLOR_ID = hash_type("main-pass-color");

intern constexpr const char *FWD_PBR_RBP = "fwd-pbr";
intern const rres_id FWD_PBR_RBP_ID = hash_type(FWD_PBR_RBP);

intern constexpr const char *MAIN_PASS_DEPTH_NAME = "main-pass-depth";
intern const rres_id MAIN_PASS_DEPTH_ID = hash_type(MAIN_PASS_DEPTH_NAME);

intern constexpr const char *MAIN_PASS_NAME = "main-pass";
intern const rres_id MAIN_PASS_ID = hash_type(MAIN_PASS_NAME);

intern constexpr const char *IMGUI_PASS_NAME = "imgui-pass";
intern const rres_id IMGUI_PASS_ID = hash_type(IMGUI_PASS_NAME);

intern constexpr const char *DIFFUSE_TECH = "fwd-diffuse";

// Default texture pool configs
const rtexture_pool_cfg TPOOL_CFGS[] = {
    {
        .tmeta{
            .fmt = RFMT_RGBA8_SRGB,
            .dims{511, 511},
            .mip_levels = 1,
            .flags = RTEXTURE_FLAG_NONE,
        },
        .pool_name = "dport_pool",
        .slot_count = 1,
    },
    {
        .tmeta{
            .fmt = RFMT_RGBA8_SRGB,
            .dims{600, 600},
            .mip_levels = 1,
            .flags = RTEXTURE_FLAG_NONE,
        },
        .pool_name = "mport_pool",
        .slot_count = 1,
    },
};

constexpr u32 MAX_INSTANCES = 5000;

const rpipeline_layout_cfg PL_LAYOUT_CFG{
    .view_ssbo_block_sz = sizeof(view_ssbo_data),
    .pass_ssbo_block_sz = sizeof(pass_ssbo_data),
    .frame_ubo_block_sz = sizeof(frame_ubo_data),
    .instance_ssbo{MAX_INSTANCES, sizeof(instance_ssbo_data)},
    .material_ssbo{256, sizeof(material_ssbo_data)},
    .push_const_range_count = 0,
    .push_const_ranges = nullptr,
};

const manifest_max_counts MANIFEST_MAX_COUNTS{
    .passes = 10,
    .views = 10,
    .render_jobs = 10,
    .draw_calls_per_job = MAX_INSTANCES,
    .texture_targets = 10,
    .buffer_targets = 10,
};

const renderer_cfg RNDR_CFG{
    .persist_fl_size = 200 * MB_SIZE,
    .scratch_stack_size = 10 * MB_SIZE,
    .extra_frame_linear_size = 5 * MB_SIZE,
    .desc{PL_LAYOUT_CFG},
    .mcounts{MANIFEST_MAX_COUNTS},
    .texture_pool_count = ARR_SIZE(TPOOL_CFGS),
    .texture_pool_cfgs = TPOOL_CFGS,
};

struct rdev_app_ctxt
{
    renderer rndr;
    sim_region rgn;
    asset_cache cg;
    f64 accumulater;

    input_keymap movement_km;
    input_keymap global_km;
    input_keymap_stack stack;

    u32 cam_id;
    vec2 mpos;
    svec2 movement;

    u32 cube_1;
    u32 plane_1;
};

intern void setup_camera_controller(platform_ctxt *ctxt, rdev_app_ctxt *app)
{
    // Create camera
    auto sz = get_window_pixel_size(ctxt->win_hndl);
    auto cam = add_entity("Editor_Cam", &app->rgn);
    auto cam_comp = add_comp<camera>(cam);
    auto cam_tcomp = add_comp<transform>(cam);

    cam_comp->fov = 60.0f;
    cam_comp->near_far = {0.1f, 1000.0f};
    cam_comp->view = (math::look_at(vec3{0.0f, 10.0f, -5.0f}, vec3{0.0f}, vec3{0.0f, 1.0f, 0.0f}));
    cam_comp->proj = math::perspective(cam_comp->fov, 1000.0f/800.0f, cam_comp->near_far.x, cam_comp->near_far.y);

    cam_tcomp->cached = math::inverse(cam_comp->view);
    cam_tcomp->orientation = math::orientation(cam_tcomp->cached);
    cam_tcomp->scale = math::scaling_vec(cam_tcomp->cached);
    cam_tcomp->world_pos = math::translation_vec(cam_tcomp->cached);
    app->cam_id = cam->id;

    cam_tcomp->cached = math::model_tform(cam_tcomp->world_pos, cam_tcomp->orientation, cam_tcomp->scale);
    cam_comp->view = math::inverse(cam_tcomp->cached);

    // Add our input trigger functions
    auto cam_turn_func = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
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
        camt->flags |= COMP_FLAG_DIRTY;
    };

    auto move_forward_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        app->movement.y += (t.ev->key.action - 1) * (-2) + 1;
    };
    auto move_back_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        app->movement.y -= (t.ev->key.action - 1) * (-2) + 1;
    };
    auto move_right_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        app->movement.x += (t.ev->key.action - 1) * (-2) + 1;
    };
    auto move_left_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
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

intern void create_entity_grid(sim_region *region, const geometry &cube_geom, const geometry &rect_geom, const material &mat)
{
    // Create a grid of entities with odd ones being cubes and even being rectangles
    int len = 20, width = 20, height = 5;
    auto ent_offset = add_entities(len * width * height, region);

    auto tf_tbl = get_comp_tbl<transform>(&region->cdb);
    for (sizet zind = 0; zind < height; ++zind) {
        for (sizet yind = 0; yind < len; ++yind) {
            for (sizet xind = 0; xind < width; ++xind) {
                sizet ent_ind = zind * (width * len) + yind * width + xind + ent_offset;
                auto ent = &region->ents[ent_ind];
                auto tfcomp = add_comp<transform>(ent->id, tf_tbl);
                auto sc = add_comp<static_mesh>(ent);
                if (xind % 2) {
                    sc->geom_id = cube_geom.id;
                    arr_emplace_back(&sc->mat_mapping, mat.id, 0);
                    ent->name = to_str("cube-%d", ent_ind);
                }
                else {
                    sc->geom_id = rect_geom.id;
                    arr_emplace_back(&sc->mat_mapping, mat.id, 0);
                    ent->name = to_str("rect-%d", ent_ind);
                }
                tfcomp->world_pos = vec3{xind * 2.0f, yind * 2.0f, zind * 2.0f};
                tfcomp->cached = math::model_tform(tfcomp->world_pos, tfcomp->orientation, tfcomp->scale);
            }
        }
    }
}

intern render_blueprint_ref build_and_compile_render_blueprint(renderer *rndr, rdev_app_ctxt *app)
{
    // First, create the needed target resources
    auto rbp = create_render_blueprint(rndr, FWD_PBR_RBP);

    auto pass_id = add_rbp_pass(rbp.item,
                                {
                                    .name = MAIN_PASS_NAME,
                                    .type = PASS_TYPE_GRAPHICS,
                                    .use_subpass_bookends = true,
                                    .geom_streams_group = MAIN_GEOM_STREAM_GP_ID,
                                });

    auto imgui_pass_id = add_rbp_pass(rbp.item,
                                      {
                                          .name = IMGUI_PASS_NAME,
                                          .type = PASS_TYPE_GRAPHICS,
                                          .use_subpass_bookends = true,
                                      });

    // Main geometry pass
    idx_t col_slot_ind = add_rbp_resource_slot(
        rbp.item, pass_id, {.name = "color", .format = get_swapchain_format(rndr), .usage = RBP_RES_USAGE_COLOR_ATTACHMENT});
    idx_t depth_slot_ind =
        add_rbp_resource_slot(rbp.item, pass_id, {.name = "depth", .format = RFMT_D32_SFLOAT, .usage = RBP_RES_USAGE_DEPTH_ATTACHMENT});

    add_rbp_resource_requirement(rbp.item,
                                 pass_id,
                                 {
                                     .slot_ind = col_slot_ind,
                                     .access_mask = RESOURCE_REQUIREMENT_ACCESS_WRITE | RESOURCE_REQUIREMENT_ACCESS_CLEAR,
                                     .visibility = RSHADER_STAGE_FRAGMENT_BIT,
                                 });

    add_rbp_resource_requirement(rbp.item,
                                 pass_id,
                                 {
                                     .slot_ind = depth_slot_ind,
                                     .access_mask = RESOURCE_REQUIREMENT_ACCESS_WRITE | RESOURCE_REQUIREMENT_ACCESS_CLEAR,
                                     .visibility = RSHADER_STAGE_FRAGMENT_BIT,
                                 });

    // I'm gui will double as the place we render UI and the place where our layout conversion happens - no depth buffer
    // needed for imgui
    idx_t imgui_col_slot_ind = add_rbp_resource_slot(
        rbp.item, imgui_pass_id, {.name = "color", .format = get_swapchain_format(rndr), .usage = RBP_RES_USAGE_COLOR_ATTACHMENT});

    add_rbp_resource_requirement(rbp.item,
                                 imgui_pass_id,
                                 {
                                     .slot_ind = imgui_col_slot_ind,
                                     .access_mask = RESOURCE_REQUIREMENT_ACCESS_WRITE | RESOURCE_REQUIREMENT_ACCESS_READ,
                                     .visibility = RSHADER_STAGE_FRAGMENT_BIT,
                                     .option_mask = RESOURCE_REQUIREMENT_OPTION_PRESENT_KHR,
                                 });

    dlog("Blueprint: %s", ls(to_json(*rbp.item)));
    compile_render_blueprint(rndr, rbp.item);
    return rbp;
}

intern void create_geometry(geometry_pool *geom_pool, geometry **rect, geometry **cube)
{
    auto cube_geom = create_asset(geom_pool, "rect");
    auto rect_geom = create_asset(geom_pool, "cube");
    make_unit_rect(rect_geom.item);
    make_unit_cube(cube_geom.item);
    *rect = rect_geom.item;
    *cube = cube_geom.item;
}

intern void create_textures(texture_pool *tex_pool)
{
    auto daniel_face = create_asset(tex_pool, "daniel-face");
    auto maria_face = create_asset(tex_pool, "maria-face");
    cstr err = load_texture(maria_face.item, "import/maria.png");
    if (err) {
        wlog("Couldn't load texture: %s", ls(daniel_face.item->name), err);
    }
    err = load_texture(daniel_face.item, "import/daniel.png");
    if (err) {
        wlog("Couldn't load texture %s: %s", ls(maria_face.item->name), err);
    }
}

intern technique_item_ref create_diffuse_technique(shader_pool *spool, technique_pool *tpool)
{
    auto shdr = create_asset(spool, "fwd-diffuse");
    const char *path = "data/shaders/fwd-diffuse";
    const char *err = load_shader(shdr.item, path);
    if (err) {
        wlog("Failed to load shader at %s: %s", path, err);
    }

    auto tech = create_asset(tpool, DIFFUSE_TECH);
    tech.item->bpid = FWD_PBR_RBP_ID;

    // All default states are good here
    technique_pass p{};
    p.shader = shdr.item->id;
    p.bp_pass = MAIN_PASS_ID;
    p.bp_subpass = 0;
    p.gsg_layout = 0;
    arr_push_back(&tech.item->passes, p);
    return tech;
}

intern b32 init_rdev(platform_ctxt *ctxt, rdev_app_ctxt *app)
{
    init_asset_cache_default_types(
        &app->cg,
        "asset-cache",
        {.free_list = get_global_arena(), .frame_linear = get_global_frame_lin_arena(), .stack = get_global_stack_arena()});

    auto geom_pool = get_asset_pool<geometry>(&app->cg);
    auto tex_pool = get_asset_pool<texture>(&app->cg);
    auto shdr_pool = get_asset_pool<shader>(&app->cg);
    auto tech_pool = get_asset_pool<technique>(&app->cg);
    auto mat_pool = get_asset_pool<material>(&app->cg);

    geometry *rect, *cube;
    create_geometry(geom_pool, &rect, &cube);
    create_textures(tex_pool);
    technique_item_ref diff_tech = create_diffuse_technique(shdr_pool, tech_pool);

    material_item_ref default_mat = create_asset(mat_pool, "default");
    arr_emplace_back(&default_mat.item->bp_techniques, FWD_PBR_RBP_ID, diff_tech.item->id);

    renderer_cfg p{RNDR_CFG};
    p.win_hndl = ctxt->win_hndl;
    
    p.upsream = &ctxt->arenas.free_list;
    if (!init_renderer(&app->rndr, p)) return false;

    auto geom_stream_gp = setup_geometry_stream_group(&app->rndr);
    auto rbp = build_and_compile_render_blueprint(&app->rndr, app);

#ifdef USE_IMGUI
    auto pass_id = find_rbp_pass(rbp.item, IMGUI_PASS_ID);
    init_imgui(&app->rndr, rbp.item->passes[pass_id]);
#endif

    upload_geometries(&app->rndr, geom_stream_gp, geom_pool, &ctxt->arenas.stack);
    upload_textures(&app->rndr, tex_pool, &ctxt->arenas.stack);
    upload_shaders(&app->rndr, shdr_pool, &ctxt->arenas.stack);
    upload_techniques(&app->rndr, tech_pool, shdr_pool, &ctxt->arenas.stack);

    // Create render targets
    // create_rtexture_target(&app->rndr, TEXTURE_TARGET_COLOR(MAIN_PASS_COLOR_NAME));
    create_rtexture_target(&app->rndr, TEXTURE_TARGET_DEPTH(MAIN_PASS_DEPTH_NAME));

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
    create_entity_grid(&app->rgn, *cube, *rect, *default_mat.item);
    return true;
}

intern void simulate(platform_ctxt *ctxt, rdev_app_ctxt *app, f64 dt)
{
    // Move the cam if needed
    auto cam = get_comp<camera>(app->cam_id, &app->rgn.cdb);
    if (app->movement != svec2{}) {
        auto cam_tform = get_comp<transform>(app->cam_id, &app->rgn.cdb);
        auto right = math::right_vec(cam_tform->orientation);
        auto target = math::target_vec(cam_tform->orientation);
        cam_tform->world_pos += (right * app->movement.x + target * app->movement.y) * dt * 10;
        cam_tform->flags |= COMP_FLAG_DIRTY;
    }
    static double update_tm = 0.0;
    static double render_tm = 0.0;

    auto tform_tbl = get_comp_tbl<transform>(&app->rgn.cdb);
    auto mat_cache = get_asset_pool<material>(&app->cg);
    auto geom_cache = get_asset_pool<geometry>(&app->cg);
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
            curtf->flags |= COMP_FLAG_DIRTY;
        }
    }
}

intern void build_manifest(rmanifest *m, rdev_app_ctxt *app)
{
    auto bp_main_pass = find_rbp_pass(m->rbp.item, MAIN_PASS_ID);
    auto imgui_pass = find_rbp_pass(m->rbp.item, IMGUI_PASS_ID);

    pass_ssbo_data pdata{};

    auto text_target = find_rtexture_target(m->rndr, SWAPCHAIN_ID);
    auto text_target_ptr = get_rtexture_target(m->rndr, text_target);
    asrt(text_target_ptr);
    pdata.resolution = text_target_ptr->cfg.dims.xy;
    pdata.inv_resolution = 1.0f / pdata.resolution;

    mpass_params p{};
    p.rbpp = bp_main_pass;
    p.pass_sdata = &pdata;
    auto mp_id = push_pass(m, p);
    push_slot_assignment(m, mp_id, MPASS_TEXTURE_SA_CCF(find_rtexture_target(m->rndr, SWAPCHAIN_ID), vec4(0.0f, 1.0f, 1.0f, 1.0f)));
    push_slot_assignment(m, mp_id, MPASS_TEXTURE_SA_DS(find_rtexture_target(m->rndr, MAIN_PASS_DEPTH_ID), 1.0f, 0));

    idx_t imgui_id = push_pass(m, {.rbpp = imgui_pass});
    push_slot_assignment(m, imgui_id, MPASS_TEXTURE_SA_CCF(find_rtexture_target(m->rndr, SWAPCHAIN_ID), vec4(0.0f, 1.0f, 1.0f, 1.0f)));

    auto cam = get_comp<camera>(app->cam_id, &app->rgn.cdb);
    auto cam_tform = get_comp<transform>(app->cam_id, &app->rgn.cdb);

    view_ssbo_data vdata{};
    vdata.view = cam->view;
    vdata.proj = cam->proj;
    vdata.view_proj = cam->proj * cam->view;
    vdata.inv_view_proj = math::inverse(vdata.view_proj);

    mview_params vp{};
    vp.view_sdata = &vdata;
    vp.vdata.norm_scissor = {0.0, 0.0, 1, 1};

    auto view_id = push_view(m, vp);
    auto imgui_view_id = push_view(m, {});

    push_render_job(m, {.pass = mp_id, .view = view_id, .max_draw_calls = MAX_INSTANCES, .cb = draw_geometry, .cb_user = nullptr});
    push_render_job(m, {.pass = imgui_id, .view = imgui_view_id, .cb = draw_imgui, .cb_user = nullptr});

    update_and_draw_region(m, &app->rgn, &app->cg);
}

intern bool run_frame(platform_ctxt *ctxt, rdev_app_ctxt *app)
{
    PROFILE_BEGIN_FRAME();
    begin_platform_frame(ctxt);

    app->accumulater += ctxt->time_pts.dt;
    map_input_frame(&app->stack, &ctxt->feventq);

    PROFILE_BEGIN("simulate");
    while (app->accumulater >= 0.01666) {
        simulate(ctxt, app, 0.01666);
        app->accumulater -= 0.01666;
    }
    f64 alpha = app->accumulater / 0.010;
    PROFILE_END();

    frame_ubo_data fdata{};
    fdata.frame_count = app->rndr.finished_frames;
    fdata.dt = ctxt->time_pts.dt;
    fdata.elapsed = ptimer_elapsed_dt(&ctxt->time_pts);

    auto bp = find_render_blueprint(&app->rndr, FWD_PBR_RBP_ID);
    rmanifest *m = begin_render_frame(&app->rndr, {.rbp = bp, .frame_sdata = &fdata});
    if (!m) return true;

    build_manifest(m, app);

// Gather visible items and do stuff
#ifdef USE_IMGUI
    ImGui::ShowDebugLogWindow();
#endif

    bool res = end_render_frame(m);
    end_platform_frame(ctxt);

    PROFILE_END_FRAME();

#if defined(PROFILING_ENABLED)
    static u32 frame_count_goal = ctxt->finished_frames + GLOBAL_PROFILING_CONTEXT[0]->avg_window;
    if (ctxt->finished_frames > frame_count_goal) {
        PROFILE_PRINT_REPORT();
        frame_count_goal += GLOBAL_PROFILING_CONTEXT[0]->avg_window;
    }
#endif
    return res;
}

intern void terminate_rdev(platform_ctxt *ctxt, rdev_app_ctxt *app)
{
    terminate_renderer(&app->rndr);
    terminate_keymap(&app->global_km);
    terminate_keymap(&app->movement_km);
    terminate_keymap_stack(&app->stack);
    terminate_sim_region(&app->rgn);
    terminate_asset_cache_default_types(&app->cg);
}

int main(int argc, char **argv)
{
    rdev_app_ctxt app{};
    platform_ctxt ctxt{};

    platform_init_info pf_config{argc, argv};
    pf_config.flags = PLATFORM_INIT_FLAG_AUDIO | PLATFORM_INIT_FLAG_WINDOW;
    pf_config.wind.resolution = {1000, 800};
    pf_config.wind.title = "RDev";
    pf_config.wind.win_flags = WINDOW_RESIZABLE | WINDOW_INPUT_FOCUS | WINDOW_VULKAN | WINDOW_SHOWN | WINDOW_ALLOW_HIGHDPI;
    pf_config.default_log_level = LOG_DEBUG;
    pf_config.mem.free_list_size = 4 * 1024 * MB_SIZE;

    int result = init_platform(&pf_config, &ctxt);
    if (result != err_code::PLATFORM_NO_ERROR) {
        return result;
    }
    ctxt.running = init_rdev(&ctxt, &app);
    if (!ctxt.running) elog("User init failed with code %d", result);

    while (ctxt.running && run_frame(&ctxt, &app))
        ;

    terminate_rdev(&ctxt, &app);

    return terminate_platform(&ctxt);
}
