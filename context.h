#ifndef VT_CONTEXT_H_INCLUDED
#define VT_CONTEXT_H_INCLUDED 1

/* valiant includes */
#include "slist.h"
#include "stats.h"

typedef struct _vt_check vt_check_t;

struct _vt_check {
  int dict; /* dict position in global list */
  int *depends; /* dict positions in global list that are required to be non-zero */
  int ndepends;
  vt_check_t *checks;
  int nchecks;
}

typedef struct _vt_context vt_context_t;

struct _vt_context {
  int uid;
  int gid;
  char *bind_address; /* address to listen on */
  char *port; /* port to listen on */
  char *pid_file;
  char *syslog_ident;
  int syslog_facility;
  /* NOTE: prio could be bitmask so logging can be more granular, but let's safe
     that for a later version. */
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
  unsigned int ndicts;

  vt_check_t **checks;
  unsigned int nchecks;
};

vt_context_t *vt_context_create (vt_dict_type_t **, cfg_t *, );
int vt_context_destroy (vt_context_t *, vt_error_t *);
//int vt_context_add_dict (vt_context_t *, vt_dict_t *, vt_error_t *);
//int vt_context_get_dict_pos (vt_context_t *, const char *);

#endif
