#ifndef VT_THREAD_POOL_H_INCLUDED
#define VT_THREAD_POOL_H_INCLUDED

/* system includes */
#include <stdbool.h>
#include <time.h>

/* valiant includes */
#include "slist.h"

typedef struct vt_thread_pool_task_struct vt_thread_pool_task_t;

struct vt_thread_pool_task_struct {
  void *arg;
  vt_thread_pool_task_t *next;
};

typedef void (*start_routine)(void *);
typedef struct vt_thread_pool_struct vt_thread_pool_t;

struct vt_thread_pool_struct {
  const char *name;
  bool dead;
  int max_threads;
  int num_threads;
  int max_idle_threads;
  int num_idle_threads;
  int max_tasks;
  int num_tasks;

  struct timespec wait;
  pthread_mutex_t lock;
  pthread_mutex_t signal_lock;
  pthread_cond_t signal;

  vt_slist_t *first_task;
  vt_slist_t *last_task;

  start_routine function; /* function to execute for every task */
};

vt_thread_pool_t *vt_thread_pool_create (const char *, int, void *);
void vt_thread_pool_destroy (vt_thread_pool_t *);
int vt_thread_pool_task_push (vt_thread_pool_t *, void *);

#endif
