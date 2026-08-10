#include "render_thread.h"
#include "render_manifest.h"
#include "logging.h"

namespace nslib
{

intern const u32 FRESH_BIT = make_flag(2);

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
    u32 read_idx{RENDER_PAYLOAD_INIT_OWNER_IND_RNDR};
    auto rt = (render_thread *)arg;
    while (true) {
        u32 cur = rt->tb.pub_idx.load(std::memory_order_relaxed);
        // Peek to see if the snapshot is fresh - if so get it and exchange in the read idx without the fresh bit set
        if (test_flags(cur, FRESH_BIT)) {
            read_idx = rt->tb.pub_idx.exchange(read_idx & ~FRESH_BIT, std::memory_order_acquire);
        }

        // We test for the fresh bit because on first render there will be nothing...
        if (test_flags(read_idx, FRESH_BIT)) {
            render_one_frame(rt, &rt->tb.payloads[(read_idx & ~FRESH_BIT)]);
        }
    }
}

render_frame_payload *get_write_slot(render_thread *rt)
{
    return &rt->tb.payloads[rt->tb.write_idx];
}

bool init_render_thread(render_thread *rt, const render_thread_cfg &cfg)
{
    asrt(cfg.rndr);
    asrt(cfg.build);
    rt->cfg = cfg;

    // Inline we simply don't create the thread
    if (cfg.mode == RENDER_THREAD_MODE_INLINE) {
        return true;
    }

    thread_desc tdesc{};
    tdesc.arenas = &cfg.rndr->arenas;
    tdesc.idx = 1;
    tdesc.name = "render";
    if (!start_thread(&rt->thrd, tdesc, render_thread_proc, rt)) {
        return false;
    }
    return true;
}

void terminate_render_thread(render_thread *rt)
{
    if (rt->cfg.mode == RENDER_THREAD_MODE_INLINE) {
        return;
    }
    join_thread(&rt->thrd);
}

bool submit_render_frame(render_thread *rt)
{
    // If inline, we render the frame directly
    if (rt->cfg.mode == RENDER_THREAD_MODE_INLINE) {
        return render_one_frame(rt, &rt->tb.payloads[rt->tb.write_idx]);
    }
    rt->tb.write_idx = rt->tb.pub_idx.exchange(rt->tb.write_idx | FRESH_BIT, std::memory_order_release) & ~FRESH_BIT;
    return true;
}

} // namespace nslib
