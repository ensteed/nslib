#include "input_mapping.h"
#include "platform.h"
#include "logging.h"

using namespace nslib;

struct app_data
{
    input_keymap km1{};
    input_keymap km2{};
    input_keymap km3{};
    input_keymap_stack stack{};
};

static void log_trigger(const input_trigger &ev, void *user)
{
    const char *map_name = (const char *)user;
    const char *entry_name = ev.name ? ev.name : "unknown";
    ilog("Input trigger: keymap=%s entry=%s ev_type=%u", map_name ? map_name : "unknown", entry_name, ev.ev_type);
}

static void add_keymap_entry_with_trigger(
    input_keymap_stack *stack,
    input_keymap *km,
    input_kmcode kmcode,
    u16 keymods,
    u8 mbutton_mask,
    const char *entry_name,
    const char *map_name)
{
    input_keymap_entry entry{};
    entry.name = entry_name;
    entry.action_mask = INPUT_ACTION_PRESS;
    entry.flags = 0;

    bool added = add_keymap_entry(km, kmcode, keymods, mbutton_mask, entry);
    ilog("Generating keymap entry %s for %s", entry_name, map_name);
    asrt(added);

    input_trigger_cb cb{};
    cb.func = log_trigger;
    cb.user = (void *)map_name;
    bool trig_added = add_input_trigger(stack, entry_name, cb);
    asrt(trig_added);
}

int app_init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    ilog("App init");
    init_keymap_stack(&app->stack, get_global_arena());
    init_keymap(&app->km1, "km1", get_global_arena());
    init_keymap(&app->km2, "km2", get_global_arena());
    init_keymap(&app->km3, "km3", get_global_arena());

    add_keymap_entry_with_trigger(&app->stack, &app->km1, KMCODE_KEY_K, KEYMOD_LCTRL, MBUTTON_MASK_NONE, "km1_ctrl_k", "km1");
    add_keymap_entry_with_trigger(&app->stack, &app->km1, KMCODE_KEY_M, KEYMOD_LSHIFT, MBUTTON_MASK_NONE, "km1_shift_m", "km1");
    add_keymap_entry_with_trigger(&app->stack, &app->km1, KMCODE_KEY_J, KEYMOD_LALT, MBUTTON_MASK_NONE, "km1_alt_j", "km1");
    add_keymap_entry_with_trigger(&app->stack, &app->km1, KMCODE_KEY_F1, KEYMOD_NONE, MBUTTON_MASK_NONE, "km1_f1", "km1");

    add_keymap_entry_with_trigger(&app->stack, &app->km2, KMCODE_KEY_K, KEYMOD_LCTRL, MBUTTON_MASK_NONE, "km2_ctrl_k", "km2");
    add_keymap_entry_with_trigger(&app->stack, &app->km2, KMCODE_KEY_M, KEYMOD_LSHIFT, MBUTTON_MASK_NONE, "km2_shift_m", "km2");
    add_keymap_entry_with_trigger(&app->stack, &app->km2, KMCODE_KEY_L, KEYMOD_LALT, MBUTTON_MASK_NONE, "km2_alt_l", "km2");
    add_keymap_entry_with_trigger(&app->stack, &app->km2, KMCODE_KEY_Q, (u16)(KEYMOD_LCTRL | KEYMOD_LSHIFT), MBUTTON_MASK_NONE, "km2_ctrl_shift_q", "km2");

    add_keymap_entry_with_trigger(&app->stack, &app->km3, KMCODE_KEY_K, KEYMOD_LCTRL, MBUTTON_MASK_NONE, "km3_ctrl_k", "km3");
    add_keymap_entry_with_trigger(&app->stack, &app->km3, KMCODE_KEY_M, KEYMOD_LSHIFT, MBUTTON_MASK_NONE, "km3_shift_m", "km3");
    add_keymap_entry_with_trigger(&app->stack, &app->km3, KMCODE_KEY_J, KEYMOD_LALT, MBUTTON_MASK_NONE, "km3_alt_j", "km3");
    add_keymap_entry_with_trigger(&app->stack, &app->km3, KMCODE_KEY_U, KEYMOD_LCTRL, MBUTTON_MASK_NONE, "km3_ctrl_u", "km3");

    push_keymap(&app->stack, &app->km1);
    push_keymap(&app->stack, &app->km2);
    push_keymap(&app->stack, &app->km3);

    ilog("Keymap stack order: km1 -> km2 -> km3 (top)");
    ilog("Overlap combos (top should win): CTRL+K, SHIFT+M, ALT+J");
    ilog("Unique combos: km1=F1, km2=CTRL+SHIFT+Q, km3=CTRL+U");
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    ilog("App terminate");
    terminate_keymap(&app->km1);
    terminate_keymap(&app->km2);
    terminate_keymap(&app->km3);
    terminate_keymap_stack(&app->stack);
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    // Use our context stack to map the platform input to callback functions
    map_input_frame(&app->stack, &ctxt->feventq);
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->flags = PLATFORM_INIT_FLAG_WINDOW;
    settings->wind.title = "Input Mapping";
    settings->wind.win_flags = WINDOW_RESIZABLE;
    settings->wind.resolution = {800,600};
    settings->user_hooks.init = app_init;
    settings->user_hooks.terminate = app_terminate;
    settings->user_hooks.run_frame = app_run_frame;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
