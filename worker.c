/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check.h"
#include "context.h"
#include "error.h"
#include "request.h"
#include "stage.h"
#include "worker.h"

/* prototypes */
void vt_worker_resp (int, const char *);
void vt_worker_init (void);
void vt_worker_deinit (void *);

typedef struct _vt_worker_store vt_worker_store_t;

struct _vt_worker_store {
  vt_request_t *request;
  vt_score_t *score;
};

static pthread_key_t vt_worker_key;
static pthread_once_t vt_worker_init_done = PTHREAD_ONCE_INIT;

int
vt_worker_weight_static (float cur, float min_gain, float max_gain,
  float min_bound, float max_bound)
{
  float min = cur + min_gain;
  float max = cur + max_gain;

  if (((!min_bound || cur >= min_bound) && (!max_bound || cur < max_bound)) &&
      ((!min_bound || min >= min_bound) && (!max_bound || min < max_bound)) &&
      ((!min_bound || max >= min_bound) && (!max_bound || max < max_bound)))
    return 1;
  return 0;
}

void
vt_worker_resp (int fd, const char *resp)
{
  size_t i, n;
  ssize_t nwn;

  for (i = 0, n = strlen (resp); i < n; ) {
    nwn = write (fd, resp+i, n-i);

    if (nwn < (n-i) && errno != EINVAL) {
      vt_error ("%s (%d): write: %s", __func__, __LINE__, strerror (errno));
      break;
    }

    if (nwn)
      i += nwn;
  }

  for (; write (fd, "\n", 1) == -1 && errno == EINTR; )
    ;

  (void)close (fd);
}

void
vt_worker_init (void)
{
  int ret;
  vt_worker_store_t *store;

  pthread_key_create (&vt_worker_key, vt_worker_deinit);

  if (! (store = calloc (1, sizeof (vt_worker_store_t))))
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
  if (store && (ret = pthread_setspecific (vt_worker_key, store)) != 0)
    vt_error ("%s (%d): pthread_setspecific: %s", __func__, __LINE__, strerror (ret));
}

void
vt_worker_deinit (void *arg)
{
  vt_worker_store_t *store;
  vt_request_t *request;
  vt_score_t *score;

  if ((store  = (vt_worker_store_t *)arg)) {
    vt_request_destroy (request);
    vt_score_destroy (score);
  }

  free (store);
}

void
vt_worker (void *arg)
{
  float eval;
  //int where;
  //vt_error_t ret;
  int fd;
  int ret;
  vt_check_t *check;
  vt_context_t *ctx;
  vt_error_t err;
  vt_request_t *request; // store in thread specific data
  vt_score_t *score; // store in thread specific data
  vt_slist_t *chk_cur, *stg_cur;
  vt_stage_t *stage;
  vt_worker_store_t *store;

  fd = ((vt_worker_arg_t *)arg)->fd;
  ctx = ((vt_worker_arg_t *)arg)->ctx;

  if ((ret = pthread_once (&vt_worker_init_done, vt_worker_init)) != 0)
    vt_fatal ("%s (%d): pthread_once: %s", __func__, __LINE__, strerror (ret));
  if (! (store = pthread_getspecific (vt_worker_key)))
    vt_fatal ("%s (%d): thread specific data unavailable", __func__, __LINE__);

  vt_map_list_cache_reset (ctx->maps, &err);

  if (! (store->request) && ! (store->request = vt_request_create (&ret)) ||
      ! (store->score) && ! (store->score = vt_score_create (ctx->stats->ncntrs, &ret)))
  {
    vt_worker_resp (fd, ctx->error_resp);
    return;
  }

  request = store->request;
  score = store->score;

  if (! (vt_request_parse (request, fd, &err))) {
    vt_worker_resp (fd, ctx->error_resp);
    return;
  }

  for (stg_cur = ctx->stages; stg_cur; stg_cur = stg_cur->next) {
    stage = (vt_stage_t *)stg_cur->data;

    /* skip entire stage? */
    ret = 0;
    if (! (eval = vt_map_list_evaluate (ctx->maps, stage->maps, request, &ret))) {
      if (ret) {
        vt_worker_resp (fd, ctx->error_resp);
        return;
      }
      //continue; // skip // FIXME: map returns 0 because we have no default
      // yet
    }

    for (chk_cur = stage->checks; chk_cur; chk_cur = chk_cur->next) {
      check = (vt_check_t *)chk_cur->data;

      /* skip check? */
      ret = 0;
      if (! (eval = vt_map_list_evaluate (ctx->maps, stage->maps, request, &ret))) {
        if (ret) {
          vt_worker_resp (fd, ctx->error_resp);
          return;
        }
        //continue; // skip // FIXME: see above
      }

      if (check->check_func (check, request, score, &err) != 0) {
        vt_worker_resp (fd, ctx->error_resp);
        return;
      }
    }

    vt_score_wait (score);

    float cur = score->points;
    float min_gain = cur + stage->min_weight_diff;
    float max_gain = cur + stage->max_weight_diff;
    float min_bound = ctx->delay_threshold;
    float max_bound = ctx->block_threshold;

    if (vt_worker_weight_static (cur, min_gain, max_gain, 0, min_bound)
     || vt_worker_weight_static (cur, min_gain, max_gain, min_bound, max_bound)
     || vt_worker_weight_static (cur, min_gain, max_gain, max_bound, 0))
      break; /* score won't change */
  }
fprintf (stderr, "%s (%d): %d\n", __func__, __LINE__, score->points);
  vt_stats_update (ctx->stats, score);

  if (ctx->block_threshold && score->points >= ctx->block_threshold)
    vt_worker_resp (fd, ctx->block_resp);
  else if (ctx->delay_threshold && score->points >= ctx->delay_threshold)
    vt_worker_resp (fd, ctx->delay_resp);
  else
    vt_worker_resp (fd, ctx->allow_resp);
  vt_score_reset (score);
  free (arg);
}
