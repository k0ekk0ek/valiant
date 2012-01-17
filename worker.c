/* system includes */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "context.h"
#include "dict.h"
#include "error.h"
#include "request.h"
#include "thread_pool.h"
#include "worker.h"

typedef struct _vt_worker_store vt_worker_store_t;

struct _vt_worker_store {
  int *dicts;
  int ndicts;
  vt_request_t *request;
  vt_result_t *result;
};

static pthread_key_t vt_worker_key;
static pthread_once_t vt_worker_init_done = PTHREAD_ONCE_INIT;

/* prototypes */
void vt_worker_resp (int, const char *);
void vt_worker_init (void);
void vt_worker_deinit (void *);

void
vt_worker_resp (int conn, const char *resp)
{
  size_t i, n;
  ssize_t nwn;

  for (i = 0, n = strlen (resp); i < n; ) {
    nwn = write (conn, resp+i, n-i);

    if (nwn < (n-i) && errno != EINVAL) {
      vt_error ("%s (%d): write: %s", __func__, __LINE__, strerror (errno));
      break;
    }

    if (nwn)
      i += nwn;
  }

  for (; write (conn, "\n", 1) == -1 && errno == EINTR; )
    ;

  (void)close (conn);
}

void
vt_worker_init (void)
{
  int ret;
  vt_worker_store_t *store;

  pthread_key_create (&vt_worker_key, vt_worker_deinit);

  if (! (store = calloc (1, sizeof (vt_worker_store_t))))
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
  if (store && (ret = pthread_setspecific (vt_worker_key, store)) != 0)
    vt_error ("%s: pthread_setspecific: %s", __func__, strerror (ret));
}

void
vt_worker_deinit (void *arg)
{
  vt_worker_store_t *store;

  if ((store  = (vt_worker_store_t *)arg)) {
    if (store->dicts)
      free (store->dicts);
    vt_request_destroy (store->request);
    (void)vt_result_destroy (store->result, NULL);
    free (store);
  }
}

void
vt_worker (void *data, void *user_data)
{
  int conn, pos, ret, run;
  int ndicts, dictno, *dicts;
  int checkno, depno, stageno;
  int i, n;
  float score = 0;
  vt_check_t *check;
  vt_context_t *ctx;
  vt_error_t err;
  vt_request_t *req;
  vt_result_t *res;
  vt_stage_t *stage;
  vt_worker_store_t *store;

  assert (data);
  assert (user_data);

  conn = *((int *)data);
  ctx = (vt_context_t *)user_data;

  if ((ret = pthread_once (&vt_worker_init_done, vt_worker_init)) != 0)
    vt_fatal ("%s: pthread_once: %s", __func__, strerror (ret));
  if (! (store = pthread_getspecific (vt_worker_key)))
    vt_fatal ("%s: pthread_getspecific: data unavailable", __func__);

  if (! store->request && ! (store->request = vt_request_create (&err)) ||
      ! store->result && ! (store->result = vt_result_create (ctx->ndicts, &err)))
  {
    vt_worker_resp (conn, ctx->error_resp);
    return;
  }

  /* it's impossible to check more dicts per iteration than the maximum number
     of dicts configured */
  if (! store->dicts) {
    store->ndicts = ctx->ndicts;
    if (! (store->dicts = calloc (ctx->ndicts, sizeof (int)))) {
      vt_error ("%s: calloc: %s", __func__, strerror (errno));
      return;
    }
  }

  dicts = store->dicts;
  req = store->request;
  res = store->result;

  if ((vt_request_parse (req, conn, &err))) {
    char *fmt =
    "helo_name=%s, sender=%s, sender_domain=%s, recipient=%s, "
    "recipient_domain=%s, client_address=%s, client_name=%s, "
    "reverse_client_name=%s";
    vt_debug (fmt,
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_HELO_NAME),
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_SENDER),
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_SENDER_DOMAIN),
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_RECIPIENT),
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_RECIPIENT_DOMAIN),
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_CLIENT_ADDRESS),
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_CLIENT_NAME),
      vt_request_mbrbyid (req, VT_REQUEST_MEMBER_REV_CLIENT_NAME)
    );
  } else {
    vt_worker_resp (conn, ctx->error_resp);
    return;
  }

  for (stageno = -1; stageno < ctx->nstages; stageno++) {
    memset (store->dicts, 0, store->ndicts * sizeof (int));
    ndicts = 0;

    /* evaluate checks in current stage */
    if (stageno >= 0) {
      run = 1;
      stage = ctx->stages[stageno];
      /* don't evaluate stage if dependencies failed */
      for (depno = 0; run && depno < stage->ndepends; depno++) {
        pos = stage->depends[depno];

        if (! res->results[pos]->ready)
          vt_panic ("%s: dict %s not ready",
            __func__, ctx->dicts[pos]->name);
        if (res->results[pos]->points)
          run = 1;
      }

      if (run) {
        for (checkno = 0; checkno < stage->nchecks; checkno++) {
          check = stage->checks[checkno];
          run = check->ndepends ? 0 : 1;

          for (depno = 0; ! run && depno < check->ndepends; depno++) {
            pos = check->depends[depno];

            if (! res->results[pos]->ready)
              vt_panic ("%s: dict %s not ready",
                __func__, ctx->dicts[pos]->name);
            if (res->results[pos]->points)
              run = 1;
          }

          pos = check->dict;

          for (dictno = 0; run && dictno < ndicts; dictno++) {
            if (dicts[dictno] == pos)
              run = 0;
          }

          if (run) {
            if (res->results[pos]->ready)
              score += res->results[pos]->points;
            else
              dicts[ndicts++] = pos;
          }
        }
      }
    }

    /* evaluate checks on which checks in next stage depend */
    if ((stageno + 1) < ctx->nstages) {
      stage = ctx->stages[(stageno + 1)];

      for (checkno = 0; checkno < stage->nchecks; checkno++) {
        check = stage->checks[checkno];

        for (depno = 0; depno < check->ndepends; depno++) {
          pos = check->depends[depno];

          for (dictno = 0, run = 1; run && dictno < ndicts; dictno++) {
            if (dicts[dictno] == pos)
              run = 0;
          }
          if (run && ! res->results[pos]->ready)
            dicts[ndicts++] = pos;
        }
      }
    }

    /* evaluate checks for which results were not available */
    for (dictno = 0; dictno < ndicts; dictno++) {
      pos = dicts[dictno];
      vt_dict_check (ctx->dicts[pos], req, res, pos, &err);
    }

    vt_result_wait (res);

    /* evaluate scores */
    if (stageno >= 0) {
      stage = ctx->stages[stageno];

      for (checkno = 0; checkno < stage->nchecks; checkno++) {
        check = stage->checks[checkno];
        pos = check->dict;

        for (dictno = 0; dictno < ndicts; dictno++) {
          if (pos == dicts[dictno]) {
            score += res->results[pos]->points;
            break;
          }
        }
      }
    }
  }

  // FIXME: update statistics
  vt_debug ("%s:%d: score: %f", __func__, __LINE__, score);

  if (ctx->block_threshold && score >= ctx->block_threshold)
    vt_worker_resp (conn, ctx->block_resp);
  else if (ctx->delay_threshold && score >= ctx->delay_threshold)
    vt_worker_resp (conn, ctx->delay_resp);
  else
    vt_worker_resp (conn, ctx->allow_resp);
  vt_result_reset (res);
  free (data);
}
