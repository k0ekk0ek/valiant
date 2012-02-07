/* system includes */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "result.h"

vt_result_t *
vt_result_create (unsigned int num, vt_error_t *err)
{
  int i;
  int ret;
  vt_result_t *res;

  if (! (res = calloc (1, sizeof (vt_result_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure_mutex;
  }

  res->nresults = num;
  if (! (res->results = calloc (num + 1, sizeof (vt_dict_result_t *)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure_mutex;
  }

  for (i = 0; i < num; i++) {
    if (! (res->results[i] = calloc (1, sizeof (vt_dict_result_t)))) {
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: calloc: %s", __func__, strerror (errno));
      goto failure_mutex;
    }
  }

  switch ((ret = pthread_mutex_init (&res->lock, NULL))) {
    case 0:
      break;
    case ENOMEM:
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: pthread_mutex_init: %s", __func__, strerror (ret));
      goto failure_mutex;
    default:
      vt_fatal ("%s: pthread_mutex_init: %s", __func__, strerror (ret));
  }

  switch ((ret = pthread_cond_init (&res->signal, NULL))) {
    case 0:
      break;
    case ENOMEM:
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: pthread_cond_init: %s", __func__, strerror (ret));
      goto failure_cond;
    default:
      vt_fatal ("%s: pthread_cond_init: %s", __func__, strerror (ret));
  }

  return res;
failure_cond:
  pthread_mutex_destroy (&res->lock);
failure_mutex:
  if (res) {
    if (res->results) {
      for (i = 0; i < num; i++)
        free (res->results[i]);
      free (res->results);
    }
    free (res);
  }
  return NULL;
}

int
vt_result_destroy (vt_result_t *res, vt_error_t *err)
{
  int i, ret;

  if (res) {
    if (res->results) {
      for (i = 0; i < res->nresults; i++) {
        if (res->results[i])
          free (res->results[i]);
      }
      free (res->results);
    }

    if ((ret = pthread_mutex_destroy (&res->lock)) != 0)
      vt_fatal ("%s: pthread_mutex_destroy: %s", __func__, strerror (ret));
    if ((ret = pthread_cond_destroy (&res->signal)) != 0)
      vt_fatal ("%s: pthread_cond_destroy: %s", __func__, strerror (ret));
    free (res);
  }

  return 0;
}

void
vt_result_lock (vt_result_t *res)
{
  int ret;

  assert (res);

  if ((ret = pthread_mutex_lock (&res->lock)) != 0)
    vt_fatal ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));

  res->writers++;

  if ((ret = pthread_mutex_unlock (&res->lock)) != 0)
    vt_fatal ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
}


void
vt_result_unlock (vt_result_t *res)
{
  int ret, signal = 0;

  if ((ret = pthread_mutex_lock (&res->lock)) != 0)
    vt_fatal ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));

  if (res->writers > 0)
    res->writers--;
  if (res->writers < 1)
    signal = 1;

  if ((ret = pthread_mutex_unlock (&res->lock)) != 0)
    vt_fatal ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
  if (signal != 0 && (ret = pthread_cond_signal (&res->signal)) != 0)
    vt_fatal ("%s: pthread_cond_signal: %s", __func__, strerror (ret));
}

void
vt_result_wait (vt_result_t *res)
{
  int ret;

  assert (res);

  if ((ret = pthread_mutex_lock (&res->lock)) != 0)
    vt_fatal ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));

  while (res->writers > 0) {
    if ((ret = pthread_cond_wait (&res->signal, &res->lock)) != 0)
      vt_fatal ("%s: pthread_cond_wait: %s", __func__, strerror (ret));
  }

  if ((ret = pthread_mutex_unlock (&res->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
}

void
vt_result_update (vt_result_t *res, unsigned int pos, float points)
{
  if (pos >= 0 && res->nresults > pos) {
    (res->results[pos])->ready = 1;
    (res->results[pos])->points = points;
  }
}

void
vt_result_reset (vt_result_t *res)
{
  int i;

  assert (res);

  if (res->results) {
    for (i = 0; i < res->nresults; i++) {
      if (res->results[i]) {
        res->results[i]->ready = 0;
        res->results[i]->points = 0.0;
      }
    }
  }
}
