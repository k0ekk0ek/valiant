#ifndef VT_RESULT_H_INCLUDED
#define VT_RESULT_H_INCLUDED

/* system includes */
#include <pthread.h>

/* valiant includes */
#include "error.h"

typedef struct _vt_dict_result vt_dict_result_t;

struct _vt_dict_result {
  int ready;
  float points;
};

typedef struct _vt_result vt_result_t;

struct _vt_result {
  vt_dict_result_t **results;
  unsigned int nresults;
  unsigned int writers;
  pthread_mutex_t lock;
  pthread_cond_t signal;
};

vt_result_t *vt_result_create (unsigned int, vt_error_t *);
int vt_result_destroy (vt_result_t *, vt_error_t *);
void vt_result_lock (vt_result_t *);
void vt_result_unlock (vt_result_t *);
void vt_result_wait (vt_result_t *);
void vt_result_update (vt_result_t *, unsigned int, float);
void vt_result_reset (vt_result_t *);

#endif
