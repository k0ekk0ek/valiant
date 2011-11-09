/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "score.h"
#include "utils.h"

vt_score_t *
vt_score_create (void)
{
  int ret;
  vt_score_t *score;

  if ((score = malloc0 (sizeof (vt_score_t))) == NULL) {
    ret = errno;
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    goto failure;
  }

  if ((ret = pthread_mutex_init (&score->lock, NULL)) != 0) {
    vt_error ("%s: pthread_mutex_init: %s", __func__, strerror (ret));
    goto failure;
  }

  if ((ret = pthread_cond_init (&score->signal, NULL)) != 0) {
    vt_error ("%s: pthread_cond_init: %s", __func__, strerror (ret));
    goto failure_signal;
  }

  return score;

failure_signal:
  pthread_mutex_destroy (&score->lock);
failure:
  if (score)
    free (score);

  errno = ret;

  return NULL;
}


int
vt_score_destroy (vt_score_t *score)
{
  int ret;

  if ((ret = pthread_mutex_destroy (&score->lock)) != 0 && ret != EINVAL) {
    vt_error ("%s: pthread_mutex_destroy: %s", __func__, strerror (ret));
    errno = ret;
    return -1;
  }

  if ((ret = pthread_cond_destroy (&score->signal)) != 0 && ret != EINVAL) {
    vt_error ("%s: pthread_cond_destroy: %s", __func__, strerror (ret));
    errno = ret;
    return -1;
  }

  free (score);

  return 0;
}


int
vt_score_lock (vt_score_t *score)
{
  int ret;

  if ((ret = pthread_mutex_lock (&score->lock)) != 0) {
    vt_error ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
    errno = ret;
    return -1;
  }

  score->writers++;

  if ((ret = pthread_mutex_unlock (&score->lock)) != 0) {
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
  }

  return 0;
}


int
vt_score_unlock (vt_score_t *score)
{
  int ret, signal=0;

  if ((ret = pthread_mutex_lock (&score->lock)) != 0) {
    vt_error ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
    errno = ret;
    return -1;
  }

  if (score->writers > 0)
    score->writers--;
  if (score->writers < 1)
    signal = 1;

  if ((ret = pthread_mutex_unlock (&score->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));
  if (signal != 0 && (ret = pthread_cond_signal (&score->signal)) != 0)
    vt_panic ("%s: pthread_cond_signal: %s", __func__, strerror (ret));

  return 0;
}

int
vt_score_update (vt_score_t *score, unsigned int id, vt_check_result_t match,
  int points)
{
  /* NOTE: Since this function is called many times throughout the application,
     I thought I would optimize it by leaving out the mutexes. If there's
     something utterly wrong with this approach, please notify me. */
  __sync_add_and_fetch (&score->points, points);
  /* NOTE: Since every check has a unique identifier, it shouldn't be necessary
     to protect the results with a memory barrier. Again if this assumption is
     false, please notify me. */
  score->results[id] = match;

  return 0;
}

int
vt_score_wait (vt_score_t *score)
{
  int ret;

  if ((ret = pthread_mutex_lock (&score->lock)) != 0) {
    vt_error ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
    errno = ret;
    return -1;
  }

  while (score->writers > 0) {
    if ((ret = pthread_cond_wait (&score->signal, &score->lock)) != 0)
      vt_panic ("%s: pthread_cond_wait: %s", __func__, strerror (ret));
  }

  if ((ret = pthread_mutex_unlock (&score->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

  return 0;
}
