#pragma once

#include "renderer.h"
#include "engine_rendering.h"
#include "threads.h"
#include "stdatomic.h"

namespace nslib
{

struct render_frame_payload
{
    f32 alpha;
    frame_ubo_data fdata;
    render_blueprint_ref rbp; // becomes a handle after step 4
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
    render_build_cb build; // required
    void *build_user;
};

enum render_handoff_state
{
    RENDER_HANDOFF_IDLE,
    RENDER_HANDOFF_SIM_READY,
    RENDER_HANDOFF_RENDER_DONE,
};

struct rt_triple_buffer {
    render_frame_payload payloads[3];
    
    // Holds exactly one buffer index.
    // Producer swaps completed write buffer into it.
    // Consumer swaps old read buffer into it.
    atomic_int ready_idx;
};

struct render_thread
{
    // Immutable after init_render_thread - readable from either thread with no lock.
    render_thread_cfg cfg{};

    // Unused when cfg.mode == RENDER_THREAD_MODE_INLINE.
    thread thrd{};

    // Shared payloads
    render_frame_payload payloads[3];
    
    // Holds buffer index in first 2 bits and fresh bit in bit 3.
    // Producer swaps completed write buffer into it along with set fresh bit.
    // Consumer swaps old read buffer into it with fresh bit as 0. The consumer only does the swap if the fresh bit is set.
    atomic_uint pub_idx;
};

// Spawns the thread (unless mode == INLINE). Asserts cfg.build != nullptr.
// Call AFTER init_renderer and after all blueprints are authored + compiled.
bool init_render_thread(render_thread *rt, const render_thread_cfg &cfg);

// Signals shutdown, joins, then tears down the sync primitives. Must be called before
// terminate_renderer - the render thread touches the renderer right up until it joins.
void terminate_render_thread(render_thread *rt);

// Must be called from the window-owning (platform) thread. Returns false when the frame
// asked the app to stop. Under PIPELINED this reports the PREVIOUS frame's result.
bool submit_render_frame(render_thread *rt, const render_frame_payload &payload);

} // namespace nslib
