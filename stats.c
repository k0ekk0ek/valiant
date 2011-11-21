/* system includes */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* valiant includes */
#include "error.h"
#include "score.h"
#include "stats.h"

vt_stats_t *
vt_stats_create (vt_errno_t *, vt_errno_t *err)
{
  int ret;
  vt_stats_t *stats;

  if (! (stats = calloc (1, sizeof (vt_stats_t)))) {
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    return NULL;
  }

  stats->ctime = time (NULL);
  stats->mtime = stats->ctime;

  switch ((ret = pthread_mutex_init (&stats->lock, NULL))) {
    case 0:
      break;
    case ENOMEM:
      vt_set_errno (err, VT_ERR_NOMEM);
      vt_error ("%s (%d): pthread_mutex_init: %s", __func__, __LINE__,
        strerror (ret));
      goto FAILURE_MUTEX_INIT;
    default:
      vt_fatal ("%s (%d): pthread_mutex_init: %s", __func__, __LINE__,
        strerror (ret));
  }

  switch ((ret = pthread_cond_init (&stats->signal, NULL))) {
    case 0:
      break;
    case ENOMEM:
      vt_set_errno (err, VT_ERR_NOMEM);
      vt_error ("%s (%d): pthread_cond_init: %s", __func__, __LINE__,
        strerror (ret));
      goto FAILURE_COND_INIT;
    default:
      vt_fatal ("%s (%d): pthread_cond_init: %s", __func__, __LINE__,
        strerror (ret));
  }

  return VT_SUCCESS;
FAILURE_COND_INIT:
  (void)pthread_mutex_destroy (&stats->lock);
FAILURE_MUTEX_INIT:
  if (stats)
    free (stats);
  return NULL;
}

int
vt_stats_destroy (vt_stats_t *stats)
{
  // IMPLEMENT
}

int
vt_stats_add_cntr (vt_stats_t *stats, const char *name, vt_errno_t *err)
{
  int pos, ret;
  unsigned int tmp;
  vt_stats_cntr_t *cntr, **cntrs;

  if ((pos = vt_stats_get_cntr_pos (&tmp, stats, name)) >= 0)
    return pos;

  cntrs = NULL;
  if (! (cntr = calloc (1, sizeof (vt_stats_cntr_t))) ||
      ! (cntr->name = strdup (name)) ||
      ! (cntrs = realloc (stats->cntrs, sizeof (vt_stats_cntr_t *) * (stats->ncntrs + 2))))
  {
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  cntr->len = strlen (cntr->name);
  pos = stats->ncntrs;
  cntrs[stats->ncntrs++] = cntr;
  cntrs[stats->ncntrs] = NULL;
  stats->cntrs = cntrs;

  return pos;
FAILURE:
  if (cntr) {
    if (cntr->name)
      free (cntr->name);
    free (cntr);
  }
  return -1;
}

int
vt_stats_get_cntr_pos (const vt_stats_t *stats, const char *name)
{
  int err, i;

  for (i=0; i < stats->ncntrs; i++) {
    if (strcmp (name, (stats->cntrs)[i]->name) == 0)
      return i;
  }
  return -1;
}

void
vt_stats_update (vt_stats_t *stats, const vt_score_t *score)
{
  int ret;
  int i;

  if ((ret = pthread_mutex_lock (&stats->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_lock: %s", __func__, __LINE__,
      strerror (ret));
  /* NOTE: We're only supposed to reach this point after all checks are done. I
     didn't implement locking of the score here because of that. I know these
     types of assumptions are risky. */
  if (stats->ncntrs != score->ncntrs)
    vt_panic ("%s (%d): number of counters in stats and score do not match",
      __func__, __LINE__);

  stats->nreqs++;
  for (i=0; i < stats->ncntrs; i++) {
    if ((score->cntrs)[i])
      (stats->cntrs)[i]->hits++;
  }

  if (pthread_mutex_unlock (&stats->lock) != 0)
    vt_fatal ("%s (%d): pthread_mutex_lock: %s", __func__, __LINE__,
      strerror (errno));
}

#define SIZE_MAX_CHARS ((SIZE_MAX / 10) + 2)
#define FMT "check %s matched %d times"
#define FMT_NUM_CHARS (21)

int
vt_stats_print (vt_stats_t *stats, vt_errno_t *err)
{
  char *ptr, *str = NULL;
  int i;
  int ret, res = 0;
  ssize_t max, num;
  time_t ctime, utime;
  unsigned long nreqs;
  unsigned int i, ncntrs;
  vt_stats_cntr_t *cntr;

  if ((ret = pthread_mutex_lock (&stats->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_lock: %s", __func__, __LINE__, strerror (ret));

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
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): not enough memory to print statistics", __func__, __LINE__);
    goto UNLOCK;
  }

  ptr = NULL;
  if ((str = calloc (ncntrs, max)) == NULL) {
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    goto UNLOCK;
  }

  for (i=0, ptr=str; i < ncntrs; i++, ptr+=max) {
    cntr = (stats->cntrs)[i];
    if ((num = snprintf (ptr, max, FMT, max, cntr->name, cntr->hits)) < 0) {
      if (errno == ENOMEM) {
        vt_set_errno (err, VT_ERR_NOMEM);
        vt_error ("%s (%d): snprintf: %s", __func__, __LINE__, strerror (errno));
        goto UNLOCK;
      }
      vt_fatal ("%s (%d): snprintf: %s", __func__, __LINE__, strerror (errno));
    }
    cntr->hits = 0; /* reset counter */
  }

  res = 1;

UNLOCK:
  if ((ret = pthread_mutex_unlock (&stats->lock)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_unlock: %s", __func__, __LINE__, strerror (ret));

  if (res) {
    vt_info ("running for %j seconds since %s",
      utime, asctime (localtime (&ctime)));
    vt_info ("number of requests %d", nreqs);
    for (i=0, ptr=str; i < ncntrs; i++, ptr+=max) {
      vt_info ("%s", ptr);
    }
  }

  if (str)
    free (str);

  return res ? 0 : -1;
}

#undef SIZE_MAX_CHARS
#undef FMT
#undef FMT_NUM_CHARS

//int
//vt_stats_print_cycle (vt_stats_t *stats, time_t cycle)
//{
//  IMPLEMENT
//}
//
//void
//vt_stats_worker (void *arg)
//{
//  IMPLEMENT
//  Create new thread in vt_stats_printer and let it execute this function. I
//  will then print statistics at given intervals using a timed wait.
//}
//
//int
//vt_stats_printer (vt_stats_t *stats)
//{
//  IMPLEMENT
//}

