#ifndef THREAD_H_C3F0CA05839339
#define THREAD_H_C3F0CA05839339

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

#include <vlib/std.h>
#include <vlib/time.h>
#include <vlib/vector.h>

/* Thread */

typedef pthread_t thread_t;

data(ThreadOptions) {
  size_t    stack_size;
  bool      start_detached;
};

thread_t  thread_spawn_opts(void* (*run)(void* arg), void* arg, ThreadOptions* options);
static inline thread_t thread_spawn(void* (*run)(void*), void* arg) {
  return thread_spawn_opts(run, arg, NULL);
}

thread_t  thread_self();

void      thread_detach(thread_t thread);
void*     thread_join(thread_t thread);

/* Lock */

data(Lock) {
  pthread_mutex_t _mutex[1];
};

void      thread_lock_init(Lock* self);
void      thread_lock_close(Lock* self);
void      thread_lock(void* self);
void      thread_unlock(void* self);

void      thread_withlock(Lock* self, void (*callback)());

/* Cond */

data(Cond) {
  Lock lock[1];
  pthread_cond_t _cond[1];
};

void      thread_cond_init(Cond* self);
void      thread_cond_close(Cond* self);
bool      thread_wait(Cond* self, Duration timeout);  // Returns false if the operation times out
void      thread_signal(Cond* self);
void      thread_broadcast(Cond* self);

/* ThreadPool */

struct ThreadPool;

// A PoolWorker is tied to a thread and runs jobs.
interface(PoolWorker) {
  void* (*create_env)(void* self);
  void  (*close_env)(void* self, void* env);
  void  (*work)(void* self, void* env, void* job);
  void  (*close)(void* self);
};

// A PoolManager is responsible for governing how many threads are active at any time.
interface(PoolManager) {
  // Returns a negative number if a thread should be terminated, a positive number if a new
  // thread should be spawned, or zero if no threads need to be spawned or terminated.
  int   (*decide)(void* self, int idle, int total);
  void  (*close)(void* self);
};

PoolManager*    poolmanager_new_basic(unsigned min_idle, unsigned max_idle, unsigned max_total);

data(ThreadPool) {
  Cond          cond[1];
  PoolManager*  manager;
  PoolWorker*   worker;
  unsigned      total_threads;
  Vector        idle[1];
};

void  threadpool_init(ThreadPool* self, PoolManager* manager, PoolWorker* worker);
void  threadpool_close(ThreadPool* self);

// Waits for an idle thread to become available and then dispatches the job.
// Returns false if the timeout is reached. Use -1 for no timeout.
bool  threadpool_dispatch(ThreadPool* self, void* job, Duration timeout);

#endif /* THREAD_H_C3F0CA05839339 */

