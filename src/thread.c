
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

void  thread_lock_init(Lock* self) {
  int r = pthread_mutex_init(self->_mutex, NULL);
  if (r) verr_raise(verr_system(r));
}
void  thread_lock_close(Lock* self) {
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

void  thread_cond_init(Cond* self) {
  thread_lock_init(self->lock);
  if (pthread_cond_init(self->_cond, NULL)) {
    int eno = errno;
    thread_lock_close(self->lock);
    verr_raise(verr_system(eno));
  }
}
void  thread_cond_close(Cond* self) {
  pthread_cond_destroy(self->_cond);
  thread_lock_close(self->lock);
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
  ThreadPool* pool;
  bool        ready;
  void*       next_job;
  char        env[];
};

static void* worker_run(void* self);

// Spawns a new worker in the background
static void spawn_worker(ThreadPool* pool) {
  Worker* worker = malloc(sizeof(Worker) + call(pool->worker, env_size));
  thread_cond_init(worker->cond);
  worker->pool = pool;
  worker->ready = false;
  worker->next_job = NULL;
  call(pool->worker, init_env, worker->env);
  *(Worker**)vector_push(pool->idle) = worker;
  thread_t id = thread_spawn(worker_run, worker);
  thread_detach(id);
  pool->total_threads++;
}
static Worker* wait_for_worker(ThreadPool* pool, Duration timeout) {
  if (timeout < 0) {
    while (pool->idle->size == 0) thread_wait(pool->cond, -1);
  } else {
    Time end = time_add(time_now_monotonic(), timeout);
    while (pool->idle->size == 0) {
      timeout = time_diff(time_now_monotonic(), end);
      if (!thread_wait(pool->cond, MAX(0, timeout))) return NULL;
    }
  }
  Worker* worker = *(Worker**)vector_back(pool->idle);
  vector_pop(pool->idle);
  return worker;
}
static void dispatch_worker(Worker* worker, void* job) {
  thread_lock(worker);
  worker->ready = true;
  worker->next_job = job;
  thread_signal(worker->cond);
  thread_unlock(worker);
}
static int check_threads(ThreadPool* self, bool add_idle, bool add_total) {
  return call(self->manager, decide, self->idle->size + (add_idle ? 1 : 0), self->total_threads + (add_total ? 1 : 0));
}

void threadpool_init(ThreadPool* self, PoolManager* manager, PoolWorker* worker) {
  thread_cond_init(self->cond);
  self->manager = manager;
  self->worker = worker;
  self->total_threads = 0;
  vector_init(self->idle, sizeof(Worker*), 4);
  // Spawn initial threads
  thread_lock(self);
  while (check_threads(self, false, false) > 0) {
    spawn_worker(self);
  }
  thread_unlock(self);
}
void threadpool_close(ThreadPool* self) {
  // Terminate all workers
  thread_lock(self);
  while (self->total_threads > 0) {
    if (self->idle->size > 0) {
      Worker* worker = *(Worker**)vector_back(self->idle);
      vector_pop(self->idle);
      dispatch_worker(worker, NULL);
    } else {
      thread_wait(self->cond, -1);
    }
  }
  thread_unlock(self);
  // Cleanup resources
  thread_cond_close(self->cond);
  vector_close(self->idle);
  call(self->manager, close);
  call(self->worker, close);
}

bool threadpool_dispatch(ThreadPool* self, void* job, Duration timeout) {
  thread_lock(self);
  if (self->idle->size == 0) {
    // Check if we are allowed to spawn a new thread
    if (check_threads(self, false, true) >= 0) spawn_worker(self);
  }
  Worker* worker = wait_for_worker(self, timeout);
  thread_unlock(self);
  if (!worker) return false;
  dispatch_worker(worker, job);
  return true;
}

static void* worker_run(void* _self) {
  verr_thread_init();
  Worker* self = _self;

  for (;;) {

    // Wait for a job
    thread_lock(self);
    while (!self->ready) thread_wait(self->cond, -1);
    thread_unlock(self);
    self->ready = false;
    if (self->next_job == NULL) break;

    // Run the job
    TRY {
      call(self->pool->worker, work, self->env, self->next_job);
    } CATCH(err) {
      fprintf(stderr, "error: %s\n", verr_msg(err));
    } ETRY

    // Add to idle set, or terminate if there are too many threads
    thread_lock(self->pool);
    if (check_threads(self->pool, true, false) >= 0) {
      *(Worker**)vector_push(self->pool->idle) = self;
      thread_signal(self->pool->cond);
      thread_unlock(self->pool);
    } else {
      thread_unlock(self->pool);
      break;
    }

  }

  call(self->pool->worker, close_env, self->env);

  thread_lock(self->pool);
  self->pool->total_threads--;
  thread_signal(self->pool->cond);
  thread_unlock(self->pool);

  thread_cond_close(self->cond);
  free(self);
  verr_thread_cleanup();
  return NULL;
}

/* BasicManager */

data(BasicManager) {
  PoolManager base;
  int         min_idle;
  int         max_idle;
  int         max_total;
};

static int basic_decide(void* _self, int idle, int total) {
  BasicManager* self = _self;
  if (total > self->max_total) return -1;
  if (idle > self->max_idle) return -1;
  if (idle < self->min_idle && total+1 <= self->max_total) return 1;
  return 0;
}

static PoolManager_Impl basic_impl = {
  .decide = basic_decide,
  .close = free,
};

PoolManager* poolmanager_new_basic(unsigned min_idle, unsigned max_idle, unsigned max_total) {
  BasicManager* self = malloc(sizeof(BasicManager));
  self->base._impl = &basic_impl;
  self->min_idle = min_idle;
  self->max_idle = max_idle;
  self->max_total = max_total;
  return &self->base;
}
