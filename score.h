#ifndef _SCORE_H_INCLUDED_
#define _SCORE_H_INCLUDED_

/* system includes */
#include <glib.h>


typedef struct score_struct score_t;

struct score_struct {
  GCond  *done;
  GMutex *wait;
  GMutex *update;
  gint    writers;
  gint    points;
};

score_t *score_new (void);
void score_free (score_t *);

void score_writers_up (score_t *);
void score_writers_down (score_t *);

gint score_update (score_t *, gint);
void score_wait (score_t *);

#endif
