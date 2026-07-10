#include "threads.h"
#include "logging.h"
#include "memory.h"
#include <cstring>

#if defined(PLATFORM_POSIX)
    #include <pthread.h>
#elif defined(PLATFORM_WIN32)
    #define NOMINMAX
    #include <windows.h>
    #include <process.h>
#endif

namespace nslib
{

intern thread_local u32 g_thread_idx{INVALID_IDX};
intern thread_local mem_arena_group *g_thread_arenas{nullptr};

u32 current_thread_idx()
{
    return g_thread_idx;
}

mem_arena_group *current_thread_arenas()
{
    return g_thread_arenas;
}

mem_arena *current_thread_free_list()
{
    return &g_thread_arenas->free_list;
}

mem_arena *current_thread_stack()
{
    return &g_thread_arenas->stack;
}

mem_arena *current_thread_scratch_flinear()
{
    return &g_thread_arenas->scratch_flinear;
}

void register_current_thread(u32 idx, mem_arena_group *thread_arenas)
{
    g_thread_idx = idx;
    g_thread_arenas = thread_arenas;
}

#if defined(PLATFORM_POSIX)

static_assert(sizeof(pthread_t) <= sizeof(void *), "pthread_t does not fit in thread::native");
static_assert(sizeof(pthread_mutex_t) <= sizeof(mutex::native), "pthread_mutex_t too large for mutex storage");
static_assert(sizeof(pthread_cond_t) <= sizeof(cond_var::native), "pthread_cond_t too large for cond_var storage");

intern void *thread_trampoline(void *data)
{
    thread *t = (thread *)data;
    register_current_thread(t->idx, t->arenas);
    t->func(t->arg);
    return nullptr;
}

bool start_thread(thread *t, const thread_desc &desc, thread_func func, void *arg)
{
    t->arenas = desc.arenas;
    t->idx = desc.idx;
    t->func = func;
    t->arg = arg;
    strncpy(t->name, desc.name, SMALL_STR_LEN);
    pthread_t tid;
    int res = pthread_create(&tid, nullptr, thread_trampoline, t);
    if (res != 0) {
        elog("Failed to create thread (err %d)", res);
        return false;
    }
    t->native = (void *)tid;
    return true;
}

void join_thread(thread *t)
{
    pthread_join((pthread_t)t->native, nullptr);
    t->native = nullptr;
}

void init_mutex(mutex *m)
{
    pthread_mutex_init((pthread_mutex_t *)m->native, nullptr);
}

void terminate_mutex(mutex *m)
{
    pthread_mutex_destroy((pthread_mutex_t *)m->native);
}

void lock_mutex(mutex *m)
{
    pthread_mutex_lock((pthread_mutex_t *)m->native);
}

bool try_lock_mutex(mutex *m)
{
    return pthread_mutex_trylock((pthread_mutex_t *)m->native) == 0;
}

void unlock_mutex(mutex *m)
{
    pthread_mutex_unlock((pthread_mutex_t *)m->native);
}

void init_cond_var(cond_var *c)
{
    pthread_cond_init((pthread_cond_t *)c->native, nullptr);
}

void terminate_cond_var(cond_var *c)
{
    pthread_cond_destroy((pthread_cond_t *)c->native);
}

void wait_cond_var(cond_var *c, mutex *m)
{
    pthread_cond_wait((pthread_cond_t *)c->native, (pthread_mutex_t *)m->native);
}

void signal_cond_var(cond_var *c)
{
    pthread_cond_signal((pthread_cond_t *)c->native);
}

void broadcast_cond_var(cond_var *c)
{
    pthread_cond_broadcast((pthread_cond_t *)c->native);
}

#elif defined(PLATFORM_WIN32)

static_assert(sizeof(CRITICAL_SECTION) <= sizeof(mutex::native), "CRITICAL_SECTION too large for mutex storage");
static_assert(sizeof(CONDITION_VARIABLE) <= sizeof(cond_var::native), "CONDITION_VARIABLE too large for cond_var storage");

intern unsigned __stdcall thread_trampoline(void *data)
{
    thread *t = (thread *)data;
    register_current_thread(t->idx, t->arenas);
    t->func(t->arg);
    return 0;
}

bool start_thread(thread *t, thread_func func, void *arg)
{
    t->func = func;
    t->arg = arg;
    // _beginthreadex over CreateThread so per-thread CRT state is set up for any CRT calls (logging etc).
    uintptr_t h = _beginthreadex(nullptr, 0, thread_trampoline, t, 0, nullptr);
    if (h == 0) {
        elog("Failed to create thread (err %d)", errno);
        return false;
    }
    t->native = (void *)h;
    return true;
}

void join_thread(thread *t)
{
    WaitForSingleObject((HANDLE)t->native, INFINITE);
    CloseHandle((HANDLE)t->native);
    t->native = nullptr;
}

void init_mutex(mutex *m)
{
    InitializeCriticalSection((CRITICAL_SECTION *)m->native);
}

void terminate_mutex(mutex *m)
{
    DeleteCriticalSection((CRITICAL_SECTION *)m->native);
}

void lock_mutex(mutex *m)
{
    EnterCriticalSection((CRITICAL_SECTION *)m->native);
}

bool try_lock_mutex(mutex *m)
{
    return TryEnterCriticalSection((CRITICAL_SECTION *)m->native) != 0;
}

void unlock_mutex(mutex *m)
{
    LeaveCriticalSection((CRITICAL_SECTION *)m->native);
}

void init_cond_var(cond_var *c)
{
    InitializeConditionVariable((CONDITION_VARIABLE *)c->native);
}

void terminate_cond_var(cond_var *c)
{
    // Win32 CONDITION_VARIABLE needs no explicit teardown.
    (void)c;
}

void wait_cond_var(cond_var *c, mutex *m)
{
    SleepConditionVariableCS((CONDITION_VARIABLE *)c->native, (CRITICAL_SECTION *)m->native, INFINITE);
}

void signal_cond_var(cond_var *c)
{
    WakeConditionVariable((CONDITION_VARIABLE *)c->native);
}

void broadcast_cond_var(cond_var *c)
{
    WakeAllConditionVariable((CONDITION_VARIABLE *)c->native);
}

#endif

} // namespace nslib
