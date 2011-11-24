#ifndef VT_SCORE_H_INCLUDED
#define VT_SCORE_H_INCLUDED

/* system includes */
#include <pthread.h>

typedef struct vt_score_struct vt_score_t;

struct vt_score_struct {
  int *cntrs;
  unsigned int ncntrs;
  int points;
  unsigned int writers;
  pthread_mutex_t lock;
  pthread_cond_t signal;
};

vt_score_t *vt_score_create (vt_error_t *);
void vt_score_destroy (vt_score_t *);
void vt_score_lock (vt_score_t *);
void vt_score_unlock (vt_score_t *);
void vt_score_update (vt_score_t *, unsigned int, int);
void vt_score_wait (vt_score_t *);

#endif

