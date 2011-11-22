#ifndef VT_CONTEXT_H_INCLUDED
#define VT_CONTEXT_H_INCLUDED 1

/* valiant includes */
#include "check.h"
#include "slist.h"
#include "stats.h"

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
  float block_threshold;
  char *block_resp;
  float delay_threshold;
  char *delay_resp;
  char *error_resp; /* what to respond with, in case we encounter an error */

  vt_slist_t *stages;
  vt_stats_t *stats;
  vt_map_list_t *maps;
};

vt_context_t *vt_context_create (vt_context_t *, cfg_t *, vt_error_t *);
int vt_context_destroy (vt_context_t *, vt_error_t *);

#endif
