#ifndef CHECK_H_INCLUDED
#define CHECK_H_INCLUDED

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "request.h"
#include "score.h"
#include "slist.h"

/* definitions */
#define CHECK_SUCCESS (0)
#define CHECK_ERR_SYSTEM (1)

typedef struct check_struct check_t;

typedef int(*CHECK_CHECK_FN)(check_t *, request_t *, score_t *);
typedef void(*CHECK_FREE_FN)(check_t *);

struct check_struct {
  int prio; /* indicates how expensive check */
  void *info; /* pointer to check specific information */
  CHECK_CHECK_FN check_fn;
  CHECK_FREE_FN free_fn;
};


// throw in extra sizeof to determine member size!
//#define CHECK_ALLOC(type) check_alloc (sizeof (type))
//
//check_t *check_alloc (size_t);
//
//
//
/* very simple structure to store the information needed for resolving etc */
typedef struct check_arg_struct check_arg_t;
//
struct check_arg_struct {
  check_t *check;
  request_t *request;
  score_t *score;
};
//
check_arg_t *check_arg_create (check_t *, request_t *, score_t *);

// FIXME: shouldn't be defined here!
slist_t *checks; /* used by valiant */

// confuse_to_checklist
//checklist_t *cfg_to_checklist (cfg_t *);
//checklist_t *checklist_alloc (void);
//checklist_t *checklist_append (checklist_t *, check_t *);
// confuse_to_checklist
//
// XXX: eventually this will be enabled/disabled using a define
//check_t *check_pcre_create (int, int, bool, int, const char *);
//
// XXX: same here
//check_t *check_dnsbl_create (int, int, const char *);
//
// XXX: same here
//check_t *check_str_create (int, int, char *, char *, bool);

#endif

