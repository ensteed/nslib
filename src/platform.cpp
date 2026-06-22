#include "basic_types.h"
#include "profiling.h"
#include "imgui/imgui_impl_vulkan.h"
#include "osdef.h"
#ifdef PLATFORM_UNIX
    #include <unistd.h>
    #include <pthread.h>
    #define PATH_SEP '/'
#elif defined(PLATFORM_WIN32)
    #include <windows.h>
    #define PATH_SEP '\\'
#endif
#include <ctime>
#include <cerrno>

#include "json_archive.h"
#include "input_kmcodes.h"
#include "math/primitives.h"
#include "platform.h"
#include "logging.h"
#include "containers/cjson.h"
#include "imgui/imgui_impl_sdl3.h"

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_init.h"

namespace nslib
{

const char *get_username()
{
    const char* user = getenv("USER");
    if (!user) user = getenv("USERNAME");
    if (!user) user = "unknown_dev";
    return user;
}

intern mem_arena *g_sdl_arena{};

intern platform_ctxt *platform_window_ptr(void *win)
{
    auto props = SDL_GetWindowProperties((SDL_Window *)win);
    auto pf = SDL_GetPointerProperty(props, "platform", nullptr);
    return (platform_ctxt *)pf;
}

intern void glfw_error_callback(s32 error, const char *description)
{
    elog("Error %d: %s", error, description);
}

intern const char *sdl_cat_str(int cat)
{
    switch (cat) {
    case SDL_LOG_CATEGORY_APPLICATION:
        return "app";
    case SDL_LOG_CATEGORY_ERROR:
        return "error";
    case SDL_LOG_CATEGORY_ASSERT:
        return "assert";
    case SDL_LOG_CATEGORY_SYSTEM:
        return "system";
    case SDL_LOG_CATEGORY_AUDIO:
        return "audio";
    case SDL_LOG_CATEGORY_VIDEO:
        return "video";
    case SDL_LOG_CATEGORY_RENDER:
        return "render";
    case SDL_LOG_CATEGORY_INPUT:
        return "input";
    case SDL_LOG_CATEGORY_TEST:
        return "test";
    case SDL_LOG_CATEGORY_GPU:
        return "gpu";
    default:
        return "unknown";
    }
}

intern void sdl_log_callback(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    switch (priority) {
    case SDL_LOG_PRIORITY_TRACE:
        tlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_VERBOSE:
        tlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        dlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_INFO:
        ilog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_WARN:
        wlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_ERROR:
        elog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_CRITICAL:
        clog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    default:
        elog("SDL? %s: %s", sdl_cat_str(category), message);
    }
}

intern bool log_any_sdl_error(const char *prefix = "SDL err")
{
    auto err = SDL_GetError();
    elog("%s: %s", prefix, (err) ? err : "none");
    bool ret = (err);
    SDL_ClearError();
    return ret;
}

const char *get_path_basename(const char *path)
{
    const char *ret = strrchr(path, PATH_SEP);
    if (!ret) {
        ret = path;
    }
    return ret;
}

bool is_input_event(u32 ev_type)
{
    return ev_type < EVENT_TYPE_WINDOW_RESIZE;
}

bool is_window_event(u32 ev_typ)
{
    return !is_input_event(ev_typ);
}

bool frame_has_event_type(platform_event_type type, const platform_frame_event_queue *fevents)
{
    for (sizet i = 0; i < fevents->events.size; ++i) {
        if (fevents->events[i].type == type) {
            return true;
        }
    }
    return false;
}

intern void init_mem_arenas(const platform_memory_init_info *info, platform_memory *mem)
{
    // Null to indicate these get platform_alloc'd
    init_fl_arena(&mem->free_list, info->free_list_size, nullptr, "global");
    init_stack_arena(&mem->stack, info->stack_size, nullptr, "global");
    init_lin_arena(&mem->frame_linear, info->frame_linear_size, nullptr, "global");
    // 213 KB is about the min needed for SDL - we'll give it 500 to be safe
    init_fl_arena(&mem->sdl_fl, 500 * KB_SIZE, &mem->free_list, "sdl");

    // Then these become our global mem arenas
    set_global_arena(&mem->free_list);
    set_global_stack_arena(&mem->stack);
    set_global_frame_lin_arena(&mem->frame_linear);
    g_sdl_arena = &mem->sdl_fl;

    // Set up our json alloc and free funcs
    json_hooks hooks;
    auto mem_glob_alloc = [](sizet sz) -> void * { return mem_alloc(sz, get_global_arena()); };
    auto mem_glob_free = [](void *ptr) -> void { mem_free(ptr, get_global_arena()); };

    hooks.malloc_fn = mem_glob_alloc;
    hooks.free_fn = mem_glob_free;
    json_init_hooks(&hooks);
}

intern void terminate_mem_arenas(platform_memory *mem)
{
    terminate_arena(&mem->sdl_fl);
    terminate_arena(&mem->stack);
    terminate_arena(&mem->frame_linear);
    terminate_arena(&mem->free_list);
    g_sdl_arena = nullptr;
    set_global_arena(nullptr);
    set_global_stack_arena(nullptr);
    set_global_frame_lin_arena(nullptr);
}

intern void log_display_info()
{
    int count;
    auto ids = SDL_GetDisplays(&count);
    ilog("Got %d displays", count);
    for (int i = 0; i < count; ++i) {
        SDL_Rect r;
        auto props = SDL_GetDisplayBounds(ids[i], &r);
        srect sr = {r.x, r.y, r.w, r.h};
        ilog("Display %s - rect x:%d y:%d w:%d h:%d", SDL_GetDisplayName(ids[i]), r.x, r.y, r.w, r.h);
    }
}

f32 get_window_display_scale(void *window_hndl)
{
    auto disp_id = SDL_GetDisplayForWindow((SDL_Window *)window_hndl);
    return SDL_GetDisplayContentScale(disp_id);
}

intern u32 get_sdl_window_flags(u32 platform_win_flags)
{
    return platform_win_flags;
}

intern input_kmcode map_sdl_key(SDL_Keycode sdl_key)
{
    u16 ret{0};
    if (sdl_key <= SDLK_PLUSMINUS) {
        ret = sdl_key;
    }
    else if (sdl_key <= SDLK_RHYPER) {
        int offset = sdl_key - SDLK_LEFT_TAB;
        ret = KMCODE_KEY_LEFT_TAB + offset;
    }
    else if (sdl_key <= SDLK_PAGEUP) {
        int offset = sdl_key - SDLK_CAPSLOCK;
        ret = KMCODE_KEY_CAPSLOCK + offset;
    }
    else if (sdl_key <= SDLK_VOLUMEDOWN) {
        int offset = sdl_key - SDLK_END;
        ret = KMCODE_KEY_END + offset;
    }
    else if (sdl_key <= SDLK_KP_EQUALSAS400) {
        int offset = sdl_key - SDLK_KP_COMMA;
        ret = KMCODE_KEY_KP_COMMA + offset;
    }
    else if (sdl_key <= SDLK_EXSEL) {
        int offset = sdl_key - SDLK_ALTERASE;
        ret = KMCODE_KEY_ALTERASE + offset;
    }
    else if (sdl_key <= SDLK_KP_HEXADECIMAL) {
        int offset = sdl_key - SDLK_KP_00;
        ret = KMCODE_KEY_KP_00 + offset;
    }
    else if (sdl_key <= SDLK_RGUI) {
        int offset = sdl_key - SDLK_LCTRL;
        ret = KMCODE_KEY_LCTRL + offset;
    }
    else if (sdl_key <= SDLK_ENDCALL) {
        int offset = sdl_key - SDLK_MODE;
        ret = KMCODE_KEY_MODE + offset;
    }
    else {
        wlog("Unhandled SDL key code %u", sdl_key);
    }
    return (input_kmcode)ret;
}

intern input_kmcode map_sdl_mbutton(u8 sdl_mbutton)
{
    u16 ret{sdl_mbutton};
    return (input_kmcode)ret;
}

intern u16 map_sdl_mods(SDL_Keymod mods)
{
    // Nothing needs to be done to map these
    return mods;
}

intern u16 map_sdl_mouse_state(SDL_MouseButtonFlags mods)
{
    // Nothing needs to be done to map these
    return (u16)mods;
}

intern void handle_sdl_key_event(platform_ctxt *ctxt, platform_event *event, const SDL_KeyboardEvent &ev)
{
    event->type = EVENT_TYPE_INPUT_KEY;
    event->timestamp = ev.timestamp;
    event->win_id = ev.windowID;
    event->ie.kmcode = map_sdl_key(ev.key);
    event->ie.keymods = map_sdl_mods(ev.mod);
    event->ie.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));

    event->ie.key.action = (ev.repeat) ? INPUT_ACTION_REPEAT : ((ev.down) ? INPUT_ACTION_PRESS : INPUT_ACTION_RELEASE);
    event->ie.key.scancode = ev.scancode;
    event->ie.key.raw_scancode = ev.raw;
    event->ie.key.keyboard_id = ev.which;
}

intern void handle_sdl_mbutton_event(platform_ctxt *ctxt, platform_event *event, const SDL_MouseButtonEvent &ev)
{
    event->type = EVENT_TYPE_INPUT_MBUTTON;
    event->timestamp = ev.timestamp;
    event->win_id = ev.windowID;
    event->ie.kmcode = map_sdl_mbutton(ev.button);
    event->ie.keymods = map_sdl_mods(SDL_GetModState());
    event->ie.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));

    vec2 win_sz = get_window_pixel_size(get_window(ev.windowID));
    event->ie.mbutton.action = (ev.down) ? INPUT_ACTION_PRESS : INPUT_ACTION_RELEASE;
    event->ie.mbutton.mpos = {ev.x, ev.y};
    event->ie.mbutton.norm_mpos = event->ie.mbutton.mpos / win_sz;
    event->ie.mbutton.mouse_id = ev.which;
}

intern void handle_sdl_mmotion_event(platform_ctxt *ctxt, platform_event *event, const SDL_MouseMotionEvent &ev)
{
    event->type = EVENT_TYPE_INPUT_MMOTION;
    event->timestamp = ev.timestamp;
    event->win_id = ev.windowID;
    event->ie.kmcode = KMCODE_MMOTION;
    event->ie.keymods = map_sdl_mods(SDL_GetModState());
    event->ie.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));

    vec2 win_sz = get_window_pixel_size(get_window(ev.windowID));
    event->ie.mmotion.mpos = {ev.x, ev.y};
    event->ie.mmotion.norm_mpos = event->ie.mbutton.mpos / win_sz;
    event->ie.mmotion.delta = {ev.xrel, ev.yrel};
    event->ie.mmotion.norm_delta = event->ie.mmotion.delta / win_sz;
    event->ie.mmotion.mouse_id = ev.which;
}

intern void handle_sdl_mwheel_event(platform_ctxt *ctxt, platform_event *event, const SDL_MouseWheelEvent &ev)
{
    event->type = EVENT_TYPE_INPUT_MWHEEL;
    event->timestamp = ev.timestamp;
    event->win_id = ev.windowID;
    event->ie.kmcode = KMCODE_MWHEEL;
    event->ie.keymods = map_sdl_mods(SDL_GetModState());
    event->ie.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));

    vec2 win_sz = get_window_pixel_size(get_window(ev.windowID));
    event->ie.mwheel.mpos = {ev.x, ev.y};
    event->ie.mwheel.norm_mpos = event->ie.mbutton.mpos / win_sz;
    event->ie.mwheel.delta = {ev.x, ev.y};
    event->ie.mwheel.idelta = {ev.integer_x, ev.integer_y};
    event->ie.mwheel.mouse_id = ev.which;
}

intern void handle_sdl_window_geom_with_prev(platform_ctxt *ctxt,
                                             platform_event *event,
                                             const svec2 &prev,
                                             platform_event_type et,
                                             const SDL_WindowEvent &ev)
{
    event->type = et;
    event->timestamp = ev.timestamp;
    event->win_id = ev.windowID;
    event->we.data = {prev, {ev.data1, ev.data2}};
}

intern void handle_sdl_window_event(platform_ctxt *ctxt, platform_event *event, int data, platform_event_type et, const SDL_WindowEvent &ev)
{
    event->type = et;
    event->timestamp = ev.timestamp;
    event->win_id = ev.windowID;
    event->we.idata = data;
}

intern void *sdl_malloc(sizet size)
{
    return mem_alloc(size, g_sdl_arena, SIMD_MIN_ALIGNMENT);
}

intern void *sdl_calloc(sizet nmemb, sizet memb)
{
    return mem_calloc(nmemb, memb, g_sdl_arena, SIMD_MIN_ALIGNMENT);
}

intern void *sdl_realloc(void *ptr, sizet size)
{
    return mem_realloc(ptr, size, g_sdl_arena, SIMD_MIN_ALIGNMENT);
}

intern void sdl_free(void *ptr)
{
    mem_free(ptr, g_sdl_arena);
}

void *platform_alloc(sizet byte_size)
{
    return malloc(byte_size);
}

void platform_free(void *block)
{
    free(block);
}

void *platform_realloc(void *ptr, sizet byte_size)
{
    return realloc(ptr, byte_size);
}

#if defined(PROFILING_ENABLED)
profiling_context *GLOBAL_PROFILING_CONTEXT[PROFILE_CONTEXT_COUNT]{};
#endif

int init_platform(const platform_init_info *settings, platform_ctxt *ctxt)
{
    ctxt->argc = settings->argc;
    ctxt->argv = settings->argv;
    set_logging_level(GLOBAL_LOGGER, settings->default_log_level);
    init_mem_arenas(&settings->mem, &ctxt->arenas);

#if defined(PROFILING_ENABLED)
    // Setup profiling
    for (int i = 0; i < PROFILE_CONTEXT_COUNT; ++i) {
        profiling_init(&ctxt->profiling_contexts[i], 128, 64, &ctxt->arenas.free_list);
        profiling_set_avg_window(&ctxt->profiling_contexts[i], 1000, &ctxt->arenas.free_list);
        GLOBAL_PROFILING_CONTEXT[i] = &ctxt->profiling_contexts[i];
    }
#endif

    ctxt->init_flags = settings->flags;
    bool init_audio = test_flags(settings->flags, PLATFORM_INIT_FLAG_AUDIO);
    bool init_window = test_flags(settings->flags, PLATFORM_INIT_FLAG_WINDOW);
    ilog("Platform init version %d.%d.%d", NSLIB_VERSION_MAJOR, NSLIB_VERSION_MINOR, NSLIB_VERSION_PATCH);

    if (init_window || init_audio) {
        SDL_SetMemoryFunctions(sdl_malloc, sdl_calloc, sdl_realloc, sdl_free);
        SDL_SetLogOutputFunction(sdl_log_callback, nullptr);
        SDL_SetAppMetadata("Rdev", "1.0.0", "rdev");
        int sdl_init_flag = (init_audio ? SDL_INIT_AUDIO : 0) | (init_window ? SDL_INIT_VIDEO : 0);
        if (!SDL_Init(sdl_init_flag)) {
            elog("Could not initialize SDL");
            return err_code::PLATFORM_INIT_FAIL;
        }
        ilog("Initialized SDL");

        if (init_window) {
            ctxt->win_hndl = create_window(&settings->wind, &ctxt->display_scale);
            if (!ctxt->win_hndl) {
                log_any_sdl_error("Failed to create window");
                return err_code::PLATFORM_INIT_FAIL;
            }
            auto props = SDL_GetWindowProperties((SDL_Window *)ctxt->win_hndl);
            if (props == 0) {
                log_any_sdl_error("Failed to get window props");
            }
            else {
                if (!SDL_SetPointerProperty(props, "platform", ctxt)) {
                    log_any_sdl_error("Failed to set platform ptr in window props");
                }
            }
            SDL_SetWindowPosition((SDL_Window *)ctxt->win_hndl, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }

    // Seed random number generator
    srand((u32)time(NULL));

    ptimer_restart(&ctxt->time_pts);
    ctxt->running = true;
    return err_code::PLATFORM_NO_ERROR;
}

int terminate_platform(platform_ctxt *ctxt)
{
    ilog("Platform terminate");
    bool init_audio = test_flags(ctxt->init_flags, PLATFORM_INIT_FLAG_AUDIO);
    bool init_window = test_flags(ctxt->init_flags, PLATFORM_INIT_FLAG_WINDOW);
    if (init_window || init_audio) {
        if (ctxt->win_hndl) {
            SDL_DestroyWindow((SDL_Window *)ctxt->win_hndl);
        }
        int sdl_init_flag = (init_audio ? SDL_INIT_AUDIO : 0) | (init_window ? SDL_INIT_VIDEO : 0);
        SDL_QuitSubSystem(sdl_init_flag);
        SDL_Quit();
        ilog("Terminated SDL");
    }
#if defined(PROFILING_ENABLED)
    for (int i = 0; i < PROFILE_CONTEXT_COUNT; ++i) {
        profiling_terminate(&ctxt->profiling_contexts[i]);
    }
#endif
    terminate_mem_arenas(&ctxt->arenas);
    return err_code::PLATFORM_NO_ERROR;
}

void set_platform_sdl_event_hook(void *window, const platform_sdl_event_hook &hook)
{
    platform_ctxt *pf = platform_window_ptr(window);
    pf->feventq.sdl_hook = hook;
}

void *create_window(const platform_window_init_info *settings, float *display_scale)
{
    asrt(settings);

    log_display_info();

    svec2 sz = settings->resolution;
    auto sdl_flags = get_sdl_window_flags(settings->win_flags);

    auto primary = SDL_GetPrimaryDisplay();
    f32 scale = SDL_GetDisplayContentScale(primary);
    if (display_scale) {
        *display_scale = scale;
    }
    if (test_flags(settings->win_flags, WINDOW_ALLOW_HIGHDPI)) {
        sz = sz * scale;
        ilog("Display scaling set to %.2f - adjusted resolution from %s to %s", scale, js(settings->resolution), js(sz));
    }
    return SDL_CreateWindow(settings->title, sz.w, sz.h, sdl_flags);
}

const char *event_type_to_string(platform_event_type type)
{
    switch (type) {
    case EVENT_TYPE_INPUT_KEY:
        return "key";
    case EVENT_TYPE_INPUT_MBUTTON:
        return "mbutton";
    case EVENT_TYPE_INPUT_MWHEEL:
        return "mwheel";
    case EVENT_TYPE_INPUT_MMOTION:
        return "mmotion";
    case EVENT_TYPE_WINDOW_RESIZE:
        return "win_resize";
    case EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGE:
        return "win_pixel_size_change";
    case EVENT_TYPE_WINDOW_MOVE:
        return "win_move";
    case EVENT_TYPE_WINDOW_FOCUS:
        return "win_focus";
    case EVENT_TYPE_WINDOW_MOUSE:
        return "win_mouse";
    case EVENT_TYPE_WINDOW_FULLSCREEN:
        return "win_fullscreen";
    case EVENT_TYPE_WINDOW_VIEWSTATE:
        return "win_viewstate";
    case EVENT_TYPE_WINDOW_VISIBILITY:
        return "win_visibility";
    default:
        return "invalid";
    }
}

void process_platform_events(platform_ctxt *pf)
{
    // Get prev sz and pos for any window resize/move events
    svec2 prev_win_sz_screen_coords = get_window_size(pf->win_hndl);
    svec2 prev_win_sz_pixels = get_window_pixel_size(pf->win_hndl);
    svec2 prev_win_pos = get_window_pos(pf->win_hndl);

    arr_clear(&pf->feventq.events);
    pf->feventq.window_pixel_change = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (pf->feventq.sdl_hook.cb && pf->feventq.sdl_hook.cb(&event, pf->feventq.sdl_hook.user)) {
            continue;
        }
        platform_event ev{};
        switch (event.type) {
        case SDL_EVENT_QUIT:
            pf->running = false;
            break;
        case SDL_EVENT_KEY_DOWN:
            handle_sdl_key_event(pf, &ev, event.key);
            break;
        case SDL_EVENT_KEY_UP:
            handle_sdl_key_event(pf, &ev, event.key);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            handle_sdl_mbutton_event(pf, &ev, event.button);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            handle_sdl_mbutton_event(pf, &ev, event.button);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            handle_sdl_mmotion_event(pf, &ev, event.motion);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            handle_sdl_mwheel_event(pf, &ev, event.wheel);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            pf->feventq.window_pixel_change = true;
            handle_sdl_window_geom_with_prev(pf, &ev, prev_win_sz_screen_coords, EVENT_TYPE_WINDOW_RESIZE, event.window);
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            pf->feventq.window_pixel_change = true;            
            handle_sdl_window_geom_with_prev(pf, &ev, prev_win_sz_pixels, EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGE, event.window);
            break;
        case SDL_EVENT_WINDOW_MOVED:
            handle_sdl_window_geom_with_prev(pf, &ev, prev_win_pos, EVENT_TYPE_WINDOW_MOVE, event.window);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            handle_sdl_window_event(pf, &ev, 1, EVENT_TYPE_WINDOW_FOCUS, event.window);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            handle_sdl_window_event(pf, &ev, 0, EVENT_TYPE_WINDOW_FOCUS, event.window);
            break;
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            handle_sdl_window_event(pf, &ev, 1, EVENT_TYPE_WINDOW_MOUSE, event.window);
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            handle_sdl_window_event(pf, &ev, 0, EVENT_TYPE_WINDOW_MOUSE, event.window);
            break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            pf->feventq.window_pixel_change = true;
            handle_sdl_window_event(pf, &ev, 1, EVENT_TYPE_WINDOW_FULLSCREEN, event.window);
            break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            pf->feventq.window_pixel_change = true;
            handle_sdl_window_event(pf, &ev, 0, EVENT_TYPE_WINDOW_FULLSCREEN, event.window);
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            pf->feventq.window_pixel_change = true;            
            handle_sdl_window_event(pf, &ev, -1, EVENT_TYPE_WINDOW_VIEWSTATE, event.window);
            break;
        case SDL_EVENT_WINDOW_MAXIMIZED:
            pf->feventq.window_pixel_change = true;            
            handle_sdl_window_event(pf, &ev, 1, EVENT_TYPE_WINDOW_VIEWSTATE, event.window);
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            pf->feventq.window_pixel_change = true;            
            handle_sdl_window_event(pf, &ev, 0, EVENT_TYPE_WINDOW_VIEWSTATE, event.window);
            break;
        case SDL_EVENT_WINDOW_SHOWN:
            handle_sdl_window_event(pf, &ev, 1, EVENT_TYPE_WINDOW_VISIBILITY, event.window);
            break;
        case SDL_EVENT_WINDOW_HIDDEN:
            handle_sdl_window_event(pf, &ev, 0, EVENT_TYPE_WINDOW_VISIBILITY, event.window);
            break;
        default:
            // do nothing yet
            break;
        }

        if (ev.type != EVENT_TYPE_INVALID) {
            // tlog("Got %s event: %s", event_type_to_string(ev.type), js(ev));
            asrt(pf->feventq.events.size < pf->feventq.events.capacity);
            arr_push_back(&pf->feventq.events, ev);
        }
    }
}

void *get_window(u32 id)
{
    return SDL_GetWindowFromID(id);
}

svec2 get_window_size(void *win)
{
    svec2 ret;
    if (!SDL_GetWindowSize((SDL_Window *)win, &ret.w, &ret.h)) {
        log_any_sdl_error();
    }
    return ret;
}

svec2 get_window_pixel_size(void *win)
{
    svec2 ret;
    if (!SDL_GetWindowSizeInPixels((SDL_Window *)win, &ret.w, &ret.h)) {
        log_any_sdl_error();
    }
    return ret;
}

svec2 get_window_pos(void *win)
{
    svec2 ret;
    if (!SDL_GetWindowPosition((SDL_Window *)win, &ret.w, &ret.h)) {
        log_any_sdl_error();
    }
    return ret;
}

u64 get_thread_id()
{
#if defined(PLATFORM_UNIX)
    return (u64)pthread_self();
#elif defined(PLATFORM_WIN32)
    return (u64)GetCurrentThreadId();
#endif
}

vec2 get_mouse_pos()
{
    vec2 ret;
    SDL_GetMouseState(&ret.x, &ret.y);
    return ret;
}

bool window_resized_this_frame(void *win_hndl)
{
    platform_ctxt *pf = platform_window_ptr(win_hndl);
    return pf->feventq.window_pixel_change;
}

void begin_platform_frame(platform_ctxt *ctxt)
{
    ptimer_split(&ctxt->time_pts);
    if (ctxt->win_hndl) {
        process_platform_events(ctxt);
    }
    reset_arena(&ctxt->arenas.frame_linear);
}

void end_platform_frame(platform_ctxt *ctxt)
{
    ++ctxt->finished_frames;
}

intern sizet platform_file_size(FILE *f, platform_file_err_desc *err)
{
    sizet ret{0};

    sizet cur_p = ftell(f);
    if (cur_p == -1L)
        goto ftell_fail;

    if (fseek(f, 0, SEEK_END) != 0)
        goto fseek_fail;

    ret = ftell(f);
    if (ret == -1L)
        goto ftell_fail;

    if (fseek(f, (long)cur_p, SEEK_SET) != 0)
        goto fseek_fail;

    return ret;

fseek_fail:
    if (err) {
        err->code = err_code::FILE_SEEK_FAIL;
        err->str = strerror(errno);
    }
    return ret;

ftell_fail:
    if (err) {
        err->code = err_code::FILE_TELL_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet get_file_size(const char *fname, platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, "r");
    if (f) {
        ret = platform_file_size(f, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

intern sizet platform_read_file(FILE *f, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet nelems{0};
    if (byte_offset != 0 && fseek(f, (long)byte_offset, SEEK_SET) != 0) {
        if (err) {
            err->code = err_code::FILE_SEEK_FAIL;
            err->str = strerror(errno);
        }
    }
    else {
        nelems = fread(data, element_size, nelements, f);
    }
    return nelems;
}

sizet read_file(const char *fname, const char *mode, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet elems{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        elems = platform_read_file(f, data, element_size, nelements, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return elems;
}

sizet read_file(const char *fname, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    return read_file(fname, "rb", data, element_size, nelements, byte_offset, err);
}

sizet read_file(const char *fname, const char *mode, byte_array *buffer, sizet byte_offset, platform_file_err_desc *err)
{
    sizet elems{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        sizet fsize = platform_file_size(f, err);
        if (fsize > 0) {
            arr_resize(buffer, fsize);
            elems = platform_read_file(f, buffer->data, 1, buffer->size, byte_offset, err);
        }
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return elems;
}

sizet read_file(const char *fname, byte_array *buffer, sizet byte_offset, platform_file_err_desc *err)
{
    return read_file(fname, "rb", buffer, byte_offset, err);
}

intern sizet platform_write_file(FILE *f, const void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet ret{0};
    int seek = SEEK_SET;
    if (byte_offset == -1) {
        byte_offset = 0;
        seek = SEEK_END;
    }

    if ((byte_offset != 0 || seek != SEEK_SET) && fseek(f, (long)byte_offset, seek) != 0) {
        if (err) {
            err->code = err_code::FILE_SEEK_FAIL;
            err->str = strerror(errno);
        }
        return ret;
    }

    ret = fwrite(data, element_size, nelements, f);
    if (ret != nelements && err) {
        err->code = err_code::FILE_WRITE_DIFF_SIZE;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname,
                 const char *mode,
                 const void *data,
                 sizet element_size,
                 sizet nelements,
                 sizet byte_offset,
                 platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        ret = platform_write_file(f, data, element_size, nelements, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname, const void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    return write_file(fname, "wb", data, element_size, nelements, byte_offset, err);
}

sizet write_file(const char *fname, const char *mode, const byte_array *data, sizet byte_offset, platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        ret = platform_write_file(f, data, 1, data->size, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname, const byte_array *data, sizet byte_offset, platform_file_err_desc *err)
{
    return write_file(fname, "wb", data, byte_offset, err);
}

} // namespace nslib
