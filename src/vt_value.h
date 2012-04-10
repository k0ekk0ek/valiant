#ifndef VT_VALUE_H_INCLUDED
#define VT_VALUE_H_INCLUDED

/* system includes */
#include <stdbool.h>
#include <stdarg.h>

typedef enum _vt_value_type vt_value_type_t;

enum _vt_value_type {
  VT_VALUE_TYPE_NONE = 0,
  VT_VALUE_TYPE_BOOL,
  VT_VALUE_TYPE_CHAR,
  VT_VALUE_TYPE_INT,
  VT_VALUE_TYPE_FLOAT,
  VT_VALUE_TYPE_STR
};

typedef struct _vt_value vt_value_t;

struct _vt_value {
  vt_value_type_t type;
  union {
    bool bln;
    int chr;
    long lng;
    double dbl;
    char *str;
  } data;
};

vt_value_t *vt_value_create (vt_value_type_t, ...);
void vt_value_destroy (vt_value_t *);
int vt_value_cmp (vt_value_t *, vt_value_t *, int *);
vt_value_t *vt_value_dup (vt_value_t *);

#endif

