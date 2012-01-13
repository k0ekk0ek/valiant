#ifndef VT_CONTEXT_H_INCLUDED
#define VT_CONTEXT_H_INCLUDED 1

/* valiant includes */
#include "dict.h"
#include "slist.h"

typedef struct _vt_check vt_check_t;

struct _vt_check {
  int dict;
  int *depends;
  int ndepends;
};

typedef struct _vt_stage vt_stage_t;

struct _vt_stage {
  vt_check_t **checks;
  int nchecks;
  int *depends;
  int ndepends;
  float max_diff; /* maximum weight gained by evaluating */
  float min_diff; /* minimum weight gained by evaluating */
};

typedef struct _vt_context vt_context_t;

struct _vt_context {
  int uid;
  int gid;
  char *bind_address; /* address to listen on */
  char *port; /* port to listen on */
  char *pid_file;
  char *syslog_ident;
  int syslog_facility;
  int syslog_prio;
  char *allow_resp;
  int block_threshold;
  char *block_resp;
  int delay_threshold;
  char *delay_resp;
  char *error_resp;
  int max_threads;
  int max_idle_threads;
  int max_tasks;

  vt_dict_t **dicts;
  int ndicts;

  vt_stage_t **stages;
  int nstages;
};

vt_context_t *vt_context_create (vt_dict_type_t **, cfg_t *, vt_error_t *);
int vt_context_destroy (vt_context_t *, vt_error_t *);
//int vt_context_add_dict (vt_context_t *, vt_dict_t *, vt_error_t *);
//int vt_context_get_dict_pos (vt_context_t *, const char *);

#endif
