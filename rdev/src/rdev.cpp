#include "asset_common.h"
#include "json_archive.h"
#include "platform.h"
#include "renderer.h"
#include "input_mapping.h"
#include "sim_region.h"
#include "basic_types.h"
#include "engine_rendering.h"
#include "render_manifest.h"
#include "render_thread.h"
#include "vkr_texture_pool.h"
#include "profiling.h"
using namespace nslib;

#ifdef USE_IMGUI
    #include "imgui/imgui.h"
#endif

// intern constexpr const char * MAIN_PASS_COLOR_NAME = "main-pass-color";
// intern const rid MAIN_PASS_COLOR_ID = make_rid("main-pass-color");

intern constexpr const char *FWD_PBR_RBP = "fwd-pbr";
intern const rid FWD_PBR_RBP_ID = make_rid(FWD_PBR_RBP);

intern constexpr const char *MAIN_PASS_DEPTH_NAME = "main-pass-depth";
intern const rid MAIN_PASS_DEPTH_ID = make_rid(MAIN_PASS_DEPTH_NAME);

intern constexpr const char *MAIN_PASS_NAME = "main-pass";
intern const rid MAIN_PASS_ID = make_rid(MAIN_PASS_NAME);

intern constexpr const char *IMGUI_PASS_NAME = "imgui-pass";
intern const rid IMGUI_PASS_ID = make_rid(IMGUI_PASS_NAME);

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

constexpr u32 MAX_INSTANCES = 500;

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
    .arena_sizes{.free_list = 400 * MB_SIZE, .stack = 10 * MB_SIZE, .scratch_flinear = 5 * MB_SIZE},
    .desc{PL_LAYOUT_CFG},
    .mcounts{MANIFEST_MAX_COUNTS},
    .texture_pool_count = ARR_SIZE(TPOOL_CFGS),
    .texture_pool_cfgs = TPOOL_CFGS,
};

struct rdev_app_ctxt
{
    renderer rndr;
    render_thread rt;
    sim_region rgn;
    asset_cache cg;
    f64 accumulater;

    material_handle default_mat;

    rid daniel_mat;
    rid maria_mat;

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
    auto cam_comp = add_camera(cam);
    auto cam_tcomp = add_transform(cam);
    auto tf_sys = &get_transform_tbl(cam->cdb)->sys;

    set_camera_fov(cam_comp, 90.0f, nslib::CAMERA_FOV_TYPE_HORIZONTAL);
    set_camera_near_far(cam_comp, {0.1f, 1000.0f});
    set_camera_aspect(cam_comp, 1000.0f / 800.0f);
    // set_camera_proj_type(cam_comp, CAMERA_PROJ_TYPE_ORTHO);

    auto cam_view = math::inverse(math::look_at(vec3{0.0f, 10.0f, -5.0f}, vec3{0.0f}, vec3{0.0f, 1.0f, 0.0f}));
    update_transform_orientation(cam_tcomp, tf_sys, math::orientation(cam_view));
    update_transform_scale(cam_tcomp, tf_sys, math::scaling_vec(cam_view));
    update_transform_pos(cam_tcomp, tf_sys, math::translation_vec(cam_view));
    app->cam_id = cam->id;

    // Add our input trigger functions
    auto cam_turn_func = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        auto cam_ent = get_entity(app->cam_id, &app->rgn);
        auto tf_sys = &get_transform_tbl(cam_ent->cdb)->sys;
        auto camc = get_camera(cam_ent);
        auto camt = get_transform(cam_ent);

        auto delta = t.ev->mmotion.norm_delta;

        vec3 right = math::right_vec(camt->current.orientation);
        f32 factor = 4;
        vec4 horizontal = {right, -(f32)delta.y * factor};
        vec4 vertical = {{0, 0, 1}, (f32)delta.x * factor};
        update_transform_orientation(camt, tf_sys, math::orientation(vertical) * math::orientation(horizontal) * camt->current.orientation);
    };

    auto move_forward_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        auto cam = get_camera(app->cam_id, &app->rgn.cdb);
        f32 amnt = (t.ev->key.action - 1) * -2 + 1;
        (cam->ptype != CAMERA_PROJ_TYPE_ORTHO) ? app->movement.y += amnt : app->movement.y -= amnt;
    };
    auto move_back_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        auto cam = get_camera(app->cam_id, &app->rgn.cdb);
        f32 amnt = (t.ev->key.action - 1) * -2 + 1;
        (cam->ptype != CAMERA_PROJ_TYPE_ORTHO) ? app->movement.y -= amnt : app->movement.y += amnt;
    };
    auto move_right_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        app->movement.x += (t.ev->key.action - 1) * (-2) + 1;
    };
    auto move_left_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        app->movement.x -= (t.ev->key.action - 1) * (-2) + 1;
    };

    auto ortho_zoom_action = [](const input_trigger &t, void *data) {
        auto app = (rdev_app_ctxt *)data;
        auto cam = get_camera(app->cam_id, &app->rgn.cdb);
        if (cam->ptype != CAMERA_PROJ_TYPE_ORTHO) return;
        set_camera_fov(cam, cam->fov * (1.0f - t.ev->mwheel.delta.y * 0.1f));
    };

    set_input_trigger(&app->stack, "cam-turn", {cam_turn_func, app});
    set_input_trigger(&app->stack, "move-forward", {move_forward_action, app});
    set_input_trigger(&app->stack, "move-back", {move_back_action, app});
    set_input_trigger(&app->stack, "move-right", {move_right_action, app});
    set_input_trigger(&app->stack, "move-left", {move_left_action, app});
    set_input_trigger(&app->stack, "ortho-zoom", {ortho_zoom_action, app});

    set_keymap_entry(&app->global_km, KMCODE_MMOTION, 0, MBUTTON_MASK_MIDDLE, {make_rid("cam-turn")});
    set_keymap_entry(&app->global_km, KMCODE_MWHEEL, 0, 0, {make_rid("ortho-zoom")});
    set_keymap_entry(&app->movement_km, KMCODE_KEY_W, 0, 0, {make_rid("move-forward"), INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});
    set_keymap_entry(&app->movement_km, KMCODE_KEY_S, 0, 0, {make_rid("move-back"), INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});
    set_keymap_entry(&app->movement_km, KMCODE_KEY_D, 0, 0, {make_rid("move-right"), INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});
    set_keymap_entry(&app->movement_km, KMCODE_KEY_A, 0, 0, {make_rid("move-left"), INPUT_ACTION_PRESS | INPUT_ACTION_RELEASE});

    // Make our movement keymap not care about any modifiers at all - we always move no matter what
    app->movement_km.kmod_mask = KEYMOD_NONE;
    app->movement_km.mbutton_mask = MBUTTON_MASK_NONE;
}

intern void create_entity_grid(sim_region *region, const geometry &cube_geom, const geometry &rect_geom, rdev_app_ctxt *app)
{
    // Create a grid of entities with odd ones being cubes and even being rectangles
    int len = 5, width = 5, height = 5;
    auto ent_offset = add_entities(len * width * height, region);

    auto tf_tbl = get_transform_tbl(&region->cdb);
    for (sizet zind = 0; zind < height; ++zind) {
        for (sizet yind = 0; yind < len; ++yind) {
            for (sizet xind = 0; xind < width; ++xind) {
                sizet ent_ind = zind * (width * len) + yind * width + xind + ent_offset;
                auto ent = &region->ents[ent_ind];
                auto tfcomp = add_transform(ent->id, tf_tbl);
                auto sc = add_static_mesh(ent);
                if (xind % 2) {
                    sc->geom_id = cube_geom.id;
                    arr_emplace_back(&sc->mat_mapping, app->daniel_mat, 0);
                    ent->name = to_str("cube-%d", ent_ind);
                }
                else {
                    sc->geom_id = rect_geom.id;
                    arr_emplace_back(&sc->mat_mapping, app->maria_mat, 0);
                    ent->name = to_str("rect-%d", ent_ind);
                }
                update_transform_pos(tfcomp, &tf_tbl->sys, vec3{xind * 2.0f, yind * 2.0f, zind * 2.0f});
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
    auto cube_geom = create_asset(geom_pool, "cube");
    auto rect_geom = create_asset(geom_pool, "rect");
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
        wlog("Couldn't load texture: %s", ls(maria_face.item->name), err);
    }
    err = load_texture(daniel_face.item, "import/daniel.png");
    if (err) {
        wlog("Couldn't load texture %s: %s", ls(daniel_face.item->name), err);
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

intern void create_materials(material_pool *mat_pool, technique *tech, texture_pool *tex_pool, rdev_app_ctxt *app)
{
    material_item_ref default_mat = create_asset(mat_pool, "default");
    default_mat.item->col = {1, 0, 0, 1};
    default_mat.item->flags |= make_flag(MATERIAL_USE_COLOR_BIT);
    // Disable all culling
    default_mat.item->overrides.rmask = 0;
    default_mat.item->override_mask |= RASTER_OVERRIDE_STATE_CULLING;
    arr_emplace_back(&default_mat.item->bp_techniques, FWD_PBR_RBP_ID, tech->id);
    app->default_mat = default_mat.hndl;

    material_item_ref daniel_mat = create_asset(mat_pool, "daniel");
    // Disable all culling
    daniel_mat.item->overrides.rmask = 0;
    daniel_mat.item->override_mask |= RASTER_OVERRIDE_STATE_CULLING;
    daniel_mat.item->textures[MAT_SAMPLER_SLOT_ALBEDO].id = find_asset(tex_pool, "daniel-face").item->id;
    arr_emplace_back(&daniel_mat.item->bp_techniques, FWD_PBR_RBP_ID, tech->id);
    app->daniel_mat = daniel_mat.item->id;

    material_item_ref maria_mat = create_asset(mat_pool, "maria");
    // Disable all culling
    maria_mat.item->overrides.rmask = 0;
    maria_mat.item->override_mask |= RASTER_OVERRIDE_STATE_CULLING;
    maria_mat.item->textures[MAT_SAMPLER_SLOT_ALBEDO].id = find_asset(tex_pool, "maria-face").item->id;
    arr_emplace_back(&maria_mat.item->bp_techniques, FWD_PBR_RBP_ID, tech->id);
    app->maria_mat = maria_mat.item->id;
}

intern void rdev_build_frame(rmanifest *m, const render_frame_payload *p, void *user);

intern b8 init_rdev(platform_ctxt *ctxt, rdev_app_ctxt *app)
{
    init_asset_cache_default_types(
        &app->cg,
        "asset-cache",
        {.free_list = current_thread_free_list(), .frame_linear = current_thread_scratch_flinear(), .stack = current_thread_stack()});

    auto geom_pool = get_asset_pool<geometry>(&app->cg);
    auto tex_pool = get_asset_pool<texture>(&app->cg);
    auto shdr_pool = get_asset_pool<shader>(&app->cg);
    auto tech_pool = get_asset_pool<technique>(&app->cg);
    auto mat_pool = get_asset_pool<material>(&app->cg);

    geometry *rect, *cube;
    create_geometry(geom_pool, &rect, &cube);
    create_textures(tex_pool);
    technique_item_ref diff_tech = create_diffuse_technique(shdr_pool, tech_pool);
    create_materials(mat_pool, diff_tech.item, tex_pool, app);

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
    upload_materials(&app->rndr, mat_pool, tex_pool, &ctxt->arenas.stack);

    // Create render targets
    // create_rtexture_target(&app->rndr, TEXTURE_TARGET_COLOR(MAIN_PASS_COLOR_NAME));
    create_rtexture_target(&app->rndr, TEXTURE_TARGET_DEPTH(MAIN_PASS_DEPTH_NAME));

    // Create our sim region aka scene
    init_sim_region(&app->rgn, current_thread_free_list());

    // Create input map
    init_keymap_stack(&app->stack, &ctxt->arenas.free_list);
    init_keymap(&app->movement_km, "movement", &ctxt->arenas.free_list);
    init_keymap(&app->global_km, "global", &ctxt->arenas.free_list);

    push_keymap(&app->stack, &app->movement_km);
    push_keymap(&app->stack, &app->global_km);

    // Create and setup input for camera
    setup_camera_controller(ctxt, app);
    create_entity_grid(&app->rgn, *cube, *rect, app);

    // Last thing in init: past here the render thread is live and the blueprint set is frozen.
    render_thread_cfg rt_cfg{};
    rt_cfg.rndr = &app->rndr;
    rt_cfg.frame_payload_arena_size = 20 * KB_SIZE;
    rt_cfg.mode = nslib::RENDER_THREAD_MODE_INLINE;
    rt_cfg.build = rdev_build_frame;
    rt_cfg.build_user = app;
    if (!init_render_thread(&app->rt, &ctxt->arenas.free_list, rt_cfg)) return false;

    return true;
}

// Loop through the scoobys
void update_transforms(sim_region *sr)
{
    transform_tbl *tf_tbl = get_transform_tbl(&sr->cdb);
    auto sys = &tf_tbl->sys;
    for (u32 i = 0; i < sys->active_set.size; ++i) {
        auto tf = get_transform(sys->active_set[i], tf_tbl);

        // Set this no matter what - even if not dirty we are in the active set which means last frame was dirty which
        // means on this frame, we remove from active set and let settle to all frames
        tf->rfif_dirty = MAX_FRAMES_IN_FLIGHT;

        // If the tform is dirty, mark it as processed and update cached, otherwise remove if from the active set
        if (is_comp_dirty(*tf)) {
            unset_flags(tf->flags, make_flag(COMP_DIRTY_BIT));
            tf->cached = build_mat4_from_trs(tf->current);
        }
        else {
            arr_swap_remove(&sys->active_set, tf->active_idx);
            if (tf->active_idx < sys->active_set.size) {
                // Get the swapped entity tform and update its active idx to the new position
                auto other_tf = get_transform(sys->active_set[tf->active_idx], tf_tbl);
                other_tf->active_idx = tf->active_idx;
            }
            tf->active_idx = INVALID_IDX;
        }
    }
}

intern void simulate(platform_ctxt *ctxt, rdev_app_ctxt *app, f64 dt)
{
    // Move the cam if needed
    auto cam = get_camera(app->cam_id, &app->rgn.cdb);
    auto tf_sys = &get_transform_tbl(&app->rgn.cdb)->sys;
    if (app->movement != svec2{}) {
        auto cam_tform = get_transform(app->cam_id, &app->rgn.cdb);
        auto right = math::right_vec(cam_tform->current.orientation);
        vec3 forward_or_up = cam->ptype == CAMERA_PROJ_TYPE_ORTHO ? math::up_vec(cam_tform->current.orientation)
                                                                  : math::target_vec(cam_tform->current.orientation);
        update_transform_pos(
            cam_tform, tf_sys, cam_tform->current.world_pos + (right * app->movement.x + forward_or_up * app->movement.y) * (f32)dt * 10);
    }
    static double update_tm = 0.0;
    static double render_tm = 0.0;

    auto tform_tbl = get_transform_tbl(&app->rgn.cdb);
    auto mat_cache = get_asset_pool<material>(&app->cg);
    auto geom_cache = get_asset_pool<geometry>(&app->cg);
    for (sizet i = 0; i < tform_tbl->entries.size; ++i) {
        auto curtf = &tform_tbl->entries[i];
        if (curtf->ent_id != app->cam_id) {
            vec4 axis_angle{(i % 3 == 0) * 1.0f, (i % 3 == 2) * 1.0f, (i % 3 == 1) * 1.0f, (f32)dt};
            update_transform_orientation(curtf, tf_sys, curtf->current.orientation * math::orientation(axis_angle));
        }
    }

    update_transforms(&app->rgn);
}

// This happens on the render thread
intern void build_manifest(rmanifest *m, const render_frame_payload *rfp, rdev_app_ctxt *app)
{
    material_pool *mat_pool = get_asset_pool<material>(&app->cg);
    auto default_mat = get_asset(mat_pool, app->default_mat);
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

    auto cam = get_camera(app->cam_id, &app->rgn.cdb);
    auto cam_tform = get_transform(app->cam_id, &app->rgn.cdb);

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
#if defined(USE_IMGUI)
    push_render_job(m, {.pass = imgui_id, .view = imgui_view_id, .cb = draw_imgui, .cb_user = rfp->imgui_data});
#endif

    prepare_and_draw_region(m, &app->rgn, &app->cg, default_mat);
}

// Runs on the render thread. Reads live sim state through app, which is only safe because
// LOCKSTEP has the sim blocked in submit_render_frame for the duration of this call.
intern void rdev_build_frame(rmanifest *m, const render_frame_payload *p, void *user)
{
    auto app = (rdev_app_ctxt *)user;
    build_manifest(m, p, app);
}

constexpr f64 FIXED_DT = 1 / 60.0f;
constexpr f64 MAX_CATCHUP_FRAMES = 4;
constexpr f64 MAX_CATCHUP_FRAME_DT = FIXED_DT * MAX_CATCHUP_FRAMES;

intern bool run_frame(platform_ctxt *ctxt, rdev_app_ctxt *app)
{
    begin_platform_frame(ctxt);

    // Get the minimum between actual dt since last frame, and a maximum frame catchup amount to prevent spinning of death
    f64 frame_dt = std::min(ctxt->time_pts.dt, MAX_CATCHUP_FRAME_DT);
    f64 elapsed = ptimer_elapsed_dt(&ctxt->time_pts);

    app->accumulater += frame_dt;
    // Input events are cleared in here rather than after the loop on purpose - if no fixed step runs this frame the
    // events stay queued for the next one instead of getting dropped. After the first step the queue is empty, so the
    // map calls in any catchup steps are no-ops.
    while (app->accumulater >= FIXED_DT) {
        map_input_frame(&app->stack, &ctxt->feventq);
        simulate(ctxt, app, FIXED_DT);
        app->accumulater -= FIXED_DT;
        clear_input_events(ctxt);
    }

// Gather visible items and do stuff
#ifdef USE_IMGUI
    ImGui::ShowDebugLogWindow();
#endif

    render_frame_payload *rp = get_write_slot(&app->rt);
    rp->alpha = app->accumulater / FIXED_DT;
    rp->fdata.frame_count = app->rndr.finished_frames;
    rp->fdata.dt = frame_dt;
    rp->fdata.elapsed = elapsed;
    rp->rbp = find_render_blueprint(&app->rndr, FWD_PBR_RBP_ID);
    bool res = submit_render_frame(&app->rt);

    // Window events live for the whole outer frame - the render thread reads them through the submit above
    clear_window_events(ctxt);
    end_platform_frame(ctxt);
    return res;
}

intern void terminate_rdev(platform_ctxt *ctxt, rdev_app_ctxt *app)
{
    // Join before tearing the renderer down - the render thread uses it until it exits.
    terminate_render_thread(&app->rt);
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
    pf_config.arena_sizes.free_list = 4 * 1024 * MB_SIZE;

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
