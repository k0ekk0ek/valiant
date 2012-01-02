#ifndef VT_CHECK_H_INCLUDED
#define VT_CHECK_H_INCLUDED 1

/* valiant includes */
#include "dict.h"

typedef struct _vt_check vt_check_t;

struct _vt_check {
  int pos;
  int *requires;
};

vt_check_t *vt_check_create (int, vt_error_t *);
int vt_check_destroy (vt_check_t *, vt_error_t *);

#endif
