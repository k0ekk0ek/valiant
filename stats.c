/* system includes */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* valiant includes */
#include "consts.h"
#include "score.h"
#include "stats.h"
#include "utils.h"

int
vt_stats_create (vt_stats_t **dest)
{
  int err, ret;
  vt_stats_t *stats;

  if (! (stats = malloc0 (sizeof (vt_stats_t)))) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  stats->ctime = time (NULL);
  stats->mtime = stats->ctime;

  if ((ret = pthread_mutex_init (&stats->lock, NULL)) != 0) {
    vt_error ("%s: pthread_mutex_init: %s", __func__, strerror (ret));
    if (ret == ENOMEM)
      err = VT_ERR_NOMEM;
    else
      err = VT_ERR_SYS;
    goto FAILURE_MUTEX_INIT;
  }

  if ((ret = pthread_cond_init (&stats->signal, NULL)) != 0) {
    vt_error ("%s: pthread_cond_init: %s", __func__, strerror (ret));
    if (ret == ENOMEM)
      err = VT_ERR_NOMEM;
    else
      err = VT_ERR_SYS;
    goto FAILURE_COND_INIT;
  }

  *dest = stats;
  return VT_SUCCESS;

FAILURE_COND_INIT:
  /* don't care about return value here */
  (void)pthread_mutex_destroy (&stats->lock);
FAILURE_MUTEX_INIT:
  if (stats)
    free (stats);
  return err;
}

int
vt_stats_destroy (vt_stats_t *stats)
{
  // IMPLEMENT
}

int
vt_stats_add_cntr (unsigned int *id, vt_stats_t *stats, const char *name)
{
  int err, ret;
  unsigned int tmp;
  vt_stats_cntr_t *cntr, **cntrs;

  if ((ret = vt_stats_get_cntr_id (&tmp, stats, name)) == VT_SUCCESS) {
    *id = tmp;
    return VT_SUCCESS;
  }

  cntrs = NULL;
  if (! (cntr = malloc0 (sizeof (vt_stats_cntr_t))) ||
      ! (cntr->name = strdup (name)) ||
      ! (cntrs = realloc (stats->cntrs, sizeof (vt_stats_cntr_t *) * (stats->ncntrs + 2))))
  {
    vt_error ("%s: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  cntr->len = strlen (cntr->name);
  *id = stats->ncntrs;
  cntrs[stats->ncntrs++] = cntr;
  cntrs[stats->ncntrs] = NULL;
  stats->cntrs = cntrs;

  return VT_SUCCESS;
FAILURE:
  if (cntr)
    free (cntr);
  return err;
}

int
vt_stats_get_cntr_id (unsigned int *id, const vt_stats_t *stats,
  const char *name)
{
  int err, i;

  if (stats->cntrs) {
    for (i=0; i < stats->ncntrs; i++) {
      if (strcmp (name, (stats->cntrs)[i]->name) == 0) {
        *id = i;
        return VT_SUCCESS;
      }
    }
  }
  return VT_ERR_INVAL;
}

void
vt_stats_update (vt_stats_t *stats, const vt_score_t *score)
{
  int ret;
  unsigned int i;

  if ((ret = pthread_mutex_lock (&stats->lock)) != 0)
    vt_fatal ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  /* NOTE: We're only supposed to reach this point after all checks are done. I
     didn't implement locking of the score here because of that. I know these
     types of assumptions are risky. */
  if (stats->ncntrs != score->ncntrs)
    vt_panic ("%s: array sizes do not match, this is a serious bug", __func__);

  stats->nreqs++;
  for (i=0; i < stats->ncntrs; i++) {
    if ((score->cntrs)[i])
      (stats->cntrs)[i]->hits++;
  }

  if (pthread_mutex_unlock (&stats->lock) != 0)
    vt_fatal ("%s: pthread_mutex_lock: %s", __func__, strerror (errno));
}

#define SIZE_MAX_CHARS ((SIZE_MAX / 10) + 2)
#define FMT "check %s matched %d times"
#define FMT_NUM_CHARS (21)

int
vt_stats_print (vt_stats_t *stats)
{
  char *str, *ptr;
  int err;
  size_t len;
  ssize_t max, num;
  time_t ctime, utime;
  unsigned long nreqs;
  unsigned int i, ncntrs;
  vt_stats_cntr_t *cntr;

  if (pthread_mutex_lock (&stats->lock) != 0)
    vt_fatal ("%s: pthread_mutex_lock: %s", __func__, strerror (errno));

  stats->mtime = time (NULL);
  ctime = stats->ctime;
  utime = stats->mtime - stats->ctime;
  ncntrs = stats->ncntrs;
  nreqs = stats->nreqs;
  for (i=0, max=0; i < ncntrs; i++) {
    if ((stats->cntrs)[i]->len > max)
      max = (stats->cntrs)[i]->len;
  }

  max += (SIZE_MAX_CHARS + FMT_NUM_CHARS + 1);
  if (max > (ssize_t)(SIZE_MAX / ncntrs)) {
    vt_error ("%s: not enough memory to print statistics", __func__);
    err = VT_ERR_NOMEM;
    goto UNLOCK;
  }

  len = max * ncntrs;
  ptr = NULL;
  if ((str = malloc0 (len)) == NULL) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto UNLOCK;
  }

  for (i=0, ptr=str; i < ncntrs; i++, ptr+=max) {
    cntr = (stats->cntrs)[i];
    num = snprintf (ptr, max, FMT, max, cntr->name, cntr->hits);
    if (num < 0) {
      vt_error ("%s: malloc0: %s", __func__, strerror (errno));
      if (errno == ENOMEM)
        err = VT_ERR_NOMEM;
      else
        err = VT_ERR_SYS;
      goto UNLOCK;
    }
    cntr->hits = 0; /* reset counter */
  }

UNLOCK:
  if (pthread_mutex_unlock (&stats->lock) != 0)
    vt_fatal ("%s: pthread_mutex_unlock: %s", __func__, strerror (errno));

  if (err == VT_SUCCESS) {
    vt_info ("running for %j seconds since %s",
      utime, asctime (localtime (&ctime)));
    vt_info ("number of requests %d", nreqs);
    for (i=0, ptr=str; i < ncntrs; i++, ptr+=max) {
      vt_info ("%s", ptr);
    }
  }
  if (str) {
    free (str);
  }

  return err;
}

#undef SIZE_MAX_CHARS
#undef FMT
#undef FMT_NUM_CHARS

int
vt_stats_lock (vt_stats_t *stats)
{
  int err, ret;

  if ((ret = pthread_mutex_lock (&stats->lock)) != 0) {
    vt_error ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
    return VT_ERR_SYS;
  }
  return VT_SUCCESS;
}

int
vt_stats_unlock (vt_stats_t *stats)
{
  return 0;
}

int
vt_stats_print_cycle (vt_stats_t *stats, time_t cycle)
{
  int err, ret;

  if ((err = pthread_mutex_lock (&stats->lock)) != VT_SUCCESS) {
    return err;
  }

  stats->cycle = cycle;

  if ((err = pthread_mutex_unlock (&stats->lock)) != VT_SUCCESS) {
    return err;
  }
  if ((ret = pthread_cond_signal (&stats->signal)) != 0) {
    vt_error ("%s: pthread_cond_signal: %s", __func__, strerror (ret));
    return VT_ERR_SYS;
  }

  return VT_SUCCESS;
}

// do stuff, create new thread in vt_stats_printer and let it execute
//void
//vt_stats_worker (void *arg)
//{
//  // IMPLEMENT
//}
//
//int
//vt_stats_printer (vt_stats_t *stats)
//{
//  // thread that prints statistics at given intervals etc, etc
//  // does a timed wait etc
//  // IMPLEMENT, just not needed now!
//}
