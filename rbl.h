#ifndef VT_RBL_H_INCLUDED
#define VT_RBL_H_INCLUDED

/* system includes */
#include <arpa/inet.h>
#include <confuse.h>
#include <pthread.h>
#include <spf2/spf.h>
#include <spf2/spf_dns.h>
#include <time.h>

/* valiant includes */
#include "check.h"
#include "request.h"
#include "score.h"
#include "slist.h"
#include "stats.h"
#include "thread_pool.h"

extern const cfg_opt_t vt_rbl_type_opts[];
extern const cfg_opt_t vt_rbl_check_in_opts[];
extern const cfg_opt_t vt_rbl_check_opts[];

typedef struct vt_rbl_param_struct vt_rbl_param_t;

struct vt_rbl_param_struct {
  vt_check_t *check;
  vt_request_t *request;
  vt_score_t *score;
  vt_stats_t *stats;
};

typedef struct vt_rbl_struct vt_rbl_t;

struct vt_rbl_struct {
  char *zone;
  time_t start;
  time_t back_off;
  vt_slist_t *weights;
  int errors; /* number of errors within time frame */
  int power; /* number of times dnsbl has been disabled */
  int max_power; /* maximum power. if reached, MAX_BACK_OFF_TIME is used */
  pthread_rwlock_t *lock;
};

int vt_rbl_create (vt_rbl_t **, const cfg_t *sec);
int vt_rbl_check (vt_check_t *, vt_request_t *, vt_score_t *, vt_stats_t *,
  vt_thread_pool_t *);
void vt_rbl_worker (const vt_rbl_param_t *, const SPF_server_t *, const char *);
int vt_rbl_skip (int *, vt_rbl_t *);

#endif
