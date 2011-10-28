#ifndef VT_MAP_H_INCLUDED
#define VT_MAP_H_INCLUDED

/* system includes */
#include <confuse.h>
#include <sys/types.h>

#define VT_MAP_RESP_EMPTY (0)
#define VT_MAP_RESP_FOUND (1)
#define VT_MAP_RESP_NOT_FOUND (2)

typedef struct vt_map_struct vt_map_t;

typedef int(*VT_MAP_LOOKUP_FUNC)(int *, const vt_map_t *, const char *, size_t);
typedef int(*VT_MAP_DESTROY_FUNC)(vt_map_t *);

struct vt_map_struct {
  const char *name;
  int attrib;
  void *context;
  VT_MAP_LOOKUP_FUNC lookup_func;
  VT_MAP_DESTROY_FUNC destroy_func;
};

typedef struct vt_map_type_struct vt_map_type_t;

typedef int(*VT_MAP_TYPE_CREATE_MAP_FUNC)(vt_map_t **, const cfg_t *);

struct vt_map_type_struct {
  const char *name;
  const cfg_opt_t *opts;
  VT_MAP_TYPE_CREATE_MAP_FUNC create_map_func;
};

int vt_map_create (vt_map_t **, const char *, const char *, size_t);
int vt_map_destroy (vt_map_t *);

#endif
