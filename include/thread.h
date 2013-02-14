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

void      thread_init_lock(Lock* self);
void      thread_close_lock(Lock* self);
void      thread_lock(void* self);
void      thread_unlock(void* self);

void      thread_withlock(Lock* self, void (*callback)());

/* Cond */

data(Cond) {
  Lock lock[1];
  pthread_cond_t _cond[1];
};

void      thread_init_cond(Cond* self);
void      thread_close_cond(Cond* self);
bool      thread_wait(Cond* self, Duration timeout);  // Returns false if the operation times out
void      thread_signal(Cond* self);
void      thread_broadcast(Cond* self);

/* ThreadPool */

struct ThreadPool;

// A PoolWorker is tied to a thread and runs jobs.
interface(PoolWorker) {
  void  (*work)(void* self, void* arg);
  void  (*close)(void* self);
};

// A PoolFactory is a thread-safe interface that creates PoolWorkers when new threads are spawned.
interface(PoolFactory) {
  PoolWorker* (*create_worker)(void* self);
  void  (*close)(void* self);
};

interface(Runnable) {
  void  (*run)(void* self);
};

// A PoolFactory that treats its arguments as Runnables and executes them.
extern PoolFactory  poolfactory_runnable[1];
// A PoolFactory that treats its arguments as function pointers of type void (*)() and calls them.
extern PoolFactory  poolfactory_function[1];

// A PoolManager is responsible for governing how many threads are active at any time.
interface(PoolManager) {
  // Returns the number of threads to spawn when the ThreadPool is created.
  int   (*initial_threads)(void* self);
  // Returns true if a new thread should be spawned.
  bool  (*thread_needed)(void* self, struct ThreadPool* pool);
  // Returns true if an idle thread should be terminated.
  bool  (*thread_idle)(void* self, struct ThreadPool* pool);
  void  (*close)(void* self);
};

PoolManager*    poolmanager_new_basic(unsigned min_idle, unsigned max_idle, unsigned max_total);

data(ThreadPool) {
  Cond          _cond[1];
  PoolManager*  manager;
  PoolFactory*  factory;
  unsigned      total_threads;
  unsigned      idle_threads;
  Vector        _idle[1];
};

void  threadpool_init(ThreadPool* self, PoolManager* manager, PoolFactory* factory);
void  threadpool_close(ThreadPool* self);

bool  threadpool_dispatch(ThreadPool* self, void* arg, Duration timeout);

#endif /* THREAD_H_C3F0CA05839339 */

