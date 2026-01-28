#include "profiling.h"

#include <stdio.h>

#include "hashfuncs.h"

namespace nslib
{

profiling_scope::profiling_scope(profiling_context *scope_ctxt, const char *name) : ctxt(scope_ctxt)
{
    if (ctxt) {
        profiling_begin(ctxt, name);
    }
}

profiling_scope::~profiling_scope()
{
    if (ctxt) {
        profiling_end(ctxt);
    }
}

intern s64 profiling_now_ns(const profiling_context *ctxt)
{
    ptimespec now = ptimer_cur(ctxt->timer.ctype);
    return ptimer_nsec(&now);
}

intern u64 profiling_hash_key(const profiling_key &key, u64 seed0, u64 seed1)
{
    u64 h0 = hash_type(&key.name, sizeof(key.name), seed0, seed1);
    u64 h1 = hash_type(&key.parent_index, sizeof(key.parent_index), seed0 ^ 0x9e3779b97f4a7c15ULL, seed1);
    return h0 ^ (h1 + 0x9e3779b97f4a7c15ULL + (h0 << 6) + (h0 >> 2));
}

intern void profiling_reset_storage(profiling_context *ctxt, sizet entry_capacity, sizet stack_capacity, sizet map_capacity)
{
    ctxt->entries = {};
    ctxt->entries.arena = &ctxt->arena;
    arr_init(&ctxt->entries, &ctxt->arena, entry_capacity);

    ctxt->stack = {};
    ctxt->stack.arena = &ctxt->arena;
    arr_init(&ctxt->stack, &ctxt->arena, stack_capacity);

    ctxt->entry_lookup = {};
    hmap_init(&ctxt->entry_lookup, profiling_hash_key, &ctxt->arena, map_capacity);
}

intern f64 profiling_avg_alpha(const profiling_context *ctxt)
{
    if (!ctxt || ctxt->avg_window <= 1) {
        return 1.0;
    }
    return 2.0 / ((f64)ctxt->avg_window + 1.0);
}

intern void profiling_update_averages(profiling_context *ctxt)
{
    if (!ctxt || ctxt->avg_window == 0) {
        return;
    }

    ctxt->frame_index += 1;
    f64 alpha = profiling_avg_alpha(ctxt);
    if (ctxt->frame_index == 1) {
        ctxt->avg_frame_total_ns = (f64)ctxt->frame_total_ns;
    }
    else {
        ctxt->avg_frame_total_ns = ctxt->avg_frame_total_ns + alpha * ((f64)ctxt->frame_total_ns - ctxt->avg_frame_total_ns);
    }

    for (sizet i = 0; i < ctxt->entries.size; ++i) {
        const profiling_entry *entry = &ctxt->entries.data[i];
        profiling_key key{};
        key.name = entry->name ? entry->name : "";
        key.parent_index = entry->parent_index;

        auto avg_item = hmap_find(&ctxt->avg_lookup, key);
        if (!avg_item) {
            profiling_avg_entry avg_entry{};
            avg_entry.avg_ns = (f64)entry->total_ns;
            avg_entry.avg_hits = (f64)entry->hits;
            avg_entry.last_frame = ctxt->frame_index;
            hmap_insert(&ctxt->avg_lookup, key, avg_entry);
        }
        else {
            avg_item->val.avg_ns = avg_item->val.avg_ns + alpha * ((f64)entry->total_ns - avg_item->val.avg_ns);
            avg_item->val.avg_hits = avg_item->val.avg_hits + alpha * ((f64)entry->hits - avg_item->val.avg_hits);
            avg_item->val.last_frame = ctxt->frame_index;
        }
    }

    auto iter = hmap_begin(&ctxt->avg_lookup);
    while (iter) {
        if (iter->val.last_frame != ctxt->frame_index) {
            iter->val.avg_ns = iter->val.avg_ns + alpha * (0.0 - iter->val.avg_ns);
            iter->val.avg_hits = iter->val.avg_hits + alpha * (0.0 - iter->val.avg_hits);
        }
        iter = hmap_next(&ctxt->avg_lookup, iter);
    }
}

void profiling_init(profiling_context *ctxt, sizet entry_count, sizet stack_depth, mem_arena *upstream)
{
    asrt(ctxt);
    ctxt->frame_start_entry_count = entry_count;
    ctxt->frame_start_stack_depth = stack_depth;
    sizet entry_sz = (sizeof(profiling_entry) + sizeof(alloc_header) + SIMD_MIN_ALIGNMENT) * entry_count;
    sizet stack_sz = (sizeof(profiling_stack_entry) + sizeof(alloc_header) + SIMD_MIN_ALIGNMENT) * stack_depth;
    sizet map_sz = (sizeof(hmap_bucket<profiling_key, s32>) + sizeof(alloc_header) + SIMD_MIN_ALIGNMENT) * entry_count * 2;
    sizet arena_size = (entry_sz + stack_sz + map_sz) * 1.5;
    init_lin_arena(&ctxt->arena, arena_size, upstream, "profiling");
    profiling_reset_storage(ctxt, entry_count, stack_depth, entry_count * 2);
    ctxt->avg_arena = upstream ? upstream : get_global_arena();
    ctxt->avg_lookup = {};
    hmap_init(&ctxt->avg_lookup, profiling_hash_key, ctxt->avg_arena, entry_count * 2);
    ctxt->avg_frame_total_ns = 0.0;
    ctxt->frame_index = 0;
    ctxt->avg_window = 0;
    ctxt->timer.ctype = PTIMER_TYPE_REALTIME;
    ptimer_restart(&ctxt->timer);
    ctxt->frame_start_ns = profiling_now_ns(ctxt);
    ctxt->frame_total_ns = 0;
}

void profiling_terminate(profiling_context *ctxt)
{
    asrt(ctxt);
    hmap_terminate(&ctxt->avg_lookup);
    hmap_terminate(&ctxt->entry_lookup);
    arr_terminate(&ctxt->entries);
    arr_terminate(&ctxt->stack);
    ctxt->frame_start_ns = 0;
    ctxt->frame_total_ns = 0;
    ctxt->avg_frame_total_ns = 0.0;
    ctxt->frame_index = 0;
    ctxt->avg_window = 0;
    ctxt->avg_arena = nullptr;
    terminate_arena(&ctxt->arena);
}

void profiling_begin_frame(profiling_context *ctxt)
{
    asrt(ctxt);
    reset_arena(&ctxt->arena);
    profiling_reset_storage(ctxt, ctxt->frame_start_entry_count, ctxt->frame_start_stack_depth, ctxt->frame_start_entry_count * 2);
    ctxt->frame_start_ns = profiling_now_ns(ctxt);
    ctxt->frame_total_ns = 0;
}

void profiling_end_frame(profiling_context *ctxt)
{
    asrt(ctxt);
    asrt(ctxt->stack.size == 0);
    s64 now = profiling_now_ns(ctxt);
    ctxt->frame_total_ns = now - ctxt->frame_start_ns;
    profiling_update_averages(ctxt);
}

void profiling_begin(profiling_context *ctxt, const char *name)
{
    asrt(ctxt);
    s32 parent_index = -1;
    if (ctxt->stack.size > 0) {
        parent_index = ctxt->stack.data[ctxt->stack.size - 1].entry_index;
    }

    profiling_key key{};
    key.name = name ? name : "";
    key.parent_index = parent_index;

    s32 entry_index = -1;
    auto found = hmap_find(&ctxt->entry_lookup, key);
    if (found) {
        entry_index = found->val;
    }
    else {
        profiling_entry *entry = arr_emplace_back(&ctxt->entries);
        entry->parent_index = parent_index;
        entry->depth = (s32)ctxt->stack.size;
        entry->total_ns = 0;
        entry->hits = 0;
        entry->name = key.name;
        entry_index = (s32)(ctxt->entries.size - 1);
        hmap_insert(&ctxt->entry_lookup, key, entry_index);
    }

    profiling_stack_entry stack_entry{};
    stack_entry.entry_index = entry_index;
    stack_entry.start_ns = profiling_now_ns(ctxt);
    arr_push_back(&ctxt->stack, stack_entry);
}

void profiling_end(profiling_context *ctxt)
{
    asrt(ctxt);
    asrt(ctxt->stack.size != 0);
    profiling_stack_entry *stack_entry = arr_back(&ctxt->stack);
    s64 now = profiling_now_ns(ctxt);
    s64 elapsed = now - stack_entry->start_ns;
    s32 entry_index = stack_entry->entry_index;
    asrt(entry_index >= 0);
    if ((sizet)entry_index < ctxt->entries.size) {
        profiling_entry *entry = &ctxt->entries.data[entry_index];
        entry->total_ns += elapsed;
        entry->hits += 1;
    }
    arr_pop_back(&ctxt->stack);
}

intern void profiling_report_line(const profiling_context *ctxt, const profiling_entry *entry)
{
    char prefix[32]{};
    for (int i = 0; i < entry->depth && i < 31; ++i) {
        strcat(prefix,"-");
    }
    if (entry->depth > 0 && entry->depth < 30) {
        strcat(prefix,">");
    }
    
    f64 frame_percent = 0.0;
    if (ctxt->frame_total_ns > 0) {
        frame_percent = ((f64)entry->total_ns / (f64)ctxt->frame_total_ns) * 100.0;
    }
    f64 ms = NSEC_TO_MSEC(entry->total_ns);
    if (ctxt->avg_window > 0) {
        profiling_key key{};
        key.name = entry->name ? entry->name : "";
        key.parent_index = entry->parent_index;
        auto avg_item = hmap_find(&ctxt->avg_lookup, key);
        f64 avg_frame_percent = 0.0;
        f64 avg_ms = 0.0;
        f64 avg_hits = 0.0;
        if (avg_item) {
            if (ctxt->avg_frame_total_ns > 0.0) {
                avg_frame_percent = avg_item->val.avg_ns / ctxt->avg_frame_total_ns * 100.0;
            }
            avg_ms = avg_item->val.avg_ns / 1000000.0;
            avg_hits = avg_item->val.avg_hits;
        }
        ilog("%s%s: avg_frame=%.2f%% avg_ms=%.4f avg_hits=%.2f", prefix, entry->name ? entry->name : "", avg_frame_percent, avg_ms, avg_hits);
    }
    else {
        ilog("%s%s: ticks=%ld frame=%.2f%% ms=%.4f hits=%u", prefix, entry->name ? entry->name : "", entry->total_ns, frame_percent, ms, entry->hits);
    }
}

intern void profiling_report_children(const profiling_context *ctxt, s32 parent_index)
{
    for (sizet i = 0; i < ctxt->entries.size; ++i) {
        const profiling_entry *entry = &ctxt->entries.data[i];
        if (entry->parent_index == parent_index) {
            profiling_report_line(ctxt, entry);
            profiling_report_children(ctxt, (s32)i);
        }
    }
}

void profiling_report(const profiling_context *ctxt)
{
    if (!ctxt) {
        return;
    }
    f64 frame_ms = NSEC_TO_MSEC(ctxt->frame_total_ns);
    if (ctxt->avg_window > 0) {
        f64 avg_frame_ms = ctxt->avg_frame_total_ns / 1000000.0;
        ilog("avg_frame_ms=%.3f avg_window=%u", avg_frame_ms, ctxt->avg_window);
    }
    else {
        ilog("frame_ticks=%ld frame_ms=%.3f", ctxt->frame_total_ns, frame_ms);
    }
    profiling_report_children(ctxt, -1);
}

void profiling_set_avg_window(profiling_context *ctxt, u32 window_frames, mem_arena *avg_arena)
{
    asrt(ctxt);
    ctxt->avg_window = window_frames;
    ctxt->avg_arena = avg_arena;
    hmap_terminate(&ctxt->avg_lookup);
    ctxt->avg_lookup = {};
    hmap_init(&ctxt->avg_lookup, profiling_hash_key, ctxt->avg_arena, ctxt->frame_start_entry_count * 2);
    ctxt->avg_frame_total_ns = 0.0;
    ctxt->frame_index = 0;
}

} // namespace nslib
