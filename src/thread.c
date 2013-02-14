
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <vlib/error.h>
#include <vlib/thread.h>
#include <vlib/util.h>

/* Thread */

thread_t  thread_spawn_opts(void* (*run)(void* arg), void* arg, ThreadOptions* options) {
  pthread_attr_t attr_storage;
  pthread_attr_t* attr = NULL;
  if (options) {
    attr = &attr_storage;
    pthread_attr_init(attr);
    pthread_attr_setstacksize(attr, options->stack_size);
    pthread_attr_setdetachstate(attr, options->start_detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
  }
  pthread_t id;
  int r = pthread_create(&id, attr, run, arg);
  if (r) {
    if (attr) pthread_attr_destroy(attr);
    verr_raise(verr_system(r));
  }
  if (attr) pthread_attr_destroy(attr);
  return id;
}
void thread_detach(thread_t id) {
  int r = pthread_detach(id);
  if (r) verr_raise(verr_system(r));
}
void* thread_join(thread_t id) {
  void* retval;
  int r = pthread_join(id, &retval);
  if (r) verr_raise(verr_system(r));
  return retval;
}

thread_t thread_self() {
  return pthread_self();
}

/* Lock */

void  thread_init_lock(Lock* self) {
  int r = pthread_mutex_init(self->_mutex, NULL);
  if (r) verr_raise(verr_system(r));
}
void  thread_close_lock(Lock* self) {
  pthread_mutex_destroy(self->_mutex);
}
void  thread_lock(void* _self) {
  Lock* self = _self;
  int r = pthread_mutex_lock(self->_mutex);
  if (r) verr_raise(verr_system(r));
}
void  thread_unlock(void* _self) {
  Lock* self = _self;
  int r = pthread_mutex_unlock(self->_mutex);
  if (r) verr_raise(verr_system(r));
}

void  thread_withlock(Lock* self, void (*callback)()) {
  thread_lock(self);
  TRY {
    callback();
  } FINALLY {
    thread_unlock(self);
  } ETRY
}

/* Cond */

void  thread_init_cond(Cond* self) {
  thread_init_lock(self->lock);
  if (pthread_cond_init(self->_cond, NULL)) {
    int eno = errno;
    thread_close_lock(self->lock);
    verr_raise(verr_system(eno));
  }
}
void  thread_close_cond(Cond* self) {
  pthread_cond_destroy(self->_cond);
  thread_close_lock(self->lock);
}
bool  thread_wait(Cond* self, Duration timeout) {
  int r;
  if (timeout >= 0) {
    Time abstime = time_add(time_now_monotonic(), timeout);
    struct timespec tp = {
      .tv_sec = abstime.seconds,
      .tv_nsec = abstime.nanos,
    };
    r = pthread_cond_timedwait(self->_cond, self->lock->_mutex, &tp);
  } else {
    r = pthread_cond_wait(self->_cond, self->lock->_mutex);
  }
  if (r) {
    if (r == ETIMEDOUT) return false;
    verr_raise(verr_system(r));
  }
  return true;
}
void  thread_signal(Cond* self) {
  int r = pthread_cond_signal(self->_cond);
  if (r) verr_raise(verr_system(r));
}
void  thread_broadcast(Cond* self) {
  int r = pthread_cond_broadcast(self->_cond);
  if (r) verr_raise(verr_system(r));
}

/* ThreadPool */

data(Worker) {
  Cond        cond[1];
  thread_t    id;
  ThreadPool* pool;
  PoolWorker* worker;
  bool        ready;
  void*       job;
};

static void* worker_run(void* self);

static void threadpool_spawn(ThreadPool* self, void* arg) {
  PoolWorker* pworker = call(self->factory, create_worker);
  Worker* worker = malloc(sizeof(Worker));
  thread_init_cond(worker->cond);
  worker->pool = self;
  worker->worker = pworker;
  worker->ready = (arg != NULL);
  worker->job = arg;

  thread_lock(worker);
  worker->id = thread_spawn(worker_run, worker);
  thread_detach(worker->id);
  thread_unlock(worker);

  self->total_threads++;
  if (!arg) {
    *(Worker**)vector_push(self->_idle) = worker;
    self->idle_threads++;
  }
}
static Worker* threadpool_popidle(ThreadPool* self, Duration timeout) {
  Time end = time_add(time_now_monotonic(), timeout);
  while (self->idle_threads == 0) {
    timeout = time_diff(time_now_monotonic(), end);
    if (!thread_wait(self->_cond, MAX(0, timeout))) return NULL;
  }
  Worker* worker = *(Worker**)vector_back(self->_idle);
  vector_pop(self->_idle);
  self->idle_threads--;
  return worker;
}
static bool threadpool_wakeup(ThreadPool* self, void* arg, Duration timeout) {
  Worker* worker = threadpool_popidle(self, timeout);
  if (!worker) return false;
  thread_lock(worker);
  worker->job = arg;
  worker->ready = true;
  thread_signal(worker->cond);
  thread_unlock(worker);
  return true;
}
static bool threadpool_idle(ThreadPool* self, Worker* worker) {
  self->idle_threads++;
  bool terminate = call(self->manager, thread_idle, self);
  if (terminate) {
    self->idle_threads--;
    self->total_threads--;
    return true;
  }
  *(Worker**)vector_push(self->_idle) = worker;
  thread_signal(self->_cond);
  return false;
}

void  threadpool_init(ThreadPool* self, PoolManager* manager, PoolFactory* factory) {
  thread_init_cond(self->_cond);
  self->manager = manager;
  self->factory = factory;
  self->total_threads = 0;
  self->idle_threads = 0;
  int initial = call(manager, initial_threads);
  vector_init(self->_idle, sizeof(Worker*), initial);
  thread_lock(self);
  for (int i = 0; i < initial; i++) {
    threadpool_spawn(self, NULL);
  }
  thread_unlock(self);
}
void  threadpool_close(ThreadPool* self) {
  thread_lock(self);
  while (self->total_threads > 0) {
    Worker* worker = threadpool_popidle(self, -1);
    thread_lock(worker);
    worker->job = NULL;
    worker->ready = true;
    thread_signal(worker->cond);
    thread_unlock(worker);
    self->total_threads--;
  }
  thread_unlock(self);
}

bool  threadpool_dispatch(ThreadPool* self, void* arg, int64_t timeout_millis) {
  thread_lock(self);
  bool result;
  bool spawn = call(self->manager, thread_needed, self);
  if (self->idle_threads > 0) {
    threadpool_wakeup(self, arg, -1);
    if (spawn) threadpool_spawn(self, NULL);
    result = true;
  } else if (spawn) {
    threadpool_spawn(self, arg);
    result = true;
  } else {
    result = threadpool_wakeup(self, arg, timeout_millis);
  }
  thread_unlock(self);
  return result;
}

static void* worker_run(void* _self) {
  Worker* self = _self;
  verr_thread_init();
  thread_lock(self);
  for (;;) {

    // Wait for work
    while (!self->ready) thread_wait(self->cond, -1);
    if (!self->job) break;

    // Run the job
    self->ready = false;
    void* job = self->job;
    thread_unlock(self);
    TRY {
      call(self->worker, work, job);
    } CATCH(err) {
      // TODO
      fprintf(stderr, "error: %s\n", verr_msg(err));
    } ETRY
    thread_lock(self);

    // Add self to idle threads
    thread_lock(self->pool);
    bool terminate = threadpool_idle(self->pool, self);
    thread_unlock(self->pool);
    if (terminate) break;

  }
  thread_unlock(self);
  verr_thread_cleanup();

  thread_close_cond(self->cond);
  free(self);
  return NULL;
}

/* BasicManager */

data(BasicManager) {
  PoolManager base;
  unsigned    min_idle;
  unsigned    max_idle;
  unsigned    max_total;
};

static int basic_initial_threads(void* _self) {
  BasicManager* self = _self;
  return self->min_idle;
}
static bool basic_thread_needed(void* _self, ThreadPool* pool) {
  BasicManager* self = _self;
  if (pool->total_threads >= self->max_total) return false;
  if (pool->idle_threads <= self->min_idle) return true;
  return false;
}
static bool basic_thread_idle(void* _self, ThreadPool* pool) {
  BasicManager* self = _self;
  if (pool->idle_threads > self->max_idle) return true;
  return false;
}
static PoolManager_Impl basic_manager_impl = {
  .initial_threads = basic_initial_threads,
  .thread_needed = basic_thread_needed,
  .thread_idle = basic_thread_idle,
  .close = free,
};

PoolManager* poolmanager_new_basic(unsigned min_idle, unsigned max_idle, unsigned max_total) {
  BasicManager* self = malloc(sizeof(BasicManager));
  self->base._impl = &basic_manager_impl;
  self->min_idle = min_idle;
  self->max_idle = max_idle;
  self->max_total = max_total;
  return &self->base;
}

/* Built-in PoolFactories */

static void null_close(void* _self) {}

static void runnable_work(void* _self, void* _arg) {
  Runnable* job = _arg;
  call(job, run);
}
static PoolWorker_Impl runnable_worker_impl = {
  .work = runnable_work,
  .close = null_close,
};
static PoolWorker runnable_worker[1] = {{
  ._impl = &runnable_worker_impl,
}};

static PoolWorker* runnable_create_worker(void* _self) {
  return runnable_worker;
}
static PoolFactory_Impl runnable_factory_impl = {
  .create_worker = runnable_create_worker,
  .close = null_close,
};
PoolFactory poolfactory_runnable[1] = {{
  ._impl = &runnable_factory_impl,
}};

static void function_work(void* _self, void* _arg) {
  void (*job)() = _arg;
  job();
}
static PoolWorker_Impl function_worker_impl = {
  .work = function_work,
  .close = null_close,
};
static PoolWorker function_worker[1] = {{
  ._impl = &function_worker_impl,
}};

static PoolWorker* factory_create_worker(void* _self) {
  return function_worker;
}
static PoolFactory_Impl function_factory_impl = {
  .create_worker = factory_create_worker,
  .close = null_close,
};
PoolFactory poolfactory_function[1] = {{
  ._impl = &function_factory_impl,
}};
