#ifndef VT_RBL_H_INCLUDED
#define VT_RBL_H_INCLUDED 1

/* system includes */
#include <arpa/inet.h>
#include <pthread.h>
#include <spf2/spf.h>
#include <spf2/spf_dns.h>
#include <time.h>

/* valiant includes */
#include "check.h"
#include "slist.h"
#include "thread_pool.h"

extern const cfg_opt_t vt_rbl_type_opts[];
extern const cfg_opt_t vt_rbl_check_in_opts[];
extern const cfg_opt_t vt_rbl_check_opts[];

typedef struct _vt_rbl_arg vt_rbl_arg_t;

struct _vt_rbl_arg {
  vt_check_t *check;
  vt_request_t *request;
  vt_score_t *score;
};

typedef struct _vt_rbl vt_rbl_t;

struct _vt_rbl {
  char *zone;
  time_t start;
  time_t back_off;
  vt_slist_t *weights;
  int errors; /* number of errors within time frame */
  int power; /* number of times dnsbl has been disabled */
  int max_power; /* maximum power. if reached, MAX_BACK_OFF_TIME is used */
  pthread_rwlock_t lock;
};

vt_rbl_t *vt_rbl_create (cfg_t *sec, vt_error_t *);
int vt_rbl_destroy (vt_rbl_t *, vt_error_t *);
int vt_rbl_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_thread_pool_t *, vt_error_t *);
void vt_rbl_worker (const vt_rbl_arg_t *, const SPF_server_t *, const char *);
int vt_rbl_skip (vt_rbl_t *);
float vt_rbl_max_weight (vt_rbl_t *);
float vt_rbl_min_weight (vt_rbl_t *);

#endif
