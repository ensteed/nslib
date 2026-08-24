#pragma once
#include "atomic_types.h"
#include "renderer.h"
#include "engine_rendering.h"
#include "threads.h"

namespace nslib
{

struct render_frame_payload
{
    mem_arena arena;
    f32 alpha;
    void *imgui_data;
    frame_ubo_data fdata;
    render_blueprint_ref rbp; // becomes a handle after step 4
};

// When the render thread first starts, each frame needs to have an initial owner
enum render_payload_init_owner_ind
{
    RENDER_PAYLOAD_INIT_OWNER_IND_SIM,
    RENDER_PAYLOAD_INIT_OWNER_IND_PUB,
    RENDER_PAYLOAD_INIT_OWNER_IND_RNDR,
    RENDER_PAYLOAD_COUNT
};

enum render_thread_mode
{
    RENDER_THREAD_MODE_INLINE,    // no thread; runs on the caller. debug / repro.
    RENDER_THREAD_MODE_LOCKSTEP,  // thread; sim blocks until present. step 2.
    RENDER_THREAD_MODE_PIPELINED, // thread; sim runs ahead. step 5.
};

// Runs on the render thread. Touch only m, p, and immutable assets - reaching through user
// to read live sim state works in LOCKSTEP and corrupts in PIPELINED.
using render_build_cb = void (*)(rmanifest *m, const render_frame_payload *p, void *user);

struct render_thread_cfg
{
    renderer *rndr; // already initialized
    render_thread_mode mode;
    sizet frame_payload_arena_size;
    render_build_cb build; // required
    void *build_user;
};

enum render_handoff_state
{
    RENDER_HANDOFF_IDLE,
    RENDER_HANDOFF_SIM_READY,
    RENDER_HANDOFF_RENDER_DONE,
};

struct rt_triple_buffer
{
    render_frame_payload payloads[RENDER_PAYLOAD_COUNT];

    // Holds exactly one buffer index.
    // Producer swaps completed write buffer into it.
    // Consumer swaps old read buffer into it.
    atomic_u32 pub_idx{RENDER_PAYLOAD_INIT_OWNER_IND_PUB};
    // Sim (platform) owned
    u32 write_idx{RENDER_PAYLOAD_INIT_OWNER_IND_SIM};
    // Render thread owned except in inline mode
    u32 read_idx{RENDER_PAYLOAD_INIT_OWNER_IND_RNDR};
};

struct render_thread
{
    // Immutable after init_render_thread - readable from either thread with no lock.
    render_thread_cfg cfg{};

    // Unused when cfg.mode == RENDER_THREAD_MODE_INLINE.
    thread thrd{};

    // Shutdown flag
    atomic_b8 shutdown{false};

    // Shared payloads
    rt_triple_buffer tb{};
};

// Spawns the thread (unless mode == INLINE). Asserts cfg.build != nullptr.
// Call AFTER init_renderer and after all blueprints are authored + compiled.
bool init_render_thread(render_thread *rt, mem_arena *upstream, const render_thread_cfg &cfg);

// Signals shutdown, joins, then tears down the sync primitives. Must be called before
// terminate_renderer - the render thread touches the renderer right up until it joins.
void terminate_render_thread(render_thread *rt);

// Must be called from the platform thread.
render_frame_payload *get_write_slot(render_thread *rt);

// Must be called from the window-owning (platform) thread. Returns false when the frame
// asked the app to stop. Under PIPELINED this reports the PREVIOUS frame's result.
bool submit_render_frame(render_thread *rt);

} // namespace nslib
