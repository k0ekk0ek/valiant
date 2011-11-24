#ifndef VT_MAP_H_INCLUDED
#define VT_MAP_H_INCLUDED 1

/* system includes */
#include <confuse.h>
#include <sys/types.h>

/* valiant includes */
#include "error.h"
#include "request.h"

typedef struct _vt_map vt_map_t;
typedef int(*VT_MAP_OPEN_FUNC)(vt_map_t *, vt_error_t *);
typedef int(*VT_MAP_SEARCH_FUNC)(vt_map_t *, const char *, size_t, vt_error_t *);
typedef int(*VT_MAP_CLOSE_FUNC)(vt_map_t *, vt_error_t *);
typedef int(*VT_MAP_DESTROY_FUNC)(vt_map_t *, vt_error_t *);

struct _vt_map {
  char *name;
  vt_request_member_t member;
  void *data;
  VT_MAP_OPEN_FUNC open_func;
  VT_MAP_SEARCH_FUNC search_func;
  VT_MAP_CLOSE_FUNC close_func;
  VT_MAP_DESTROY_FUNC destroy_func;
};

typedef struct _vt_map_list_t vt_map_list_t;

struct _vt_map_list_t {
  vt_map_t **maps;
  unsigned int nmaps;
  pthread_rwlock_t lock;
};

typedef struct _vt_map_type vt_map_type_t;
typedef vt_map_t *(*VT_MAP_TYPE_CREATE_MAP_FUNC)(cfg_t *, vt_error_t *);

struct _vt_map_type {
  char *name;
  VT_MAP_TYPE_CREATE_MAP_FUNC create_map_func;
};

int *vt_map_lineup_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
vt_map_list_t *vt_map_list_create (vt_error_t *);
int vt_map_list_destroy (vt_map_list_t *, int, vt_error_t *);
int vt_map_list_cache_reset (const vt_map_list_t *, vt_error_t *);
int vt_map_list_add_map (vt_map_list_t *, const vt_map_t *, vt_error_t *);
int vt_map_list_get_map_pos (const vt_map_list_t *, const char *);
int vt_map_list_search (const vt_map_list_t *, int, const vt_request_t *,
  vt_error_t *);
int vt_map_list_evaluate (const vt_map_list_t *, const int *,
  const vt_request_t *, vt_error_t *);
void vt_map_list_lock (vt_map_list_t *);
void vt_map_list_unlock (vt_map_list_t *);

#endif
