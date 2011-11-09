#ifndef VT_STATS_H_INCLUDED
#define VT_STATS_H_INCLUDED

/* system includes */
#include <pthread.h>
#include <time.h>

/* valiant includes */
#include "score.h"



/*
Defines the API and the control structure for keeping statistics...

postfwd's statistics look like this:
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Counters: 10498009 seconds uptime since Tue, 05 Jul 2011 19:22:57 CEST
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Requests: 0 overall, 0 last interval, 0.00% cache hits, 0.00% rate hits
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Averages: 0.00 overall, 0.00 last interval, 0.00 top
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Contents: 25 rules, 76 cached requests, 1778 cached dns results, 9 rate
limits
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: CLNT01   matched: 20263 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: CLNT02   matched: 27 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: CLNT03   matched: 725972 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: CLNT04   matched: 135040 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: GREY_001   matched: 27 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: GREY_003   matched: 35752 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: GREY_004   matched: 522 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: GREY_005   matched: 252958 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: GREY_008   matched: 104426 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: RATE_001   matched: 396102 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: RATE_002   matched: 60535 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: RATE_003   matched: 712 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: RBL01   matched: 1293983 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: REJE_001   matched: 111 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: REJE_002   matched: 260577 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: REJE_004   matched: 279796 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: REJE_005   matched: 5 times
Nov  4 06:29:46 mx1 postfwd[2531]: [STATS] Rule ID: RHSBL01   matched: 1293983 times

// there's a few things I need to think about, when reporting statistics...
// I really want to support things that make sense... for example, why am I
// using a different statistics structure, I think it makes sense just to keep
// the counter in the check itself?!?!

// while it makes sense to keep statistics in the check that would require a
// lot of locking... let's not do that!!!!

// what about atomic operations?!?!, I really need to figure out how they work
// but since I only want to implement something that increases a counter it
// might be a good idea to use them
// eventually it might be a good idea to use this in 

// we probably need a watchdog thread to log statistics at given intervals

*/

typedef struct vt_stats_cntr_struct vt_stats_cntr_t;

struct vt_stats_cntr_t {
  char *name;
  size_t len;
  unsigned long hits;
};

typedef struct vt_stats_struct vt_stats_t;

struct vt_stats_struct {
  time_t ctime; /* creation time */
  time_t mtime; /* modification time */
  time_t cycle;
  unsigned long reqs;
  vt_stat_cntr_t *cntrs;
  unsigned int ncntrs;
  pthread_mutex_t lock;
  pthread_cond_t signal;
};

int vt_stats_create (vt_stats_t **);
int vt_stats_destroy (vt_stats_t *);
int vt_stats_add_cntr (unsigned int *, vt_stats_t *, const char *);
int vt_stats_get_cntr_id (unsigned int *, const vt_stats_t *, const char *);
void vt_stats_update (vt_stats_t *, const vt_score_t *);
void vt_stats_print (vt_stats_t *);
int vt_stats_lock (vt_stats_t *);
int vt_stats_unlock (vt_stats_t *);
int vt_stats_print_cycle (vt_stats_t *, time_t);
void vt_stats_printer (vt_stats_t *);

#endif
