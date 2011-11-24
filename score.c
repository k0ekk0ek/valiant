/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "error.h"
#include "score.h"

vt_score_t *
vt_score_create (unsigned int ncntrs, vt_error_t *err)
{
  int ret;
  vt_score_t *score;

  if (! (score = calloc (1, sizeof (vt_score_t))) ||
      ! (score->cntrs = calloc (ncntrs, sizeof (int))))
  {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, __LINE__, strerror (errno));
    goto FAILURE;
  }

  score->ncntrs = ncntrs;

  switch ((ret = pthread_mutex_init (&score->lock, NULL))) {
    case 0:
      break;
    case ENOMEM:
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s (%d): pthread_mutex_init: %s", __func__, __LINE__, strerror (ret));
      goto FAILURE;
    default:
      vt_fatal ("%s (%d): pthread_mutex_init: %s", __func__, __LINE__, strerror (ret));
  }

  switch ((ret = pthread_cond_init (&score->signal, NULL))) {
    case 0:
      break;
    case ENOMEM:
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s (%d): pthread_cond_init: %s", __func__, __LINE__, strerror (ret));
      goto FAILURE_SIGNAL;
    default:
      vt_fatal ("%s (%d): pthread_cond_init: %s", __func__, __LINE__, strerror (ret));
  }

  return score;
FAILURE_SIGNAL:
  pthread_mutex_destroy (&score->lock);
FAILURE:
  if (score)
    free (score);
  return NULL;
}

void
vt_score_destroy (vt_score_t *score)
{
  int ret;

  if ((ret = pthread_mutex_destroy (&score->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_destroy: %s", __func__, __LINE__, strerror (ret));
  if ((ret = pthread_cond_destroy (&score->signal)) != 0)
    vt_fatal ("%s (%d): pthread_cond_destroy: %s", __func__, __LINE__, strerror (ret));
  free (score);
}

void
vt_score_lock (vt_score_t *score)
{
  int ret;

  if ((ret = pthread_mutex_lock (&score->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_lock: %s", __func__, __LINE__, strerror (ret));

  score->writers++;

  if ((ret = pthread_mutex_unlock (&score->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_unlock: %s", __func__, __LINE__, strerror (ret));
}


void
vt_score_unlock (vt_score_t *score)
{
  int ret, signal=0;

  if ((ret = pthread_mutex_lock (&score->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_lock: %s", __func__, __LINE__, strerror (ret));

  if (score->writers > 0)
    score->writers--;
  if (score->writers < 1)
    signal = 1;

  if ((ret = pthread_mutex_unlock (&score->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_unlock: %s", __func__, __LINE__, strerror (ret));
  if (signal != 0 && (ret = pthread_cond_signal (&score->signal)) != 0)
    vt_fatal ("%s (%d): pthread_cond_signal: %s", __func__, __LINE__, strerror (ret));
}

void
vt_score_update (vt_score_t *score, unsigned int pos, int points)
{
  /* NOTE: Since this function is called many times throughout the application,
     I thought I would optimize it by leaving out the mutexes. If there's
     something utterly wrong with this approach, please notify me. */
  __sync_add_and_fetch (&score->points, points);
  /* NOTE: Since every check has a unique identifier, it shouldn't be necessary
     to protect the results with a memory barrier. Again if this assumption is
     false, please notify me. */
  if (score->ncntrs > 0)
    score->cntrs[pos] = points ? 1 : 0;
}

void
vt_score_reset (vt_score_t *score)
{
  score->points = 0;
  memset (score->cntrs, 0, score->ncntrs * sizeof (int));
}

void
vt_score_wait (vt_score_t *score)
{
  int ret;

  if ((ret = pthread_mutex_lock (&score->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_lock: %s", __func__, __LINE__, strerror (ret));

  while (score->writers > 0) {
    if ((ret = pthread_cond_wait (&score->signal, &score->lock)) != 0)
      vt_fatal ("%s (%d): pthread_cond_wait: %s", __func__, __LINE__, strerror (ret));
  }

  if ((ret = pthread_mutex_unlock (&score->lock)) != 0)
    vt_panic ("%s (%d): pthread_mutex_unlock: %s", __func__, __LINE__, strerror (ret));
}

