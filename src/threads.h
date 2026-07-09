#pragma once
#include "basic_types.h"

namespace nslib
{

// Entry point for a thread. The OS thread's return value is ignored on purpose - communicate
// results back through shared state guarded by a mutex/cond_var, not a return value.
using thread_func = void (*)(void *arg);

struct thread
{
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
bool start_thread(thread *t, thread_func func, void *arg);

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

} // namespace nslib
