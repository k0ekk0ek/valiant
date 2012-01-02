/* system includes */
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "state.h"

#define TIME_FRAME (60)
#define MAX_ERRORS (10)
#define HALF_HOUR_IN_SECONDS (1800) /* minimum back off time */
#define BACK_OFF_TIME HALF_HOUR_IN_SECONDS /* half hour in seconds */
#define MAX_BACK_OFF_TIME (86400) /* 1 day in seconds */

int
vt_state_init (vt_state_t *state, int locks, vt_error_t *error)
{
  int i, n;
  pthread_rwlock_t *rwlock;

  if (locks) {
    if (! (rwlock = calloc (1, sizeof (pthread_rwlock_t)))) {
      vt_set_error (error, VT_ERR_NOMEM);
      vt_error ("%s: calloc: %s", __func__, strerror (errno));
      return (-1);
    }
  } else {
    rwlock = NULL;
  }

  for (i=1, n=ceil ((MAX_BACK_OFF_TIME / BACK_OFF_TIME)); i < n; i<<=1)
    ;

  memset (state, 0, sizeof (vt_state_t));
  state->time_frame = TIME_FRAME;
  state->back_off_secs = BACK_OFF_TIME;
  state->max_back_off_secs = MAX_BACK_OFF_TIME;
  state->max_errors = MAX_ERRORS;
  state->max_power = i;
  state->lock = rwlock;

  return (0);
}

int
vt_state_deinit (vt_state_t *state, vt_error_t *error)
{
  int ret;

  assert (state);

  if (state->lock) {
    if ((ret = pthread_rwlock_destroy (state->lock)) != 0)
      return (ret);
    free (state->lock);
  }

  memset (state, 0, sizeof (vt_state_t));
  return (0);
}

int
vt_state_get_attr (vt_state_t *state, vt_state_attr_t attr)
{
  int ret, res;

  assert (state);

  if (state->lock && (ret = pthread_rwlock_rdlock (state->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));

  if (attr == VT_STATE_ATTR_MAX_ERRORS)
    res = state->max_errors;
  else if (attr == VT_STATE_ATTR_TIME_FRAME)
    res = state->time_frame;
  else if (attr == VT_STATE_ATTR_BACK_OFF_TIME)
    res = state->back_off_secs;
  else if (attr == VT_STATE_ATTR_MAX_BACK_OFF_TIME)
    res = state->max_back_off_secs;
  else
    res = -1;

  if (state->lock && (ret = pthread_rwlock_unlock (state->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));

  return (res);
}

void
vt_state_set_attr (vt_state_t *state, vt_state_attr_t attr, unsigned int num)
{
  int i, n, ret;

  assert (state);
  assert (attr >= VT_STATE_ATTR_MAX_ERRORS);
  assert (attr <= VT_STATE_ATTR_MAX_BACK_OFF_TIME);

  if (state->lock && (ret = pthread_rwlock_wrlock (state->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));

  if (attr == VT_STATE_ATTR_MAX_ERRORS)
    state->max_errors = num;
  else if (attr == VT_STATE_ATTR_TIME_FRAME)
    state->time_frame = num;
  else if (attr == VT_STATE_ATTR_BACK_OFF_TIME)
    state->back_off_secs = num;
  else if (attr == VT_STATE_ATTR_MAX_BACK_OFF_TIME)
    state->max_back_off_secs = num;

  /* the max_power member should be reset under certain conditions */
  if (attr == VT_STATE_ATTR_BACK_OFF_TIME ||
      attr == VT_STATE_ATTR_MAX_BACK_OFF_TIME)
  {
    n = ceil ((state->max_back_off_secs / state->back_off_secs));
    for (i=1; i < n; i<<=1)
      ;

    state->max_power = i;
  }

  if (state->lock && (ret = pthread_rwlock_unlock (state->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
}

int
vt_state_omit (vt_state_t *state)
{
  int omit, ret;
  time_t now = time (NULL);

  if (state->lock && (ret = pthread_rwlock_rdlock (state->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));
  omit = (state->back_off > now) ? 1 : 0;
  if (state->lock && (ret = pthread_rwlock_unlock (state->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));

  return (omit);
}

void
vt_state_error (vt_state_t *state)
{
  int ret, power;
  double multiplier;
  time_t time_slot, now = time(NULL);

  if (state->lock && (ret = pthread_rwlock_wrlock (state->lock)))
    vt_panic ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));

  if (now > state->back_off) {
    /* error counter should only be increased if we're within the allowed time
       frame. */
    if (state->start > state->back_off &&
        state->start > (now - state->time_frame))
    {
      if (++state->errors >= state->max_errors) {
        state->errors = 0;

        // FIXME: update comment
        /* current power must be updated, since the check might not have
           failed for a long time. here. The general idea here is to get
           the difference between back off time and now and divide it by the
           number of seconds in BACK_OFF_TIME. That gives us the multiplier.
           then we substrac the value of power either until power reaches 1
           or the multiplier is smaller than one! */
        power = state->power;
        time_slot = (now - state->back_off);

        if (state->power >= state->max_power) {
          if (time_slot > state->max_back_off_secs) {
            power <<= 1;
            time_slot -= state->max_back_off_secs;
          } else {
            time_slot = 0;
          }
        }

        multiplier = (time_slot / state->back_off_secs);

        for (; power > 1 && power < multiplier; power<<=1)
          multiplier -= power;

        // now: either power is 1... which is 2^0
        // or: multiplier is less than 1!
        // if multiplier is smaller than 1... we have our current power
        // the one thing that is funny here is the max_power thing... if we've
        // reached that we should first substract that one!
        // we should update the power... it's only fair... right?
        // we do that by doing a log... so... that's the current time
        // minus the current back off time... divided by the back off time and
        // then the log2 of the multiplier

        /* now that the power is updated we increase it... and set the back
           off time! */
        state->power = power;

        if (state->power < state->max_power) {
          state->power >>= 1;
          state->back_off = now + (state->back_off_secs * state->power);
        } else {
          state->power = state->max_power;
          state->back_off = now + state->max_back_off_secs;
        }
      }
    } else {
      state->start = time (NULL);
      state->errors = 1;
    }
  }

  if (state->lock && (ret = pthread_rwlock_unlock (state->lock)) != 0)
    vt_panic ("%s; pthread_rwlock_unlock: %s", __func__, strerror (ret));
}

#undef TIME_FRAME
#undef MAX_ERRORS
#undef HALF_HOUR_IN_SECONDS
#undef BACK_OFF_TIME
#undef MAX_BACK_OFF_TIME
