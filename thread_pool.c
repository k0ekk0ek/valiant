/* system includes */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* valiant includes */
#include "error.h"
#include "thread_pool.h"

#define QUEUE_FULL(p) ((p)->max_tasks && (p)->max_tasks < (p)->num_tasks)
#define THREADS_DEPLETED(p) ((p)->num_idle_threads < 1 && (p)->max_threads \
                         && (p)->max_threads < (p)->num_threads)

/* prototypes */
void *vt_thread_pool_shift (vt_thread_pool_t *);
void vt_thread_pool_time (struct timespec *, struct timespec *);

#define DEFAULT_MAX_THREADS (100)
#define DEFAULT_MAX_IDLE_THREADS (15)
#define DEFAULT_MAX_TASKS (200)

vt_thread_pool_t *
vt_thread_pool_create (void *user_data, unsigned int threads, void *func,
  vt_error_t *err)
{
  char *fmt;
  int ret;
  vt_thread_pool_t *pool;

  if (! (pool = calloc (1, sizeof (vt_thread_pool_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto FAILURE_MUTEX_INIT;
  }
  if ((ret = pthread_mutex_init (&pool->lock, NULL)) != 0) {
    fmt = "%s: pthread_mutex_init: %s";
    if (ret != ENOMEM)
      vt_fatal (fmt, __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error (fmt, __func__, strerror (ret));
    goto FAILURE_MUTEX_INIT;
  }
  if ((ret = pthread_cond_init (&pool->signal, NULL)) != 0) {
    fmt = "%s: pthread_cond_init: %s";
    if (ret != ENOMEM)
      vt_fatal (fmt, __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error (fmt, __func__, strerror (ret));
    goto FAILURE_COND_INIT;
  }

  pool->user_data = user_data;
  pool->dead = 0;
  pool->max_threads = threads;
  pool->num_threads = 0;
  pool->max_idle_threads = DEFAULT_MAX_IDLE_THREADS;
  pool->num_idle_threads = 0;
  pool->max_tasks = DEFAULT_MAX_TASKS;
  pool->num_tasks = 0;
  pool->first_task = NULL;
  pool->last_task = NULL;
  pool->function = func;

  return pool;
FAILURE_COND_INIT:
  (void)pthread_mutex_destroy (&pool->lock);
FAILURE_MUTEX_INIT:
  free (pool);
  return NULL;
}

#undef DEFAULT_MAX_THREADS
#undef DEFAULT_MAX_IDLE_THREADS
#undef DEFAULT_MAX_TASKS

int
vt_thread_pool_destroy (vt_thread_pool_t *pool, vt_error_t *err)
{
  int lock, loop, ret;
  struct timespec wait;

  for (lock = 1, loop = 1; loop;) {
    if (lock && (ret = pthread_mutex_lock (&(pool->lock))) != 0)
      vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));

    pool->dead = 1;

    if (pool->num_idle_threads > 0) {
      if ((ret = pthread_cond_broadcast (&pool->signal)) != 0)
        vt_panic ("%s: pthread_cond_broadcast: %s", __func__, strerror (ret));
      if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
        vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
      lock = 1;

    } else if (pool->num_threads > 0 || pool->num_tasks > 0) {
      /* new tasks are not accepted, but we need to make sure that existing
         tasks finish correctly to avoid memory problems and the like */
      if ((ret = clock_gettime (CLOCK_REALTIME, &wait)) == 0) {
        wait.tv_sec += 2; /* magic number, BAD! */
        wait.tv_nsec += 0;
      }

      if ((ret = pthread_cond_timedwait (&pool->signal, &pool->lock, &wait)) != 0) {
        if (ret != ETIMEDOUT)
          vt_panic ("%s: pthread_cond_timed_wait: %s", __func__, strerror (ret));
        /* do nothing */
      }

      lock = 0;

    } else {
      vt_debug ("%s:%d: no more worker threads, no more tasks",
        __func__, __LINE__);
      if ((ret = pthread_mutex_unlock (&(pool->lock))) != 0)
        vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
      loop = 0;
    }
  }

  if ((ret = pthread_cond_destroy (&pool->signal)) != 0)
    vt_panic ("%s: pthread_cond_destroy: %s", __func__, strerror (ret));
  if ((ret = pthread_mutex_destroy (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_destroy: %s", __func__, strerror (ret));

  free (pool);

  return 0;
}

void *
thread_pool_worker (void *thread_pool)
{
  char *func;
  vt_thread_pool_t *pool;
  void *arg;
  struct timespec wait;
  int ret, timed;
  void *res;
  int lock;

  if (! (pool = thread_pool))
    return NULL;

  lock = 1;

  //while (! pool->dead) {
  for (;;) {
    if (lock && (ret = pthread_mutex_lock (&pool->lock)) != 0)
      vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
    if ((arg = vt_thread_pool_shift (pool))) {
      pool->num_idle_threads--;

      if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
        vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

      pool->function (arg, pool->user_data);
      lock = 1;
    } else if (pool->dead) {
      if (pool->num_threads > 0)
        pool->num_threads--;
      if (pool->num_idle_threads > 0)
        pool->num_idle_threads--;
      if ((ret = pthread_mutex_unlock (&pool->lock)))
        vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
      break;
    } else {
      pool->num_idle_threads++;

      if (! pool->max_idle_threads ||
            pool->max_idle_threads > pool->num_idle_threads)
      {
        timed = 0;
      } else {
        timed = 1;
        vt_thread_pool_time (&pool->wait, &wait);
      }

      if (timed) {
        func = "pthread_cond_timedwait";
        ret = pthread_cond_timedwait (&pool->signal, &pool->lock, &wait);
      } else {
        func = "pthread_cond_wait";
        ret = pthread_cond_wait (&pool->signal, &pool->lock);
      }

      lock = 0;
      if (ret) {
        if (ret != ETIMEDOUT)
          vt_panic ("%s: %s: %s", __func__, func, strerror (ret));
        /* just a timeout, terminate self */
        if (pool->num_threads > 0)
          pool->num_threads--;
        if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
          vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
        break;
      }
    }
  }

  return NULL;
}

int
vt_thread_pool_push (vt_thread_pool_t *pool, void *arg, vt_error_t *err)
{
  char *fmt;
  int ret, res = 0;
  int signal = 0;
  pthread_t worker;
  vt_slist_t *task;

  if ((ret = pthread_mutex_lock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  if (pool->dead) {
    vt_set_error (err, VT_ERR_QFULL);
    vt_error ("%s: thread pool dead", __func__);
    goto unlock;
  }
  if (THREADS_DEPLETED (pool) && QUEUE_FULL (pool)) {
    vt_set_error (err, VT_ERR_QFULL);
    vt_error ("%s: queue full", __func__);
    goto unlock;
  }

  if (! (task = vt_slist_alloc ())) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: vt_slist_alloc: %s", __func__, strerror (errno));
    goto unlock;
  }

  task->data = arg;
  if (pool->first_task) {
    pool->last_task->next = task;
    pool->last_task = task;
  } else {
    pool->first_task = task;
    pool->last_task = task;
  }

  pool->num_tasks++;

  if (pool->num_idle_threads > 0) {
    res = 1;
    signal = 1;
  } else if (! THREADS_DEPLETED (pool)) {
    ret = pthread_create (&worker, NULL, &thread_pool_worker, (void *)pool);
    if (ret != 0) {
      fmt = "%s: pthread_create: %s";
      if (ret != EAGAIN)
        vt_fatal (fmt, __func__, strerror (ret));
      vt_set_error (err, VT_ERR_AGAIN);
      vt_error (fmt, __func__, strerror (ret));
      goto unlock;
    }

    pool->num_threads++;
    pool->num_idle_threads++;
    res = 1;
    signal = 1;
  }

unlock:
vt_error ("%s:%d: unlock, res: %d", __func__, __LINE__, res);
  if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
vt_error ("%s:%d: unlocked, res: %d", __func__, __LINE__, res);
  if (signal && (ret = pthread_cond_signal (&pool->signal)) != 0)
    vt_panic ("%s: pthread_cond_signal: %s", __func__, strerror (ret));

  return res == 1 ? 0 : -1;
}

void *
vt_thread_pool_shift (vt_thread_pool_t *pool)
{
  vt_slist_t *task;
  void *arg = NULL;

  task = pool->first_task;

  if (task) {
    arg = task->data;

    if (task->next) {
      pool->first_task = task->next;
    } else {
      pool->first_task = NULL;
      pool->last_task = NULL;
    }

    pool->num_tasks--;
    free (task);
  }

  return arg;
}

void
vt_thread_pool_time (struct timespec *wait, struct timespec *tp)
{
  int ret;

  if ((ret = clock_gettime (CLOCK_REALTIME, tp)) == 0) {
    tp->tv_sec += wait->tv_sec;
    tp->tv_nsec += wait->tv_nsec;
  }

  return;
}

void
vt_thread_pool_set_max_threads (vt_thread_pool_t *pool, unsigned int num)
{
  int ret;

  assert (pool);

  if ((ret = pthread_mutex_lock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  pool->max_threads = num;
  if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
}

void
vt_thread_pool_set_max_idle_threads (vt_thread_pool_t *pool, unsigned int num)
{
  int ret;

  assert (pool);

  if ((ret = pthread_mutex_lock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  pool->max_idle_threads = num;
  if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
}

void
vt_thread_pool_set_max_queued (vt_thread_pool_t *pool, unsigned int num)
{
  int ret;

  assert (pool);

  if ((ret = pthread_mutex_lock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  pool->max_tasks = num;
  if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
}

#undef QUEUE_FULL
#undef THREADS_DEPLETED
