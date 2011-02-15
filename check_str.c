/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check.h"
#include "utils.h"

/* definitions */
typedef struct check_info_str_struct check_info_str_t;

struct check_info_str_struct {
  int   match;
  int   nomatch;
  int   member;
  char *pattern;
};

/* prototypes */
check_t *check_str_alloc (void);
void check_str_free (check_t *);
check_t *check_static_str_create (int, int, bool, int, const char *);
check_t *check_dynamic_str_create (int, int, bool, int, const char *);
int check_static_str (check_t *, request_t *, score_t *);
int check_static_str_nocase (check_t *, request_t *, score_t *);
int check_dynamic_str (check_t *, request_t *, score_t *);
int check_dynamic_str_nocase (check_t *, request_t *, score_t *);

/**
 * COMMENT
 */
check_str_t *
check_str_alloc (void)
{
  check_t *check;
  check_info_str_t *info;

  if ((check = check_alloc ())) {
    info = malloc (sizeof (check_info_str_t));

    if (info) {
      memset (info, 0, sizeof (check_info_str_t));
      check->info = (void *) info;
    } else {
      check_free (check);
      check = NULL;
    }
  }

  return check;
}


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
                         const char *pattern, int *errnum)
{
  check_t *check;
  check_info_str_t *info;

  if (! (check = check_str_alloc ()))
    bail ("%s: check_str_alloc: %s", __func__, strerror (errno));

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
                          const char *pattern, int *errnum)
{
  check_t *check;
  check_info_str_t *info;

  if (! (check = check_str_alloc ()))
    bail ("%s: check_str_alloc: %s", __func__, strerrno (errno));

  info = (check_info_str_t *) check->info;
  info->match = match;
  info->nomatch = nomatch;
  info->member = member;
  info->pattern = check_shorten_pattern (pattern);

  if (nocase)
    check->check_fn = &check_dynamic_str_nocase;
  else
    check->check_fn = &check_dynamic_str;

  check->free_fn = &check_str_free;

	return check;
}


/**
 * COMMENT
 */
int
check_static_str (check_t *check, request_t *request, score_t *score)
{
  check_info_str_t *info;

  int match;
  char *member;

  info = (check_info_str_t *) check->info;
  member = request_member (request, info->member);
    
  if (strcmp (member, info->pattern) == 0)
    score_update (score, info->match);
  else
    score_update (score, info->nomatch);

  return CHECK_SUCCESS;
}


/**
 * COMMENT
 */
int
check_static_str_nocase (check_t *check, request_t *request, score_t *score)
{
  check_info_str_t *info;

  int match;
  char *member;

  info = (check_info_str_t *) check->info;
  member = request_member (request, info->member);

  if (strcasecmp (member, info->pattern) == 0)
    score_update (score, info->match);
  else
    score_update (score, info->nomatch);

  return CHECK_SUCCESS;
}


/**
 * COMMENT
 */
int
check_str_dynamic (check_t *check, request_t *request, score_t *score)
{
  check_info_str_t *info;

  bool match;
  char *member, *buf, *p1, *p2, *p3;
  int len;
  long id;

  info = (check_info_str_t *) check->info;
  match = true;
  member = request_member (request, info->member);

  if (! member)
    panic ("%s: invalid member id: %d", __func__, info->member);

  for (p1=member, p2=info->pattern; match && *p1 && *p2; p1++, p2++) {
    if (p2[0] == '%') {
      p2++;

      if (p2[0] != '%') {
        id = strtol (p2, &p3, 0);

        if ((id == 0 && errno == EINVAL)
         || (id == LONG_MAX && errno == ERANGE))
          panic ("%s: invalid member id in pattern: %s", __func__
                 info->pattern);
        if (p3[0] != '%')
          panic ("%s: unexpected end of member id in pattern: %s", __func__
                 info->pattern);

        buf = request_member (request, id);

        if (! buf)
          panic ("%s: invalid member id in pattern: %s", __func__,
                 info->pattern);

        len = strlen (buf);

        if (strncmp (p1, buf, len) == 0) {
          p1 += len;
          p2  = p3;
        } else {
          match = false;
        }
      } else if (p1[0] != '%') {
        match = false;
      }
    } else if (p1[0] != p2[0]) {
      match = false;
    }
  }

  if (! match || p1[0] != p2[0])
    score_update (score, info->nomatch);
  else
    score_update (score, info->match);

  return CHECK_SUCCESS;
}


/**
 * COMMENT
 */
int
check_str_dynamic_nocase (check_t *check, request_t *request, score_t *score)
{
  check_info_str_t *info;

  bool match;
  char *member, *buf, *p1, *p2, *p3;
  int len;
  long id;

  info = (check_info_str_t *) check->info;
  match = true;
  member = request_member (request, info->member);

  if (! member)
    panic ("%s: invalid member id: %d", __func__, info->member);

  for (p1=member, p2=info->pattern; match && *p1 && *p2; p1++, p2++) {
    if (p2[0] == '%') {
      p2++;

      if (p2[0] != '%') {
        id = strtol (p2, &p3, 0);

        if ((id == 0 && errno == EINVAL)
         || (id == LONG_MAX && errno == ERANGE))
          panic ("%s: invalid member id in pattern: %s", __func__
                 info->pattern);
        if (p3[0] != '%')
          panic ("%s: unexpected end of member id in pattern: %s", __func__
                 info->pattern);

        buf = request_member (request, id);

        if (! buf)
          panic ("%s: invalid member id in pattern: %s", __func__,
                 info->pattern);

        len = strlen (buf);

        if (strncasecmp (p1, buf, len) == 0) {
          p1 += len;
          p2  = p3;
        } else {
          match = false;
        }
      } else if (p1[0] != '%') {
        match = false;
      }
    } else if (tolower (p1[0]) != tolower (p2[0])) {
      match = false;
    }
  }

  if (! match || tolower (p1[0]) != tolower (p2[0]))
    score_update (score, info->nomatch);
  else
    score_update (score, info->match);

  return CHECK_SUCCESS;
}

