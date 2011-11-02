#ifndef VT_MAP_H_INCLUDED
#define VT_MAP_H_INCLUDED

/* system includes */
#include <confuse.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>

#define VT_MAP_RESP_EMPTY (0)
#define VT_MAP_RESP_FOUND (1)
#define VT_MAP_RESP_NOT_FOUND (2)

typedef enum vt_map_result_enum vt_map_result_t;

enum vt_map_result_enum {
  VT_MAP_RESULT_EMPTY,
  VT_MAP_RESULT_DUNNO,
  VT_MAP_RESULT_ACCEPT,
  VT_MAP_RESULT_REJECT
};

typedef struct vt_map_struct vt_map_t;
typedef int(*VT_MAP_SEARCH_FUNC)(vt_map_result_t *, const vt_map_t *,
  const char *, size_t);
typedef int(*VT_MAP_DESTROY_FUNC)(vt_map_t *);

struct vt_map_struct {
  char *name;
  int member;
  void *data;
  VT_MAP_SEARCH_FUNC search_func;
  VT_MAP_DESTROY_FUNC destroy_func;
};

typedef unsigned int vt_map_id_t;
typedef struct vt_map_list_struct vt_map_list_t;

struct vt_map_list_struct {
  vt_map_t **maps;
  unsigned int nmaps;
  pthread_rwlock_t lock;
};

typedef struct vt_map_type_struct vt_map_type_t;
typedef int(*VT_MAP_TYPE_CREATE_MAP_FUNC)(vt_map_t **, const cfg_t *);

struct vt_map_type_struct {
  char *name;
  VT_MAP_TYPE_CREATE_MAP_FUNC create_map_func;
};

/* DO NOT USE, use create_map_func member */
int vt_map_create (vt_map_t **, size_t, const char *cfg_t);
/* DO NOT USE, use destroy_func member */
int vt_map_destroy (vt_map_t *);
int vt_map_ids_create (vt_map_id_t **, const vt_map_list_t *, const cfg_t *);
int vt_map_list_create (vt_map_list_t **);
int vt_map_list_destroy (vt_map_list_t *, bool);
int vt_map_list_cache_reset (const vt_map_list_t *);
int vt_map_list_set_map (vt_map_id_t *, vt_map_list_t *, const vt_map_t *);
int vt_map_list_get_map_id (vt_map_id_t *, const vt_map_list_t *,
  const char *name);
int vt_map_list_search (vt_map_result_t *, const vt_map_list_t *,
  const vt_map_id_t, const vt_request_t *);
int vt_map_list_evaluate (vt_map_result_t *, const vt_map_list_t *,
  const vt_map_id_t *, const vt_request_t *request);
int vt_map_list_lock (vt_map_list_t *);
int vt_map_list_unlock (vt_map_list_t *);

#endif
