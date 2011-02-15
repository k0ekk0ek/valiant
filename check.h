#ifndef _CHECK_H_INCLUDED_
#define _CHECK_H_INCLUDED_

/* system includes */
#include <confuse.h>
#include <stdbool.h>

/* valiant includes */
#include "request.h"
#include "score.h"


#define CHECK_DEFAULT_MATCH   (1.0)
#define CHECK_DEFAULT_NOMATCH (0.0)


typedef struct check_struct check_t;

typedef int(*CHECK_CHECK_FN)(check_t *, request_t *, score_t *);
typedef void(*CHECK_FREE_FN)(check_t *);

struct check_struct {
  int   priority; /* indicates how expensive check */
  void *info; /* pointer to check specific information */

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

/* very simple structure to store the information needed for resolving etc */
typedef struct check_arg_struct check_arg_t;

struct check_arg_struct {
  check_t *check;
  request_t *request;
  score_t *score;
};


/**
 * COMMENT
 */
check_arg_t *check_arg_create (check_t *, request_t *, score_t *);


// confuse_to_checklist
checklist_t *cfg_to_checklist (cfg_t *);
checklist_t *checklist_alloc (void);
checklist_t *checklist_append (checklist_t *, check_t *);
//checklist_t *checklist_prioritize (checklist_t *);

const char *check_shorten_pattern (const char *);
const char *check_unescape_pattern (const char *);

// XXX: eventually this will be enabled/disabled using a define
check_t *check_pcre_create (int, int, bool, int, const char *);

// XXX: same here
check_t *check_dnsbl_create (int, int, const char *);

// XXX: same here
check_t *check_str_create (int, int, char *, char *, bool);

#endif
