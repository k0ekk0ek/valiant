#ifndef VT_WORKER_H_INCLUDED
#define VT_WORKER_H_INCLUDED 1

/* system includes */
/* valiant includes */

typedef _vt_arg vt_arg_t;

struct _vt_arg {
  int fd;
  vt_context_t *ctx;
};

void vt_worker (void *);

#endif
