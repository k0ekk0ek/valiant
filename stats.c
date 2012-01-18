/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* valiant includes */
#include "stats.h"

/* five minutes in seconds 60 * 5 = 300 (for debugging purposes) */
/* half an hour in seconds 60 * 30 = 1800 */
#define VT_STATS_DIFF_SECONDS (60)
#define VT_STATS_DIFF_MICROSECONDS (0)

vt_stats_t *
vt_stats_create (vt_dict_t **dicts, int ndicts, vt_error_t *err)
{
  int i, ret;
  vt_stats_t *stats;

  if (! (stats = calloc (1, sizeof (vt_stats_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return NULL;
  }

  stats->ctime = time (NULL);
  stats->mtime = stats->ctime;
  stats->ncntrs = ndicts;

  if (! (stats->cntrs = calloc (stats->ncntrs, sizeof (vt_stats_cntr_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  for (i = 0; i < stats->ncntrs; i++) {
    if (dicts[i]) {
      stats->cntrs[i].len = strlen (dicts[i]->name);
      if (! (stats->cntrs[i].name = calloc (stats->cntrs[i].len+1, sizeof (char)))) {
        vt_set_error (err, VT_ERR_NOMEM);
        vt_error ("%s: calloc: %s", __func__, strerror (errno));
        goto failure;
      }
      strcpy (stats->cntrs[i].name, dicts[i]->name);
    }
  }

  if ((ret = pthread_mutex_init (&stats->lock, NULL)) != 0) {
    if (ret != ENOMEM)
      vt_panic ("%s: pthread_mutex_init: %s", __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: pthread_mutex_init: %s", __func__, strerror (ret));
    goto failure;
  }

  if ((ret = pthread_cond_init (&stats->signal, NULL)) != 0) {
    if (ret != ENOMEM)
      vt_fatal ("%s: pthread_cond_init: %s", __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: pthread_cond_init: %s", __func__, strerror (ret));
    goto failure_mutex_init;
  }

  return stats;
failure_mutex_init:
  (void)pthread_mutex_destroy (&stats->lock);
failure:
  if (stats) {
    if (stats->cntrs) {
      for (i = 0; i < stats->ncntrs; i++) {
        if (stats->cntrs[i].name)
          free (stats->cntrs[i].name);
      }
      free (stats->cntrs);
    }
    free (stats);
  }
  return NULL;
}

int
vt_stats_destroy (vt_stats_t *stats, vt_error_t *err)
{
  int cntrno, ret;
  struct timespec wait;

  assert (stats);

  /* lock */
  if ((ret = pthread_mutex_lock (&stats->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  /* NOTE: The for loop below is used to terminate the worker thread if it's
     still running. */
  stats->dead;
  for (; stats->worker; ) {
    if ((ret = pthread_cond_signal (&stats->signal)) != 0)
      vt_panic ("%s: pthread_cond_signal: %s", __func__, strerror (ret));

    wait.tv_sec = (time (NULL) + 5); /* magic number, BAD! */
    wait.tv_nsec = 0;
    /* unlocked by pthread_cond_timedwait */
    ret = pthread_cond_timedwait (&stats->signal, &stats->lock, &wait);
    if (ret && ret != ETIMEDOUT)
      vt_panic ("%s: pthread_cond_timedwait: %s", __func__, strerror (ret));
    /* locked by pthread_cond_timedwait */
  }

  /* unlock */
  if ((ret = pthread_mutex_unlock (&stats->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

  if (stats->cntrs) {
    for (cntrno = 0; cntrno < stats->ncntrs; cntrno++) {
      if (stats->cntrs[cntrno].name)
        free (stats->cntrs[cntrno].name);
    }
    free (stats->cntrs);
  }
  free (stats);

  return 0;
}

void
vt_stats_update (vt_stats_t *stats, vt_result_t *res)
{
  int ret;
  int i;

  if ((ret = pthread_mutex_lock (&stats->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  /* NOTE: We're only supposed to reach this point after all checks are done. I
     didn't implement locking of the result here because of that. I know these
     types of assumptions are risky. */
  if (stats->ncntrs != res->nresults)
    vt_panic ("%s: number of counters do not match", __func__);

  stats->nreqs++;
  for (i=0; i < stats->ncntrs; i++) {
    if (res->results[i] && res->results[i]->points)
      stats->cntrs[i].hits++;
  }

  if ((ret = pthread_mutex_unlock (&stats->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
}

#define BUFLEN (32)
void *
vt_stats_worker (void *arg)
{
  char buf[BUFLEN];
  int cntrno, ret;
  struct timespec wait;
  vt_stats_t *stats;

  assert (arg);

  stats = (vt_stats_t *)arg;

  if ((ret = pthread_mutex_lock (&stats->lock)) != 0)
    vt_panic ("%s: pthread_mutex_lock: %s", __func__, strerror (ret));
  if (stats->worker || stats->dead)
    goto unlock;

  stats->worker = 1;

  for (; ! stats->dead; ) {
    /* calculate when to wake up */
    wait.tv_sec = stats->mtime + VT_STATS_DIFF_SECONDS;
    wait.tv_nsec = VT_STATS_DIFF_MICROSECONDS;

    /* unlocked by pthread_cond_timedwait */
    ret = pthread_cond_timedwait (&stats->signal, &stats->lock, &wait);
    if (ret && ret != ETIMEDOUT)
      vt_panic ("%s: pthread_cond_timedwait: %s", __func__, strerror (ret));
    /* locked by pthread_cond_timedwait */

    stats->mtime = time (NULL);
    //  Wed Jan 18 14:42:54 2012
    // fmt: %a %b %d %T %Y
    strftime (buf, BUFLEN, "%a %b %d %T %Y", localtime (&stats->ctime));
    vt_info ("running for %u seconds since %s",
      (stats->mtime - stats->ctime), buf);
    vt_info ("number of requests %d", stats->nreqs);
    for (cntrno = 0; cntrno < stats->ncntrs; cntrno++) {
      vt_info ("check %s matched %u times",
        stats->cntrs[cntrno].name, stats->cntrs[cntrno].hits);
      stats->cntrs[cntrno].hits = 0;
    }

    stats->nreqs = 0;
  }

  stats->worker = 0;
  if ((ret = pthread_cond_signal (&stats->signal)) != 0)
    vt_panic ("%s: ptrhead_cond_signal: %s", __func__, strerror (ret));

unlock:
  /* locked by pthread_cond_timedwait */
  if ((ret = pthread_mutex_unlock (&stats->lock)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

  return NULL;
}
#undef BUFLEN

void
vt_stats_thread (vt_stats_t *stats)
{
  int ret;
  pthread_t thread;

  assert (stats);

  ret = pthread_create (&thread, NULL, &vt_stats_worker, (void *)stats);
  if (ret != 0)
    vt_panic ("%s: pthread_create: %s", __func__, strerror (ret));
}
