#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h" /* needed for panic */
#include "thread_pool.h"


#define QUEUE_FULL(p) ((p)->max_tasks && (p)->max_tasks < (p)->num_tasks)
#define THREADS_DEPLETED(p) ((p)->num_idle_threads < 1 && (p)->max_threads \
                         && (p)->max_threads < (p)->num_threads)


thread_pool_task_t *
thread_pool_task_create (void *arg)
{
  thread_pool_task_t *task;

  if ((task = malloc (sizeof (thread_pool_task_t)))) {
    task->arg = arg;
    task->next = NULL;
  }

  /* errno set by malloc */

  return task;
}





/*
 * function should only be used internally!
 * btw... locking is done by the worker! 
 */
void *
thread_pool_task_shift (thread_pool_t *pool)
{
  thread_pool_task_t *task;
  void *arg;

  task = pool->first_task;

  if (task) {
    arg = task->arg;

    if (task->next) {
      pool->first_task = task->next;
    } else {
      pool->first_task = NULL;
      pool->last_task = NULL;
    }

    pool->num_tasks--;

    free (task);

  } else {
    arg = NULL;
  }

  return arg;
}


void
thread_pool_time (struct timespec *wait, struct timespec *tp)
{
  int rv;

  // XXX: implement assertions

  if ((rv = clock_gettime (CLOCK_REALTIME, tp)) == 0) {
    // XXX: check if we have sane values...
    // XXX: if more than enough nano seconds incread number of sec etc...
    tp->tv_sec += wait->tv_sec;
    tp->tv_nsec += wait->tv_nsec;
  }

  return;
}


// and why exactly do we return a void pointer?
//{
// we can't bail and report back status so we need an error reporting
// function...
//}

void *
thread_pool_worker (void *thread_pool)
{
  thread_pool_t *pool;
  void *arg;
  struct timespec wait;
  int ret;
  void *res;

  if (! (pool = thread_pool))
    return;

  while (! pool->dead) {

    if (pthread_mutex_lock (pool->lock))
      panic ("%s: %s: pthread_mutex_lock: %s", __func__, pool->name, strerror (errno));

    arg = thread_pool_task_shift (pool);


    if (arg) {
      pool->num_idle_threads--;

      if (pthread_mutex_unlock (pool->lock))
        panic ("%s: %s: pthread_mutex_unlock: %s", __func__, pool->name, strerror (errno));

      pool->function (arg);

    } else {
      pool->num_idle_threads++;

      if (! pool->max_idle_threads
       ||   pool->max_idle_threads > pool->num_idle_threads)
      {
        if (pthread_mutex_unlock (pool->lock))
          panic ("%s: %s: pthread_mutex_unlock: %s", __func__, pool->name, strerror (errno));

        ret = pthread_cond_wait (pool->signal, pool->signal_lock);

      } else {
        thread_pool_time (&pool->wait, &wait);

        if (pthread_mutex_unlock (pool->lock))
          panic ("%s: %s: pthread_mutex_unlock: %s", __func__, pool->name, strerror (errno));

        ret = pthread_cond_timedwait (pool->signal, pool->signal_lock, &wait);
      }

      if (ret == ETIMEDOUT)
        break;
      if (ret)
        panic ("%s: %s: pthread_cond_wait: %s", __func__, pool->name, strerror (errno));
    }
  }

  return NULL;
}


/**
 * This function should definitely return something more than a void! a lot of
 * things can go wrong!
 */
int
thread_pool_task_push (thread_pool_t *pool, void *arg)
{
  bool signal;
  pthread_t worker;
  thread_pool_task_t *task;

  if (pthread_mutex_lock (pool->lock))
    return THREAD_POOL_ERROR;
  if (THREADS_DEPLETED (pool) && QUEUE_FULL (pool))
    return THREAD_POOL_DEPLETED;
  if ((task = thread_pool_task_create (arg)) == NULL)
    return THREAD_POOL_ERROR;

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
    if (pthread_create (&worker, NULL, (void *) &thread_pool_worker, (void *) pool))
      return THREAD_POOL_ERROR;

    pool->num_threads++;
    pool->num_idle_threads++;

    signal = true;
  }

  if (pthread_mutex_unlock (pool->lock))
    return THREAD_POOL_ERROR;
  if (signal && pthread_cond_signal (pool->signal))
    return THREAD_POOL_ERROR;

  return THREAD_POOL_SUCCESS;
}


#define DEFAULT_MAX_THREADS (100)
#define DEFAULT_MAX_IDLE_THREADS (15)
#define DEFAULT_MAX_TASKS (200)

thread_pool_t *
thread_pool_create (const char *name, int threads, void *func)
{
  thread_pool_t *pool;

  if (! (pool = malloc (sizeof (thread_pool_t)))) {
    return NULL;
  }

  memset (pool, 0, sizeof (thread_pool_t));

pool->signal_lock = malloc (sizeof (pthread_mutex_t));
pool->lock = malloc (sizeof (pthread_mutex_t));
pool->signal = malloc (sizeof (pthread_cond_t));

  if (pthread_mutex_init (pool->signal_lock, NULL)) {
    goto error;
  }

printf ("here!\n");

  if (pthread_mutex_init (pool->lock, NULL)) {
    pthread_mutex_destroy (pool->signal_lock);
    goto error;
  }

  if (pthread_cond_init (pool->signal, NULL)) {
    pthread_mutex_destroy (pool->lock);
    pthread_mutex_destroy (pool->signal_lock);
    goto error;
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

error:
  free (pool);
  return NULL;
}























