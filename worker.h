#ifndef VT_WORKER_H_INCLUDED
#define VT_WORKER_H_INCLUDED 1

/* system includes */
/* valiant includes */

typedef struct _vt_worker_arg vt_worker_arg_t;

struct _vt_worker_arg {
  int fd;
  vt_context_t *ctx;
};

void vt_worker (void *);

#endif
