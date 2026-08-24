#include "render_thread.h"
#include "render_manifest.h"
#include "logging.h"

#if defined(USE_IMGUI)
    #include "imgui/imgui.h"
#endif

namespace nslib
{

// Both must run on the thread that owns the imgui allocator (the platform thread) -
// IM_NEW/IM_DELETE route through rndr->imgui.fl, which is not thread-safe. The render
// thread may READ the returned ImDrawData (RenderDrawData) but must never free it.
// Requires: #include "imgui/imgui.h"

// Deep-copies src into a freshly allocated ImDrawData. Valid until you free_imgui_draw_data it,
// independent of imgui's next NewFrame(). src must be live (post-Render, pre-next-NewFrame).
ImDrawData *clone_imgui_draw_data(const ImDrawData *src)
{
    ImDrawData *dst = IM_NEW(ImDrawData);
    dst->Valid = src->Valid;
    dst->CmdListsCount = src->CmdListsCount;
    dst->TotalIdxCount = src->TotalIdxCount;
    dst->TotalVtxCount = src->TotalVtxCount;
    dst->DisplayPos = src->DisplayPos;
    dst->DisplaySize = src->DisplaySize;
    dst->FramebufferScale = src->FramebufferScale;
    dst->OwnerViewport = nullptr; // backend doesn't need it single-viewport

    // CloneOutput() deep-copies each list's CmdBuffer/IdxBuffer/VtxBuffer into a new heap
    // ImDrawList. Clip rects and texture refs ride along inside CmdBuffer.
    dst->CmdLists.reserve(src->CmdListsCount);
    for (int i = 0; i < src->CmdListsCount; ++i) {
        dst->CmdLists.push_back(src->CmdLists[i]->CloneOutput());
    }
    return dst;
}

// Frees the cloned lists and the ImDrawData itself. Safe on null.
void free_imgui_draw_data(ImDrawData *dd)
{
    if (!dd) {
        return;
    }
    for (int i = 0; i < dd->CmdLists.Size; ++i) {
        IM_DELETE(dd->CmdLists[i]);
    }
    // IM_DELETE(dd) runs ~ImDrawData(), whose ImVector destructor frees CmdLists' own
    // pointer array - so no need to clear it first.
    IM_DELETE(dd);
}

intern const u32 FRESH_BIT = make_flag(2);

// The one place a frame actually gets rendered. INLINE and LOCKSTEP both land here, so the
// only difference between the modes is which thread the call happens on.
intern bool render_one_frame(render_thread *rt)
{
    const render_frame_payload *p = &rt->tb.payloads[rt->tb.read_idx & ~FRESH_BIT];
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
    while (!rt->shutdown.load(std::memory_order_relaxed)) {
        u32 cur = rt->tb.pub_idx.load(std::memory_order_relaxed);
        // Peek to see if the snapshot is fresh - if so get it and exchange in the read idx without the fresh bit set
        if (test_flags(cur, FRESH_BIT)) {
            rt->tb.read_idx = rt->tb.pub_idx.exchange(rt->tb.read_idx & ~FRESH_BIT, std::memory_order_acquire);
        }

        // We test for the fresh bit because on first render there will be nothing...
        if (test_flags(rt->tb.read_idx, FRESH_BIT)) {
            bool keep_running = render_one_frame(rt);
            // We use the same shutdown flag so sim can read this flag also and shutdown if needed
            if (!keep_running) rt->shutdown.store(true, std::memory_order_relaxed);
        }
    }
}

render_frame_payload *get_write_slot(render_thread *rt)
{
    return &rt->tb.payloads[rt->tb.write_idx];
}

bool init_render_thread(render_thread *rt, mem_arena *upstream, const render_thread_cfg &cfg)
{
    intern char FRAME_PAYLOAD_BUFFER[24]{};
    asrt(cfg.rndr);
    asrt(cfg.build);
    asrt(upstream);
    asrt(cfg.frame_payload_arena_size > 0);
    rt->cfg = cfg;

    // Inline we simply don't create the thread
    for (int i = 0; i < RENDER_PAYLOAD_COUNT; ++i) {
        snprintf(FRAME_PAYLOAD_BUFFER, 24, "frame-%d-payload", i + 1);
        init_arena(&rt->tb.payloads[i].arena, cfg.frame_payload_arena_size, mem_alloc_type::LINEAR, upstream, FRAME_PAYLOAD_BUFFER);
    }
    
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
    rt->shutdown.store(true, std::memory_order_relaxed);
    join_thread(&rt->thrd);
    for (u32 i = 0; i < RENDER_PAYLOAD_COUNT; ++i) {
        terminate_arena(&rt->tb.payloads[i].arena);
    }
}

bool submit_render_frame(render_thread *rt)
{
    auto rfp = get_write_slot(rt);

#if defined(USE_IMGUI)
    ImGui::Render();
    rfp->imgui_data = clone_imgui_draw_data(ImGui::GetDrawData());
#endif

    rt->tb.write_idx = rt->tb.pub_idx.exchange(rt->tb.write_idx | FRESH_BIT, std::memory_order_release) & ~FRESH_BIT;
    auto next_rfp = get_write_slot(rt);
    reset_arena(&next_rfp->arena);

#if defined(USE_IMGUI)
    if (next_rfp->imgui_data) {
        free_imgui_draw_data((ImDrawData *)next_rfp->imgui_data);
        next_rfp->imgui_data = nullptr;
    }
#endif

    if (rt->cfg.mode == RENDER_THREAD_MODE_INLINE) {
        rt->tb.read_idx = rt->tb.pub_idx.exchange(rt->tb.read_idx & ~FRESH_BIT, std::memory_order_acquire);
        return render_one_frame(rt);
    }
    else {
        return !rt->shutdown.load(std::memory_order_relaxed);
    }
}

} // namespace nslib
