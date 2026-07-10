#include "render_thread.h"
#include "render_manifest.h"
#include "logging.h"

namespace nslib
{

// The one place a frame actually gets rendered. INLINE and LOCKSTEP both land here, so the
// only difference between the modes is which thread the call happens on.
intern bool render_one_frame(render_thread *rt, const render_frame_payload *p)
{
    reset_arena(&rt->cfg.rndr->arenas.scratch_flinear);
    rmanifest *m = begin_render_frame(rt->cfg.rndr, {.rbp = p->rbp, .frame_sdata = &p->fdata});

    // Null manifest means the swapchain went out of date. Not an error - skip the frame.
    if (!m) {
        return true;
    }

    m->frame_alpha = p->alpha;
    rt->cfg.build(m, p, rt->cfg.build_user);
    return end_render_frame(m);
}

intern void render_thread_proc(void *arg)
{
    ilog("Starting render thread");
    auto rt = (render_thread *)arg;
    while (true) {
        render_frame_payload p{};

        lock_mutex(&rt->mtx);
        // A cond var is a doorbell, not a mailbox - wait_cond_var can return with nobody
        // having signalled. state is the only truth, so re-check it in a loop.
        while (rt->state != RENDER_HANDOFF_SIM_READY && !rt->shutdown) {
            wait_cond_var(&rt->cv, &rt->mtx);
        }
        if (rt->shutdown) {
            unlock_mutex(&rt->mtx);
            break;
        }
        // Copy the payload out under the lock. Everything the frame needs is now local.
        p = rt->payload;
        unlock_mutex(&rt->mtx);

        bool ok = render_one_frame(rt, &p);

        lock_mutex(&rt->mtx);
        rt->frame_ok = ok;
        rt->state = RENDER_HANDOFF_RENDER_DONE;
        // Exactly one waiter (the sim thread) cares about this transition. If a third thread
        // ever waits on rt->cv this must become broadcast_cond_var.
        signal_cond_var(&rt->cv);
        unlock_mutex(&rt->mtx);
    }
}

bool init_render_thread(render_thread *rt, const render_thread_cfg &cfg)
{
    asrt(cfg.rndr);
    asrt(cfg.build);

    if (cfg.mode == RENDER_THREAD_MODE_PIPELINED) {
        elog("RENDER_THREAD_MODE_PIPELINED is not implemented yet");
        return false;
    }

    rt->cfg = cfg;
    rt->state = RENDER_HANDOFF_IDLE;
    rt->frame_ok = true;
    rt->shutdown = false;

    if (cfg.mode == RENDER_THREAD_MODE_INLINE) {
        return true;
    }

    init_mutex(&rt->mtx);
    init_cond_var(&rt->cv);
    thread_desc tdesc{};
    tdesc.arenas = &cfg.rndr->arenas;
    tdesc.idx = 1;
    tdesc.name = "render";
    if (!start_thread(&rt->thrd, tdesc, render_thread_proc, rt)) {
        terminate_cond_var(&rt->cv);
        terminate_mutex(&rt->mtx);
        return false;
    }
    return true;
}

void terminate_render_thread(render_thread *rt)
{
    if (rt->cfg.mode == RENDER_THREAD_MODE_INLINE) {
        return;
    }

    lock_mutex(&rt->mtx);
    rt->shutdown = true;
    // Broadcast rather than signal - shutdown doesn't care who is waiting.
    broadcast_cond_var(&rt->cv);
    unlock_mutex(&rt->mtx);

    join_thread(&rt->thrd);
    terminate_cond_var(&rt->cv);
    terminate_mutex(&rt->mtx);
}

bool submit_render_frame(render_thread *rt, const render_frame_payload &payload)
{
    if (rt->cfg.mode == RENDER_THREAD_MODE_INLINE) {
        return render_one_frame(rt, &payload);
    }

    lock_mutex(&rt->mtx);
    rt->payload = payload;
    rt->state = RENDER_HANDOFF_SIM_READY;
    signal_cond_var(&rt->cv);

    // Lockstep: block until the render thread has presented. This barrier is what makes it
    // safe for the build cb to touch live sim state today. Step 5 removes it.
    while (rt->state != RENDER_HANDOFF_RENDER_DONE) {
        wait_cond_var(&rt->cv, &rt->mtx);
    }
    rt->state = RENDER_HANDOFF_IDLE;
    bool ok = rt->frame_ok;
    unlock_mutex(&rt->mtx);
    return ok;
}

} // namespace nslib
