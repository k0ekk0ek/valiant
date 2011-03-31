/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "check_common.h"
#include "check_str.h"
#include "utils.h"

/* definitions */
typedef struct check_info_str_struct check_info_str_t;

struct check_info_str_struct {
  int member;
  char *pattern;
  int weight;
};

/* prototypes */
void check_str_free (check_t *);
//check_t *check_static_str_create (int, int, bool, int, const char *);
//check_t *check_dynamic_str_create (int, int, bool, int, const char *);
int check_static_str (check_t *, request_t *, score_t *);
int check_static_str_nocase (check_t *, request_t *, score_t *);
int check_dynamic_str (check_t *, request_t *, score_t *);
int check_dynamic_str_nocase (check_t *, request_t *, score_t *);

void
check_str_free (check_t *check)
{
  check_info_str_t *info;

  if (check) {
    if (check->info) {
      info = (check_info_str_t *) check->info;

      if (info->pattern)
        free (info->pattern);
      free (info);
    }

    check_free (check);
  }

  return;
}

check_t *
check_str_create (int match, int nomatch, bool nocase, int member,
                  const char *pattern)
{
  if (check_dynamic_pattern (pattern))
    return check_dynamic_str_create (match, nomatch, nocase, member, pattern);

  return check_static_str_create (match, nomatch, nocase, member, pattern);
}


check_t *
check_static_str_create (int match, int nomatch, bool nocase, int member,
                         const char *pattern)
{
  check_t *check;
  check_info_str_t *info;

  if ((check = CHECK_ALLOC (check_info_str_t)) == NULL)
    bail ("%s: CHECK_ALLOC: %s", __func__, strerror (errno));

  info = (check_info_str_t *) check->info;
  info->match = match;
  info->nomatch = nomatch;
  info->member = member;
  info->pattern = check_unescape_pattern (pattern);

  if (nocase)
    check->check_fn = &check_static_str_nocase;
  else
    check->check_fn = &check_static_str;

  check->free_fn = &check_str_free;

  return check;
}


check_t *
check_dynamic_str_create (int match, int nomatch, bool nocase, int member,
                          const char *pattern)
{
  check_t *check;
  check_info_str_t *info;

  if ((check = CHECK_ALLOC (check_info_str_t)) == NULL)
    bail ("%s: CHECK_ALLOC: %s", __func__, strerrno (errno));

  info = (check_info_str_t *) check->info;
  info->match = match;
  info->nomatch = nomatch;
  info->member = member;

  if ((info->pattern = strdup (pattern)) == NULL)
    bail ("%s: strdup: %s", __func__, strerrno (errno));

  if (nocase)
    check->check_fn = &check_dynamic_str_nocase;
  else
    check->check_fn = &check_dynamic_str;

  check->free_fn = &check_str_free;

	return check;
}

int
check_static_str (check_t *check, request_t *request, score_t *score)
{
  char *mem;
  check_info_str_t *info;

  info = (check_info_str_t *) check->info;
  mem = request_memberid (request, info->member);
    
  if (strcmp (mem, info->pattern) == 0)
    score_update (score, info->match);
  else
    score_update (score, info->nomatch);

  return CHECK_SUCCESS;
}

int
check_static_str_nocase (check_t *check, request_t *request, score_t *score)
{
  char *mem;
  check_info_str_t *info;

  info = (check_info_str_t *) check->info;
  mem = request_memberid (request, info->member);

  if (strcasecmp (mem, info->pattern) == 0)
    score_update (score, info->match);
  else
    score_update (score, info->nomatch);

  return CHECK_SUCCESS;
}

int
check_dynamic_str (check_t *check, request_t *request, score_t *score)
{
  char *mem1, *mem2;
  char *p1, *p2;
  check_info_str_t *info;

  info = (check_info_str_t *) check->info;
  mem1 = request_memberid (request, info->member);

  for (p1=info->pattern; *mem1 && *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {

      if ((p2 = strchr (p1, '%')) == NULL)
        return CHECK_ERR_PATTERN;

      mem2 = request_membern (request, p1, (size_t) (p2 - p1));
      p1 = p2;

      if (mem2) {
        for (; *mem1 && *mem1 == *mem2; mem1++, mem2++)
          ;

        if (*mem2 == '\0') {
          if (*mem1 == '\0')
            goto match;
        } else {
          goto nomatch;
        }
      }
    } else if (*mem1 == *p1) {
      mem1++;
      //p1++;
    } else {
      break;
    }
  }

  if (*mem1 == *p1) {
match:
    score_update (score, info->match);
  } else {
nomatch:
    score_update (score, info->nomatch);
  }

  return CHECK_SUCCESS;
}

#define CHRCASECMP(c1, c2) ((c1) == (c2) || (tolower ((c1)) == tolower ((c2))))

int
check_dynamic_str_nocase (check_t *check, request_t *request, score_t *score)
{
  char *mem1, *mem2;
  char *p1, *p2;
  check_info_str_t *info;

  info = (check_str_t *) check->info;
  mem1 = request_memberid (check->member);

  for (p1=info->pattern; *mem1 && *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {

      if ((p2 = strchr (p1, '%')) == NULL)
        return CHECK_ERR_PATTERN;

      mem2 = request_membern (p1, (size_t) (p2 - p1));
      p1 = p1;

      for (; *mem1 && CHRCASECMP (*mem1, *mem2) ; *mem1++, *mem2++)
        ;

      if (*mem2 == '\0') {
        if (*mem1 == '\0')
          goto match;
      } else {
        goto nomatch;
      }

    } else if (CHRCASECMP(*mem1, *p1)) {
      mem1++;
    } else {
      break;
    }
  }

  if (CHRCASECMP (*mem1, *p1)) {
match:
    score_update (score, info->match);
  } else {
nomatch:
    score_update (score, info->nomatch);
  }

  return CHECK_SUCCESS;
}

#undef CHRCASECMP

