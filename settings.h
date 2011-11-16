#ifndef VT_SETTINGS_H_INCLUDED
#define VT_SETTINGS_H_INCLUDED

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "check.h"
#include "map.h"
#include "stats.h"

/* defaults */
#define VT_BIND_ADDR "localhost"
#define VT_PORT "10025"
#define VT_LOG_IDENT "valiant"
#define VT_LOG_FACILITY "mail"
#define VT_LOG_PRIO "info"
#define VT_GREYLIST_RESP "421 Questionable reputation, please try again later"
#define VT_GREYLIST_THRESHOLD (20)
#define VT_BLOCK_RESP "521 Bad reputation"
#define VT_BLOCK_THRESHOLD (50)
#define VT_ERROR_RESP "451 Server error, please try again later"

typedef struct {
  char *bind_address; /* what address to listen on */
  char *port; /* what port to listen on */
  char *pid_file;
  char *log_ident;
  int log_facility;
  /* XXX prio could be bitmask logging could be more granular, but let's safe
     that for a later version. */
  int log_prio;
  int greylist_threshold;
  char *greylist_resp;
  int block_threshold;
  char *block_resp;
  char *error_resp; /* what to respond with, in case we encounter an error */

  vt_slist_t *stages; /* stage->checks, stage->checks */
  vt_stats_t *stats; /* statistics */

  vt_map_list_t **maps;
  unsigned int nmaps;
} vt_context_t;

int vt_context_init (vt_context_t *, cfg_t *);

//int vt_context_deinit (vt_
//int vt_context_create (vt_context_t **, cfg_t *);
//void vt_context_free (vt_context_t *);

unsigned int vt_cfg_size_opts (cfg_t *);
cfg_opt_t *vt_cfg_getnopt (cfg_t *, unsigned int);
unsigned int vt_cfg_opt_parsed (cfg_opt_t *);
cfg_t *vt_cfg_parse (const char *);

#endif
