#ifndef _CHECK_STRING_H_INCLUDED_
#define _CHECK_STRING_H_INCLUDED_

/* system includes */

#include <stdbool.h>


/* valiant includes */

#include "check.h"


typedef struct check_str_struct check_str_t;

struct check_str_struct {
  bool  fold; /* case sensitive or case insensitive */
  int   left; /* left-hand side */
  char *right; /* right-hand side */
};

check_str_t *check_str_alloc (void);

check_t *cfg_to_check_str (cfg_t *);
check_t *check_str_create (int, int, char *, char *, bool);

#endif
