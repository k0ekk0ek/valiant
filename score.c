/* system includes */
#include <errno.h>
#include <string.h>

/* valiant includes */
#include "score.h"


score_t *
score_new (void)
{
  score_t *score;
  gchar *error;

  if ((score = g_new0 (score_t, 1)) == NULL) {
    error = strerror (errno);
    g_warning ("%s: %s", __func__, error);
    return NULL;
  }

  if ((score->wait = g_mutex_new ()) == NULL) {
    error = strerror (errno);
    g_free (score);
    g_warning ("%s: %s", __func__, error);
    return NULL;
  }

  if ((score->update = g_mutex_new ()) == NULL) {
    error = strerror (errno);
    g_mutex_free (score->wait);
    g_free (score);
    g_warning ("%s: %s", __func__, error);
    return NULL;
  }

  score->writers = 0;
  score->points = 0;

  return score;
}


void
score_free (score_t *score)
{
  // IMPLEMENT

  return;
}


void
score_writers_up (score_t *score)
{
  g_return_if_fail (score != NULL);

  g_mutex_lock (score->update);

  score->writers++;

  g_mutex_trylock (score->wait);

  g_mutex_unlock (score->update);

  return;
}


void
score_writers_down (score_t *score)
{
  g_return_if_fail (score != NULL);

  g_mutex_lock (score->update);

  if (  score->writers)
    score->writers--;
  if (! score->writers)
    g_mutex_unlock (score->wait);

  g_mutex_unlock (score->update);

  return;
}


gint
score_update (score_t *score, gint points)
{
  g_mutex_lock (score->update);

  if (points)
    score->points += points;

  gint total = score->points;

  g_mutex_unlock (score->update);

  return total;
}


void
score_wait (score_t *score)
{
  g_mutex_lock (score->wait);
  return;
}



















