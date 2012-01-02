#ifndef VT_RBL_H_INCLUDED
#define VT_RBL_H_INCLUDED 1

/* system includes */
#include <arpa/inet.h>
#include <pthread.h>
#include <spf2/spf.h>
#include <spf2/spf_dns.h>
#include <time.h>

/* valiant includes */
#include "dict.h"
#include "slist.h"
#include "state.h"
#include "thread_pool.h"

typedef struct _vt_rbl vt_rbl_t;

struct _vt_rbl {
  SPF_server_t *spf_server;
  char *zone;
  vt_state_t back_off;
  vt_slist_t *weights;
};

vt_rbl_t *vt_rbl_create (cfg_t *sec, vt_error_t *);
int vt_rbl_destroy (vt_rbl_t *, vt_error_t *);
int vt_rbl_check (vt_dict_t *, const char *, vt_result_t *, vt_error_t *);
//int vt_rbl_skip (vt_rbl_t *);
int vt_rbl_max_weight (vt_rbl_t *);
int vt_rbl_min_weight (vt_rbl_t *);

#endif
