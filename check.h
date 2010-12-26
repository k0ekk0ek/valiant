#ifndef _CHECK_H_INCLUDED_
#define _CHECK_H_INCLUDED_

/* system includes */
#include <confuse.h>
#include <stdbool.h>

/* valiant includes */
#include "request.h"
#include "score.h"


#define CHECK_POSITIVE_SCORE (1.0)
#define CHECK_NEGATIVE_SCORE (0.0)


typedef struct check_struct check_t;

typedef int(*CHECK_CHECK_FN)(check_t *, request_t *, score_t *);
typedef void(*CHECK_FREE_FN)(check_t *);

struct check_struct {
  bool  slow; /* indicates if check is expensive */
  int   plus; /* score to use if check matches */
  int   minus; /* score to use if check does not match */
  void *extra; /* pointer to check specific information */

  CHECK_CHECK_FN  check_fn;
  CHECK_FREE_FN   free_fn;
};

check_t *check_alloc (void);


typedef struct checklist_struct checklist_t;

struct checklist_struct {
  check_t     *check;
  checklist_t *next;
};


checklist_t *checks; /* used by valiant */


checklist_t *cfg_to_checklist (cfg_t *);
checklist_t *checklist_alloc (void);
checklist_t *checklist_append (checklist_t *, check_t *);
//checklist_t *checklist_prioritize (checklist_t *);

#endif
