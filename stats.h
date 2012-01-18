#ifndef VT_STATS_H_INCLUDED
#define VT_STATS_H_INCLUDED 1

/* system includes */
#include <pthread.h>
#include <time.h>

/* valiant includes */
#include "dict.h"
#include "error.h"
#include "result.h"

typedef struct vt_stats_cntr_struct vt_stats_cntr_t;

struct vt_stats_cntr_struct {
  char *name;
  size_t len;
  unsigned long hits;
};

typedef struct vt_stats_struct vt_stats_t;

struct vt_stats_struct {
  int dead;
  int worker;
  time_t ctime; /* creation time */
  time_t mtime; /* modification time */
  time_t cycle;
  unsigned long nreqs;
  vt_stats_cntr_t *cntrs;
  unsigned int ncntrs;
  pthread_mutex_t lock;
  pthread_cond_t signal;
};

vt_stats_t *vt_stats_create (vt_dict_t **, int, vt_error_t *);
int vt_stats_destroy (vt_stats_t *, vt_error_t *);
//int vt_stats_add_cntr (vt_stats_t *, const char *, vt_error_t *);
//int vt_stats_get_cntr_pos (const vt_stats_t *, const char *);
void vt_stats_update (vt_stats_t *, vt_result_t *);
//int vt_stats_print (vt_stats_t *, vt_error_t *);
// FIXME: IMPLEMENT
//int vt_stats_print_cycle (vt_stats_t *, time_t);
void vt_stats_thread (vt_stats_t *);

#endif
