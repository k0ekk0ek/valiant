#ifndef VT_SET_H_INCLUDED
#define VT_SET_H_INCLUDED

/* system includes */
#include <pthread.h>
#include <sys/types.h>

/* valiant includes */
#include "map.h"
#include "request.h"

typedef struct vt_set_struct vt_set_t;

struct vt_set_struct {
  vt_map_t **maps;
  unsigned int nmaps;
  pthread_rwlock_t lock;
};

int vt_set_create (vt_set_t **);
int vt_set_destroy (vt_set_t *, int);
int vt_set_insert_map (int *, vt_set_t *, const vt_map_t *);
int vt_set_select_map_id (int *, const vt_set_t *, const char *name);
int vt_set_lock (const vt_set_t *);
int vt_set_unlock (const vt_set_t *);
int vt_set_lookup (int *, const vt_set_t *, const int, const vt_request_t *);

#endif
