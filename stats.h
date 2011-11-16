#ifndef VT_STATS_H_INCLUDED
#define VT_STATS_H_INCLUDED

/* system includes */
#include <pthread.h>
#include <time.h>

/* valiant includes */
#include "score.h"

typedef struct vt_stats_cntr_struct vt_stats_cntr_t;

struct vt_stats_cntr_struct {
  char *name;
  size_t len;
  unsigned long hits;
};

typedef struct vt_stats_struct vt_stats_t;

struct vt_stats_struct {
  time_t ctime; /* creation time */
  time_t mtime; /* modification time */
  time_t cycle;
  unsigned long nreqs;
  vt_stats_cntr_t **cntrs;
  unsigned int ncntrs;
  pthread_mutex_t lock;
  pthread_cond_t signal;
};

//int vt_stats_create (vt_stats_t **);
int vt_stats_destroy (vt_stats_t *);
int vt_stats_add_cntr (unsigned int *, vt_stats_t *, const char *);
int vt_stats_get_cntr_id (unsigned int *, const vt_stats_t *, const char *);
void vt_stats_update (vt_stats_t *, const vt_score_t *);
int vt_stats_print (vt_stats_t *);
int vt_stats_lock (vt_stats_t *);
int vt_stats_unlock (vt_stats_t *);
int vt_stats_print_cycle (vt_stats_t *, time_t);
void vt_stats_printer (vt_stats_t *);

#endif
