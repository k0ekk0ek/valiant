#ifndef VT_WORKER_H_INCLUDED
#define VT_WORKER_H_INCLUDED

#include "context.h"
#include "stats.h"

typedef struct _vt_worker_arg vt_worker_arg_t;

struct _vt_worker_arg {
  vt_context_t *context;
  vt_stats_t *stats;
};

void vt_worker (void *, void *);

#endif
