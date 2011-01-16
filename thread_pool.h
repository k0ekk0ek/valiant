#ifndef _THREAD_POOL_H_INCLUDED_
#define _THREAD_POOL_H_INCLUDED_

#include <stdbool.h>
#include <time.h>


#define THREAD_POOL_SUCCESS (0)
#define THREAD_POOL_DEPLETED (1)
#define THREAD_POOL_ERROR (2)


typedef struct thread_pool_task_struct thread_pool_task_t;

struct thread_pool_task_struct {
  void *arg;
  thread_pool_task_t *next;
};


//typedef int(*CHECK_CHECK_FN)(check_t *, request_t *, score_t *);
typedef void (*start_routine)(void *);
typedef struct thread_pool_struct thread_pool_t;

struct thread_pool_struct {
  const char *name;
  bool dead;
  int max_threads;
  int num_threads;
  int max_idle_threads;
  int num_idle_threads;
  int max_tasks;
  int num_tasks;

  struct timespec wait;
  pthread_mutex_t *lock;
  pthread_mutex_t *signal_lock;
  pthread_cond_t *signal;

  thread_pool_task_t *first_task;
  thread_pool_task_t *last_task;

  start_routine function; /* function to execute for every task */
};

// XXX: thread_pool_create could be a lot simpeler, by using defaults and
//      and creating functions that allow to tweak settings!
//thread_pool_t *thread_pool_create (int, int, int, double, void *);

thread_pool_t *thread_pool_create (const char *, int, void *);
void thread_pool_destroy (thread_pool_t *);
int thread_pool_task_push (thread_pool_t *, void *);

#endif
