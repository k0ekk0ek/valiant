#ifndef VT_DICT_H_INCLUDED
#define VT_DICT_H_INCLUDED 1

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "error.h"
#include "request.h"
#include "result.h"

typedef struct _vt_dict vt_dict_t;

typedef int(*VT_DICT_CHECK_FUNC)(vt_dict_t *, vt_request_t *, vt_result_t *,
  vt_error_t *);
typedef int(*VT_DICT_DESTROY_FUNC)(vt_dict_t *, vt_error_t *);

struct _vt_dict {
  char *name;
  int async;
  int pos; /* position in vt_result_t */
  void *data;
  float max_diff; /* maximum weight gained by evaluating */
  float min_diff; /* minimum weight gained by evaluating */
  VT_DICT_CHECK_FUNC check_func;
  VT_DICT_DESTROY_FUNC destroy_func;
};

typedef struct _vt_dict_type vt_dict_type_t;

typedef vt_dict_t*(*VT_DICT_TYPE_CREATE_FUNC)(vt_dict_type_t *, cfg_t *,
  cfg_t *, vt_error_t *);

struct _vt_dict_type {
  char *name;
  VT_DICT_TYPE_CREATE_FUNC create_func;
};

#define vt_dict_destroy(dict,err) ((dict)->destroy_func ((dict),(err)))
#define vt_dict_check(dict,req,res,err) \
  ((dict)->check_func ((dict),(req),(res),(err)))

vt_dict_t *vt_dict_create (vt_dict_type_t *, cfg_t *, cfg_t *, vt_error_t *);
int vt_dict_dynamic_pattern (const char *);
char *vt_dict_unescape_pattern (const char *);

#endif
