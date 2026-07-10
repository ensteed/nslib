#pragma once
#include "basic_types.h"

namespace nslib
{
inline constexpr u32 MAX_THREADS = 32;

struct mem_arena_group;
struct mem_arena;

// Entry point for a thread. The OS thread's return value is ignored on purpose - communicate
// results back through shared state guarded by a mutex/cond_var, not a return value.
using thread_func = void (*)(void *arg);

struct thread_desc
{
    u32 idx;
    mem_arena_group *arenas;
    const char *name;
};

struct thread
{
    u32 idx;
    mem_arena_group *arenas;
    small_str name;
    // Opaque native handle - pthread_t on posix, the thread HANDLE on win32.
    void *native{};
    thread_func func{};
    void *arg{};
};

// The native primitives (pthread_mutex_t, CRITICAL_SECTION, etc) are stored in these opaque
// buffers so this header stays free of <pthread.h>/<windows.h>. The .cpp static_asserts that
// the real types fit.
struct mutex
{
    alignas(16) u8 native[96]{};
};

struct cond_var
{
    alignas(16) u8 native[96]{};
};

// Start a thread running func(arg) immediately. Returns false on failure. The thread object must
// outlive the running thread - it holds the handle needed to join.
bool start_thread(thread *t, const thread_desc &desc, thread_func func, void *arg);

// Block until the thread's func returns, then release the handle.
void join_thread(thread *t);

void init_mutex(mutex *m);
void terminate_mutex(mutex *m);
void lock_mutex(mutex *m);
// Returns true if the lock was acquired, false if it was already held by someone else.
bool try_lock_mutex(mutex *m);
void unlock_mutex(mutex *m);

void init_cond_var(cond_var *c);
void terminate_cond_var(cond_var *c);
// Atomically unlock m and block until the cond var is signalled, then re-lock m before returning.
// Must be called with m already locked, and the predicate re-checked in a loop on return (wakeups
// can be spurious).
void wait_cond_var(cond_var *c, mutex *m);
void signal_cond_var(cond_var *c);
void broadcast_cond_var(cond_var *c);

// Set the name the OS/debugger shows for the calling thread. Can only be done from the thread
// itself - that's the only form macos supports. Linux caps names at 15 chars, so longer names get
// truncated. Threads started with start_thread() get named from thread_desc::name automatically.
void set_current_thread_name(const char *name);

u32 current_thread_idx(); // INVALID_IDX on threads we didn't create
mem_arena_group *current_thread_arenas();
mem_arena *current_thread_free_list();
mem_arena *current_thread_stack();
mem_arena *current_thread_scratch_flinear();
void register_current_thread(u32 idx, mem_arena_group *thread_arenas); // called once, at thread entry

} // namespace nslib
