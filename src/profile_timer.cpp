#include "profile_timer.h"

#if defined(PLATFORM_WIN32)
    #define NOMINMAX
    #include <windows.h>
#endif

namespace nslib
{
ptimespec ptimer_cur(s32 ptype)
{
    ptimespec temp;
#if defined(PLATFORM_UNIX)
    clockid_t t;
    switch (ptype) {
    case (PTIMER_TYPE_MONOTONIC):
        t = CLOCK_MONOTONIC;
        break;
    case (PTIMER_TYPE_REALTIME):
        t = CLOCK_REALTIME;
        break;
    case (PTIMER_TYPE_PROCESS_CPU):
        t = CLOCK_PROCESS_CPUTIME_ID;
        break;
    case (PTIMER_TYPE_THREAD_CPU):
        t = CLOCK_THREAD_CPUTIME_ID;
        break;
    }
    clock_gettime(t, &temp.t);
#elif defined(PLATFORM_WIN32)
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    temp.t = t.QuadPart;
    temp.f = f.QuadPart;
#endif
    return temp;
}

ptimespec ptimer_diff(const ptimespec *start, const ptimespec *end)
{
    ptimespec temp;
#if defined(PLATFORM_UNIX)
    temp.t.tv_sec = end->t.tv_sec - start->t.tv_sec;
    temp.t.tv_nsec = end->t.tv_nsec - start->t.tv_nsec;
#elif defined(PLATFORM_WIN32)
    temp.t = end->t - start->t;
    temp.f = end->f;
#endif
    return temp;
}

s64 ptimer_nsec(const ptimespec *spec)
{
    s64 ret{};
#if defined(PLATFORM_UNIX)
    ret = SEC_TO_NSEC(spec->t.tv_sec) + spec->t.tv_nsec;
#elif defined(PLATFORM_WIN32)
    ret = SEC_TO_NSEC(spec->t / spec->f) + SEC_TO_NSEC(spec->t % spec->f) / spec->f;
#endif
    return ret;
}

void ptimer_restart(profile_timepoints *ptimer)
{
    ptimer->restart = ptimer_cur(ptimer->ctype);
    ptimer->split = ptimer->restart;
    ptimer->dt_ns = 0;
}

void ptimer_split(profile_timepoints *ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->split, &cur);
    ptimer->dt_ns = ptimer_nsec(&split_dt);
    ptimer->dt = nanos_to_sec(ptimer->dt_ns);
    ptimer->split = cur;
}

s64 ptimer_split_dt(const profile_timepoints *ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->split, &cur);
    return ptimer_nsec(&split_dt);
}

s64 ptimer_elapsed_dt(const profile_timepoints *ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->restart, &cur);
    return ptimer_nsec(&split_dt);
}

} // namespace nslib
