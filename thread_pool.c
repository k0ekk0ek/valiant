/* system includes */
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
void *vt_thread_pool_task_shift (vt_thread_pool_t *);
void vt_thread_pool_time (struct timespec *, struct timespec *);

#define DEFAULT_MAX_THREADS (100)
#define DEFAULT_MAX_IDLE_THREADS (15)
#define DEFAULT_MAX_TASKS (200)

vt_thread_pool_t *
vt_thread_pool_create (const char *name, unsigned int threads, void *func,
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
  if (name && ! (pool->name = strdup (name))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
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
  // IMPLEMENT... we do have to wait for all tasks to finish...
  // it's probably best to accept an argument that specifies whether to wait or
  // not!
  return 0;
}

void *
thread_pool_worker (void *thread_pool)
{
  vt_thread_pool_t *pool;
  void *arg;
  struct timespec wait;
  int ret, timed;
  void *res;

  if (! (pool = thread_pool))
    return;

  while (! pool->dead) {
    if ((ret = pthread_mutex_lock (&pool->lock)) != 0)
      vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));

    if ((arg = vt_thread_pool_task_shift (pool))) {
      pool->num_idle_threads--;

      if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
        vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

      pool->function (arg);

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

      if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
        vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

      if (timed)
        ret = pthread_cond_timedwait (&pool->signal, &pool->lock, &wait);
      else
        ret = pthread_cond_wait (&pool->signal, &pool->lock);

      if (ret == ETIMEDOUT)
        break;
      if (ret)
        vt_panic ("%s: pthread_cond_wait: %s", __func__, strerror (ret));
    }
  }

  return NULL;
}

int
vt_thread_pool_task_push (vt_thread_pool_t *pool, void *arg, vt_error_t *err)
{
  char *fmt;
  int ret, res = 0;
  int signal = 0;
  pthread_t worker;
  vt_slist_t *task;

  if ((ret = pthread_mutex_lock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  if (THREADS_DEPLETED (pool) && QUEUE_FULL (pool)) {
    vt_set_error (err, VT_ERR_QFULL);
    vt_error ("%s: %s: queue full", __func__, pool->name);
    return -1;
  }
  if (! (task = vt_slist_alloc ())) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: vt_slist_alloc: %s", __func__, strerror (errno));
    return -1;
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
    signal = 1;
  } else if (! THREADS_DEPLETED (pool)) {
    ret = pthread_create (&worker, NULL, &thread_pool_worker, (void *)pool);
    if (ret != 0) {
      fmt = "%s: pthread_create: %s";
      if (ret != EAGAIN)
        vt_fatal (fmt, __func__, strerror (ret));
      vt_set_error (err, VT_ERR_AGAIN);
      vt_error (fmt, __func__, strerror (ret));
      goto UNLOCK;
    }

    pool->num_threads++;
    pool->num_idle_threads++;
    res = 1;
    signal = 1;
  }

UNLOCK:
  if ((ret = pthread_mutex_unlock (&pool->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
  if (signal && (ret = pthread_cond_signal (&pool->signal)) != 0)
    vt_panic ("%s: pthread_cond_signal: %s", __func__, strerror (ret));

  return res == 1 ? 0 : -1;
}

void *
vt_thread_pool_task_shift (vt_thread_pool_t *pool)
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

#undef QUEUE_FULL
#undef THREADS_DEPLETED
