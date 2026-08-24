#pragma once

#include "osdef.h"
#if defined(PLATFORM_UNIX)
#include <ctime>
#endif

#include "basic_types.h"

    
#define NSEC_TO_SEC(nsec) (((f64)nsec) / 1000000000.0)
#define NSEC_TO_MSEC(nsec) (((f64)nsec) / 1000000.0)
#define NSEC_TO_USEC(nsec) (((f64)nsec) / 1000.0)
#define SEC_TO_NSEC(sec) s64(sec * 1000000000)
#define MSEC_TO_NSEC(msec) s64(msec * 1000000)
#define USEC_TO_NSEC(usec) s64(usec * 1000)

namespace nslib
{

enum ptimer_type
{
    PTIMER_TYPE_MONOTONIC,
    PTIMER_TYPE_REALTIME,
    PTIMER_TYPE_PROCESS_CPU,
    PTIMER_TYPE_THREAD_CPU
};

struct ptimespec
{
#if defined(PLATFORM_UNIX) 
    timespec t;
#elif defined(PLATFORM_WIN32)
    s64 t;
    s64 f;
#endif
};

struct profile_timepoints
{
    s32 ctype{};

    // Most recent timepoint - set at restart and update
    ptimespec split;

    // Time point at restart
    ptimespec restart;

    // Time, in us, between current split point and previous split (updated with ptimer_split)
    s64 dt_ns;

    // Time, in s, between current split point and previous split (updated with ptimer_split)
    f64 dt;
};

inline f64 nanos_to_sec(s64 ns) {
    return (f64)ns / 1000000000.0;
}

ptimespec ptimer_cur(s32 ptype);

ptimespec ptimer_diff(const ptimespec *start, const ptimespec *end);

s64 ptimer_nsec(const ptimespec *spec);

// Restart the timer setting all timepoints to current time
void ptimer_restart(profile_timepoints *ptimer);

// Update the timer split timespec with current time and dt_ns with elapsed nanoseconds since previous split
void ptimer_split(profile_timepoints *ptimer);

// Return elapsed nanoseconds between now and last split
s64 ptimer_split_dt(const profile_timepoints *ptimer);

// Return elapsed nanoseconds between now and last restart
s64 ptimer_elapsed_dt(const profile_timepoints *ptimer);
} // namespace nslib
