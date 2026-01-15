#pragma once
#include "memory.h"
#include "profile_timer.h"
#include "math/vector2.h"
#include "containers/array.h"
#include "util.h"
#include "logging.h"

namespace nslib
{

namespace err_code
{
enum platform
{
    PLATFORM_NO_ERROR,
    PLATFORM_INIT_FAIL,
    PLATFORM_RUN_FRAME_FAIL,
    PLATFORM_TERMINATE_FAIL
};

enum file
{
    FILE_NO_ERROR,
    FILE_OPEN_FAIL,
    FILE_TELL_FAIL,
    FILE_SEEK_FAIL,
    FILE_READ_DIFF_SIZE,
    FILE_WRITE_DIFF_SIZE,
    FILE_GET_CWD_FAIL
};

} // namespace err_code

enum platform_window_flags
{
    WINDOW_FULLSCREEN = 1u << 0,
    WINDOW_OPENGL = 1u << 1,
    WINDOW_SHOWN = 1u << 2,
    WINDOW_HIDDEN = 1u << 3,
    WINDOW_BORDERLESS = 1u << 4,
    WINDOW_RESIZABLE = 1u << 5,
    WINDOW_MINIMIZED = 1u << 6,
    WINDOW_MAXIMIZED = 1u << 7,
    WINDOW_MOUSE_GRABBED = 1u << 8,
    WINDOW_INPUT_FOCUS = 1u << 9,
    WINDOW_MOUSE_FOCUS = 1u << 10,
    WINDOW_FOREIGN = 1u << 11,
    WINDOW_FULLSCREEN_DESKTOP = (WINDOW_FULLSCREEN | (1u << 12)),
    // On macOS NSHighResolutionCapable must be set true in the application's Info.plist for this to have any effect.
    WINDOW_ALLOW_HIGHDPI = 1u << 13,
    // Has mouse captured (unrelated to MOUSE_GRABBED)
    WINDOW_MOUSE_CAPTURE = 1u << 14,
    WINDOW_ALWAYS_ON_TOP = 1u << 15,
    WINDOW_SKIP_TASKBAR = 1u << 16,
    WINDOW_UTILITY = 1u << 17,
    WINDOW_TOOLTIP = 1u << 18,
    WINDOW_POPUP_MENU = 1u << 19,
    WINDOW_KEYBOARD_GRABBED = 1u << 20,
    // Usable for Vulkan surface
    WINDOW_VULKAN = 1u << 28,
    // Usable for Metal view
    WINDOW_METAL = 1u << 29,
    WINDOW_TRANSPARENT = 1u << 30,
    WINDOW_NOT_FOCUSABLE = 1u << 31
};

enum platform_event_type
{
    EVENT_TYPE_INVALID = -1,
    EVENT_TYPE_INPUT_KEY,
    EVENT_TYPE_INPUT_MBUTTON,
    EVENT_TYPE_INPUT_MWHEEL,
    EVENT_TYPE_INPUT_MMOTION,
    EVENT_TYPE_WINDOW_RESIZE,
    EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGE,
    EVENT_TYPE_WINDOW_MOVE,
    EVENT_TYPE_WINDOW_FOCUS,
    EVENT_TYPE_WINDOW_MOUSE,
    EVENT_TYPE_WINDOW_FULLSCREEN,
    EVENT_TYPE_WINDOW_VIEWSTATE,
    EVENT_TYPE_WINDOW_VISIBILITY
};

bool is_input_event(u32 ev_type);
bool is_window_event(u32 ev_typ);

struct platform_key_event
{
    // Press, release, repeat
    u8 action;
    // Physical scancode
    u32 scancode;
    // Platform dependent scancode
    u16 raw_scancode;
    // Which keyboard
    u32 keyboard_id;
};

pup_func(platform_key_event)
{
    pup_member(action);
    pup_member(scancode);
    pup_member(raw_scancode);
    pup_member(keyboard_id);
}

struct platform_mbutton_event
{
    // Press, release
    u8 action;
    // Mouse position pixel coordinates
    vec2 mpos;
    // Mouse position normalized to screen size
    vec2 norm_mpos;
    // Which mouse
    u32 mouse_id;
};

pup_func(platform_mbutton_event)
{
    pup_member(action);
    pup_member(mpos);
    pup_member(norm_mpos);
    pup_member(mouse_id);
}

struct platform_mmotion_event
{
    // Mouse position pixel coordinates
    vec2 mpos;
    // Mouse position normalized to screen size
    vec2 norm_mpos;
    // Motion, in pixels, since last frame
    vec2 delta;
    // Motion since last frame normalzied to screen size
    vec2 norm_delta;
    // Which mouse
    u32 mouse_id;
};

pup_func(platform_mmotion_event)
{
    pup_member(mpos);
    pup_member(norm_mpos);
    pup_member(delta);
    pup_member(norm_delta);
    pup_member(mouse_id);
}

struct platform_mwheel_event
{
    // Mouse position pixel coordinates
    vec2 mpos;
    // Mouse position normalized to screen size
    vec2 norm_mpos;
    // Amount scrolled - left neg x, right pos x, y positive away from user (ie up/increase) and y neg towards the user (down/decrease)
    vec2 delta;
    // Amount scrolled - same as above but accumulated in to ticks
    svec2 idelta;
    // Which mouse
    u32 mouse_id;
};

pup_func(platform_mwheel_event)
{
    pup_member(mpos);
    pup_member(norm_mpos);
    pup_member(delta);
    pup_member(idelta);
    pup_member(mouse_id);
}

struct platform_input_event
{
    u16 kmcode;
    u16 keymods;
    u8 mbutton_mask;
    union
    {
        platform_key_event key;
        platform_mbutton_event mbutton;
        platform_mwheel_event mwheel;
        platform_mmotion_event mmotion;
    };
};

pup_func(platform_input_event)
{
    u32 type = *((u32 *)vinfo.meta.data);
    pup_member(kmcode);
    pup_member(keymods);
    pup_member(mbutton_mask);
    if (type == EVENT_TYPE_INPUT_KEY) {
        pup_member(key);
    }
    else if (type == EVENT_TYPE_INPUT_MBUTTON) {
        pup_member(mbutton);
    }
    else if (type == EVENT_TYPE_INPUT_MWHEEL) {
        pup_member(mwheel);
    }
    else if (type == EVENT_TYPE_INPUT_MMOTION) {
        pup_member(mmotion);
    }
}

union platform_window_event
{
    // First is new size, second is prev size - for window resize these are screen coords, for fb resize they are pixels
    pair<svec2, svec2> resize{};
    // First is new pos, second is prev pos
    pair<svec2, svec2> move;
    // Generic name
    pair<svec2, svec2> data;

    // Gained focus is 1, lost focus is 0
    int focus;
    // Mouse entered is 1, mouse left is 0
    int mouse;
    // Enter fullscreen is 1, leave fullscreen is 0
    int fullscreen;
    // Minimized is -1, restored is 0, maximized is 1
    int viewstate;
    // Shown is 1 and hidden is 0
    int visibility;
    // Generic name
    int idata;
};

pup_func(platform_window_event)
{
    u32 type = *((u32 *)vinfo.meta.data);
    if (type == EVENT_TYPE_WINDOW_RESIZE || type == EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGE) {
        pup_member(resize);
    }
    else if (type == EVENT_TYPE_WINDOW_MOVE) {
        pup_member(move);
    }
    else if (type == EVENT_TYPE_WINDOW_FOCUS) {
        pup_member(focus);
    }
    else if (type == EVENT_TYPE_WINDOW_MOUSE) {
        pup_member(mouse);
    }
    else if (type == EVENT_TYPE_WINDOW_FULLSCREEN) {
        pup_member(fullscreen);
    }
    else if (type == EVENT_TYPE_WINDOW_VIEWSTATE) {
        pup_member(viewstate);
    }
    else if (type == EVENT_TYPE_WINDOW_VISIBILITY) {
        pup_member(visibility);
    }
}

struct platform_event
{
    platform_event_type type{EVENT_TYPE_INVALID};
    u64 timestamp;
    u32 win_id;
    union
    {
        platform_input_event ie;
        platform_window_event we;
    };
};

pup_func(platform_event)
{
    pup_enum_member(platform_event_type, u32, type);
    pup_member(timestamp);
    pup_member(win_id);
    if (is_input_event(val.type)) {
        pup_member_meta(ie, .data = &val.type);
    }
    else if (is_window_event(val.type)) {
        pup_member_meta(we, .data = &val.type);
    }
}

// Return true if the event was handled and the platform input should ignore it
using platform_sdl_event_func = bool(void *sdl_event, void *user);
struct platform_sdl_event_hook
{
    platform_sdl_event_func *cb;
    void *user;
};

struct platform_frame_event_queue
{
    static_array<platform_event, 1024> events{};
    platform_sdl_event_hook sdl_hook{};
};

struct platform_memory
{
    mem_arena free_list{};
    mem_arena stack{};
    mem_arena frame_linear{};
    mem_arena sdl_fl{};
};

struct platform_ctxt
{
    u32 init_flags;
    void *win_hndl{};
    f32 display_scale{};

    profile_timepoints time_pts{};
    platform_frame_event_queue feventq{};

    platform_memory arenas{};
    int finished_frames{0};
    char **argv{};
    bool running{false};
    int argc;
};

using platform_user_hook = int(platform_ctxt *ctxt, void *user_data);

struct platform_file_err_desc
{
    int code;
    const char *str;
};

struct platform_window_init_info
{
    u32 win_flags{};
    svec2 resolution;
    const char *title;
};

struct platform_memory_init_info
{
    sizet free_list_size{4000 * MB_SIZE};
    sizet stack_size{100 * MB_SIZE};
    sizet frame_linear_size{100 * MB_SIZE};
};

struct platform_user_hooks
{
    platform_user_hook *init;
    platform_user_hook *run_frame;
    platform_user_hook *terminate;
};

enum platform_init_flag {
    PLATFORM_INIT_FLAG_AUDIO,
    PLATFORM_INIT_FLAG_WINDOW
};

struct platform_init_info
{
    int argc;
    char **argv;
    u32 flags;
    platform_user_hooks user_hooks;
    platform_window_init_info wind;
    platform_memory_init_info mem;
    int default_log_level{LOG_TRACE};
};

int init_platform(const platform_init_info *config, platform_ctxt *ctxt);
int terminate_platform(platform_ctxt *ctxt);

void *platform_alloc(sizet byte_size);
void *platform_realloc(void *ptr, sizet byte_size);
void platform_free(void *block);

void start_platform_frame(platform_ctxt *ctxt);
void end_platform_frame(platform_ctxt *ctxt);

void set_platform_sdl_event_hook(void *window, const platform_sdl_event_hook &hook);

void *create_window(const platform_window_init_info *pf_config, float *display_scale = nullptr);

// Get the window size in screen coords
svec2 get_window_size(void *window_hndl);

// Get the window size in pixels - could be different than screen coords for HighDPI displays
svec2 get_window_pixel_size(void *window_hndl);

// Get the window position in screen coords
svec2 get_window_pos(void *window_hndl);

// Get the window scale for the display it is on at the time of this call
f32 get_window_display_scale(void *window_hndl);

// Get a pointer to the window from the id
void *get_window(u32 id);

// Get mouse position in pixels relative to the currently focused window
vec2 get_mouse_pos();

// Get the OS specific thread id
u64 get_thread_id();

const char *event_type_to_string(platform_event_type type);

// // Get the cursor
// vec2 get_cursor_pos(void *window_hndl);
// vec2 get_normalized_cursor_pos(void *window_hndl);

bool frame_has_event_type(platform_event_type type, const platform_frame_event_queue *fevents);
void process_platform_events(platform_ctxt *pf);
bool window_resized_this_frame(void *win_hndl);

const char *get_path_basename(const char *path);

sizet get_file_size(const char *fname, platform_file_err_desc *err);
const char *get_username();

sizet read_file(const char *fname,
                const char *mode,
                void *data,
                sizet element_size,
                sizet nelements,
                sizet byte_offset = 0,
                platform_file_err_desc *err = nullptr);

sizet read_file(const char *fname, void *data, sizet element_size, sizet nelements, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

// If 0, vec will be resized to entire file size
sizet read_file(const char *fname, const char *mode, byte_array *buffer, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

sizet read_file(const char *fname, byte_array *buffer, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname,
                 const char *mode,
                 const void *data,
                 sizet element_size,
                 sizet nelements,
                 sizet byte_offset = 0,
                 platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname,
                 const void *data,
                 sizet element_size,
                 sizet nelements,
                 sizet byte_offset = 0,
                 platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname, const char *mode, const byte_array *data, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname, const byte_array *data, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

} // namespace nslib

// int config_platform_func(nslib::platform_cfg *config, client_app_data_type *user_data);
#define __MAIN_BLOCK__(config_platform_func)                                                                                               \
    using namespace nslib;                                                                                                                 \
    bool run_loop{true};                                                                                                                   \
    platform_init_info pf_config{argc, argv};                                                                                              \
    if (config_platform_func(&pf_config, &user_data) != err_code::PLATFORM_NO_ERROR) {                                                     \
        return err_code::PLATFORM_INIT_FAIL;                                                                                               \
    }                                                                                                                                      \
    ctxt.argc = pf_config.argc;                                                                                                            \
    ctxt.argv = pf_config.argv;                                                                                                            \
    if (init_platform(&pf_config, &ctxt) != err_code::PLATFORM_NO_ERROR) {                                                                 \
        return err_code::PLATFORM_INIT_FAIL;                                                                                               \
    }                                                                                                                                      \
    if (pf_config.user_hooks.init) {                                                                                                       \
        int err = pf_config.user_hooks.init(&ctxt, &user_data);                                                                            \
        if (err != err_code::PLATFORM_NO_ERROR) {                                                                                          \
            elog("User init failed with code %d", err);                                                                                    \
            return terminate_platform(&ctxt);                                                                                              \
        }                                                                                                                                  \
    }                                                                                                                                      \
    ptimer_restart(&ctxt.time_pts);                                                                                                        \
    while (run_loop && ctxt.running) {                                                                                                     \
        start_platform_frame(&ctxt);                                                                                                       \
        if (pf_config.user_hooks.run_frame && pf_config.user_hooks.run_frame(&ctxt, &user_data) != err_code::PLATFORM_NO_ERROR) {          \
            run_loop = false;                                                                                                              \
        }                                                                                                                                  \
        end_platform_frame(&ctxt);                                                                                                         \
    }                                                                                                                                      \
    if (pf_config.user_hooks.terminate) {                                                                                                  \
        int err = pf_config.user_hooks.terminate(&ctxt, &user_data);                                                                       \
        if (err != err_code::PLATFORM_NO_ERROR) {                                                                                          \
            elog("User terminate failed with code %d", err);                                                                               \
        }                                                                                                                                  \
    }                                                                                                                                      \
    return terminate_platform(&ctxt);

#define DEFINE_APPLICATION_MAIN_STATIC(user_type, config_platform_func)                                                                    \
    user_type user_data{};                                                                                                                 \
    nslib::platform_ctxt ctxt{};                                                                                                           \
    int main(int argc, char **argv)                                                                                                        \
    {                                                                                                                                      \
        __MAIN_BLOCK__(config_platform_func)                                                                                               \
    }

#define DEFINE_APPLICATION_MAIN(user_type, config_platform_func)                                                                           \
    int main(int argc, char **argv)                                                                                                        \
    {                                                                                                                                      \
        user_type user_data{};                                                                                                             \
        nslib::platform_ctxt ctxt{};                                                                                                       \
        __MAIN_BLOCK__(config_platform_func)                                                                                               \
    }
