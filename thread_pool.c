/* system includes */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* valiant includes */
#include "consts.h"
#include "thread_pool.h"
#include "utils.h"

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
vt_thread_pool_create (const char *name, int threads, void *func)
{
  vt_thread_pool_t *pool;

  if (! (pool = malloc0 (sizeof (vt_thread_pool_t))))
    return NULL;

  if (pthread_mutex_init (&pool->signal_lock, NULL)) {
    goto failure;
  }

  if (pthread_mutex_init (&pool->lock, NULL)) {
    pthread_mutex_destroy (&pool->signal_lock);
    goto failure;
  }

  if (pthread_cond_init (&pool->signal, NULL)) {
    pthread_mutex_destroy (&pool->lock);
    pthread_mutex_destroy (&pool->signal_lock);
    goto failure;
  }

  /* sanitize settings */
  if (threads < 0)
    threads = 0; /* unlimited */

  pool->dead = false;
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

failure:
  free (pool);
  return NULL;
}

#undef DEFAULT_MAX_THREADS
#undef DEFAULT_MAX_IDLE_THREADS
#undef DEFAULT_MAX_TASKS

void
vt_thread_pool_destroy (vt_thread_pool_t *pool)
{
  // IMPLEMENT... we do have to wait for all tasks to finish...
  // it's probably best to accept an argument that specifies whether to wait or
  // not!
  return; // VT_SUCCESS;
}

void *
thread_pool_worker (void *thread_pool)
{
  vt_thread_pool_t *pool;
  void *param;
  struct timespec wait;
  int ret;
  void *res;

  if (! (pool = thread_pool))
    return;

  while (! pool->dead) {

    if (pthread_mutex_lock (&pool->lock))
      vt_panic ("%s: %s: pthread_mutex_lock: %s", __func__, pool->name,
        strerror (errno));

    if ((param = vt_thread_pool_task_shift (pool))) {
      pool->num_idle_threads--;

      if (pthread_mutex_unlock (&pool->lock))
        vt_panic ("%s: %s: pthread_mutex_unlock: %s", __func__, pool->name,
          strerror (errno));

      pool->function (param);

    } else {
      pool->num_idle_threads++;

      if (! pool->max_idle_threads ||
            pool->max_idle_threads > pool->num_idle_threads)
      {
        if (pthread_mutex_unlock (&pool->lock))
          vt_panic ("%s: %s: pthread_mutex_unlock: %s", __func__, pool->name,
            strerror (errno));

        ret = pthread_cond_wait (&pool->signal, &pool->signal_lock);

      } else {
        vt_thread_pool_time (&pool->wait, &wait);

        if (pthread_mutex_unlock (&pool->lock))
          vt_panic ("%s: %s: pthread_mutex_unlock: %s", __func__, pool->name,
            strerror (errno));

        ret = pthread_cond_timedwait (&pool->signal, &pool->signal_lock, &wait);
      }

      if (ret == ETIMEDOUT)
        break;
      if (ret)
        vt_panic ("%s: %s: pthread_cond_wait: %s", __func__, pool->name,
          strerror (errno));
    }
  }

  return NULL;
}

int
vt_thread_pool_task_push (vt_thread_pool_t *pool, void *param)
{
  bool signal;
  pthread_t worker;
  vt_slist_t *task;

  if (pthread_mutex_lock (&pool->lock))
    return VT_ERR_SYS;
  if (THREADS_DEPLETED (pool) && QUEUE_FULL (pool))
    return VT_ERR_QFULL;
  if ((task = vt_slist_alloc ()) == NULL)
    return VT_ERR_NOMEM;

  task->data = param;
  if (pool->first_task) {
    pool->last_task->next = task;
    pool->last_task = task;
  } else {
    pool->first_task = task;
    pool->last_task = task;
  }

  pool->num_tasks++;

  if (pool->num_idle_threads > 0) {
    signal = true;
  } else if (! THREADS_DEPLETED (pool)) {
    if (pthread_create (&worker, NULL, &thread_pool_worker, (void *)pool))
      return VT_ERR_SYS;

    pool->num_threads++;
    pool->num_idle_threads++;

    signal = true;
  }

  if (pthread_mutex_unlock (&pool->lock))
    return VT_ERR_SYS;
  if (signal && pthread_cond_signal (&pool->signal))
    return VT_ERR_SYS;

  return VT_SUCCESS;
}

void *
vt_thread_pool_task_shift (vt_thread_pool_t *pool)
{
  vt_slist_t *task;
  void *param = NULL;

  task = pool->first_task;

  if (task) {
    param = task->data;

    if (task->next) {
      pool->first_task = task->next;
    } else {
      pool->first_task = NULL;
      pool->last_task = NULL;
    }

    pool->num_tasks--;

    free (task);

  }

  return param;
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
