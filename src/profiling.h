#pragma once

#include "basic_types.h"
#include "memory.h"
#include "profile_timer.h"
#include "containers/array.h"
#include "containers/hmap.h"

namespace nslib
{

#define PROFILE_SCOPE_T(name, i) nslib::profiling_scope _nslib_profile_scope(nslib::GLOBAL_PROFILING_CONTEXT[i], name)
#define PROFILE_SCOPE(name) PROFILE_SCOPE_T(name, PROFILE_MAIN_THREAD_ID)

#define PROFILE_BEGIN_T(name, i) profiling_begin(nslib::GLOBAL_PROFILING_CONTEXT[i], name)
#define PROFILE_BEGIN(name) PROFILE_BEGIN_T(name, PROFILE_MAIN_THREAD_ID)

#define PROFILE_END_T(i) profiling_end(nslib::GLOBAL_PROFILING_CONTEXT[i])
#define PROFILE_END() PROFILE_END_T(PROFILE_MAIN_THREAD_ID)

#define PROFILE_BEGIN_FRAME_T(i) profiling_begin_frame(nslib::GLOBAL_PROFILING_CONTEXT[i])
#define PROFILE_BEGIN_FRAME() PROFILE_BEGIN_FRAME_T(PROFILE_MAIN_THREAD_ID)

#define PROFILE_END_FRAME_T(i) profiling_end_frame(nslib::GLOBAL_PROFILING_CONTEXT[i])
#define PROFILE_END_FRAME() PROFILE_END_FRAME_T(PROFILE_MAIN_THREAD_ID)
    

#define PROFILE_PRINT_REPORT_T(i) profiling_report(nslib::GLOBAL_PROFILING_CONTEXT[i])
#define PROFILE_PRINT_REPORT() PROFILE_PRINT_REPORT_T(PROFILE_MAIN_THREAD_ID)

inline constexpr sizet PROFILE_CONTEXT_COUNT = 4;
inline constexpr sizet PROFILE_MAIN_THREAD_ID = 0;

struct profiling_entry
{
    const char *name{};
    s32 parent_index{-1};
    s32 depth{};
    s64 total_ns{};
    u32 hits{};
};

struct profiling_stack_entry
{
    s32 entry_index{-1};
    s64 start_ns{};
};

struct profiling_avg_entry
{
    f64 avg_ns{};
    f64 avg_hits{};
    u64 last_frame{};
};

struct profiling_key
{
    const char *name{};
    s32 parent_index{-1};
};

op_eq_func(profiling_key)
{
    return lhs.name == rhs.name && lhs.parent_index == rhs.parent_index;
}

struct profiling_context
{
    profile_timepoints timer{};
    array<profiling_entry> entries{};
    array<profiling_stack_entry> stack{};
    hmap<profiling_key, s32> entry_lookup{};
    hmap<profiling_key, profiling_avg_entry> avg_lookup{};
    sizet frame_start_entry_count{};
    sizet frame_start_stack_depth{};
    s64 frame_start_ns{};
    s64 frame_total_ns{};
    f64 avg_frame_total_ns{};
    u64 frame_index{};
    u32 avg_window{};
    mem_arena arena{};
    mem_arena *avg_arena{};
};

extern profiling_context *GLOBAL_PROFILING_CONTEXT[PROFILE_CONTEXT_COUNT];

struct profiling_scope
{
    profiling_context *ctxt{};
    profiling_scope(profiling_context *scope_ctxt, const char *name);
    ~profiling_scope();
};

void profiling_init(profiling_context *ctxt, sizet entry_count = 128, sizet stack_depth = 64, mem_arena *upstream = get_global_arena());
void profiling_terminate(profiling_context *ctxt);

void profiling_begin_frame(profiling_context *ctxt);
void profiling_end_frame(profiling_context *ctxt);

void profiling_begin(profiling_context *ctxt, const char *name);
void profiling_end(profiling_context *ctxt);

void profiling_report(const profiling_context *ctxt);

void profiling_set_avg_window(profiling_context *ctxt, u32 window_frames, mem_arena *avg_arena = get_global_arena());

} // namespace nslib
