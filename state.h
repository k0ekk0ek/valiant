#ifndef VT_STATE_H_INCLUDED
#define VT_STATE_H_INCLUDED 1

/* system includes */
#include <pthread.h>

/* valiant includes */
#include "error.h"

typedef enum _vt_state_attr vt_state_attr_t;

enum _vt_state_attr {
  VT_STATE_ATTR_MAX_ERRORS,
  VT_STATE_ATTR_TIME_FRAME,
  VT_STATE_ATTR_BACK_OFF_TIME,
  VT_STATE_ATTR_MAX_BACK_OFF_TIME
};

typedef struct _vt_state vt_state_t;

struct _vt_state {
  time_t start;
  time_t back_off;
  int time_frame; /* time frame that errors should be recorded in */
  int back_off_secs;
  int max_back_off_secs;
  int errors; /* number of errors within time frame */
  int max_errors;
  int power; /* number of times state has been reported as in error */
  int max_power; /* maximum power. if reached, MAX_BACK_OFF_TIME is used */
  pthread_rwlock_t *lock;
};

int vt_state_init (vt_state_t *, int, vt_error_t *);
int vt_state_deinit (vt_state_t *, vt_error_t *);
int vt_state_get_attr (vt_state_t *, vt_state_attr_t);
void vt_state_set_attr (vt_state_t *, vt_state_attr_t, unsigned int);
int vt_state_omit (vt_state_t *);
void vt_state_error (vt_state_t *);

#define vt_state_get_max_errors(p) \
  (vt_state_get_attr ((p), VT_STATE_ATTR_MAX_ERRORS))
#define vt_state_get_time_frame(p) \
  (vt_state_get_attr ((p), VT_STATE_ATTR_TIME_FRAME))
#define vt_state_get_back_off_time(p) \
  (vt_state_get_attr ((p), VT_STATE_ATTR_BACK_OFF_TIME))
#define vt_state_get_max_back_off_time(p) \
  (vt_state_get_attr ((p), VT_STATE_ATTR_MAX_BACK_OFF_TIME))
#define vt_state_set_max_errors(p,n) \
  (vt_state_set_attr ((p), VT_STATE_ATTR_MAX_ERRORS, (n)))
#define vt_state_set_time_frame(p,n) \
  (vt_state_set_attr ((p), VT_STATE_ATTR_TIME_FRAME, (n)))
#define vt_state_set_back_off_time(p,n) \
  (vt_state_set_attr ((p), VT_STATE_ATTR_BACK_OFF_TIME, (n)))
#define vt_state_set_max_back_off_time(p,n) \
  (vt_state_set_attr ((p), VT_STATE_ATTR_MAX_BACK_OFF_TIME, (n)))

#endif
