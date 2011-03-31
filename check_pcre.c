/**
 * @file check_pcre.c
 * @brief check_pcre.c implements support for Perl-compatible regular
 * expressions in valiant.
 *
 * @author Jeroen Koekkoek
 * @date 01/30/2011
 */

/* system includes */
#include <errno.h>
#include <pcre.h>
#include <limits.h>
#include <stdio.h> /* FIXME: must be removed, used for DEBUG only */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "check.h"
#include "utils.h"


/*
 * Unlike normal string comparison, as implemented by check_str.c, there is a
 * huge difference with static and dynamic pcre checks. Because with static
 * pcre checks the string is pre-compiled where as with dynamic pcre checks the
 * pattern is compiled when the check is executed. Therefore we need two
 * fundamentally different structures... also this comment must be altered :-).
 */

typedef struct check_info_static_pcre_struct check_info_static_pcre_t;

struct check_info_static_pcre_struct {
  int         match;
  int         nomatch;
  int         member;
  pcre       *regex;
  pcre_extra *regex_extra;
};

typedef struct check_info_dynamic_pcre_struct check_info_dynamic_pcre_t;

struct check_info_dynamic_pcre_struct {
  int   match;
  int   nomatch;
  int   member;
  char *pattern;
  int   options;
};


/* prototypes */
void check_static_pcre_free (check_t *);
void check_dynamic_pcre_free (check_t *);
check_t *check_static_pcre_create (int, int, bool, int, const char *);
check_t *check_dynamic_pcre_create (int, int, bool, int, const char *);
int check_static_pcre (check_t *, request_t *, score_t *);
int check_dynamic_pcre (check_t *, request_t *, score_t *);

void
check_static_pcre_free (check_t *check)
{
  check_info_static_pcre_t *info;

  if (check) {
    if (check->info) {
      info = (check_info_static_pcre_t *) check->info;

      if (info->regex)
        pcre_free (info->regex);
      if (info->regex_extra)
        pcre_free (info->regex_extra);
      free (info);
    }

    free (check);
  }

  return;
}


void
check_dynamic_pcre_free (check_t *check)
{
  check_info_dynamic_pcre_t *info;

  if (check) {
    if (check->info) {
      info = (check_info_dynamic_pcre_t *) check->info;

      if (info->pattern)
        free (info->pattern);
      free (info);
    }

    free (check);
  }

  return;
}


/**
 * COMMENT
 */
check_t *
check_pcre_create (int match, int nomatch, bool nocase, int member,
                   const char *pattern)
{
  char *p;

  /* Check whether PCRE is static or dynamic */
  for (p=pattern; *p; p++) {
    if (p[0] == '%' && p[1] != '%')
      return check_dynamic_pcre_create (match, nomatch, nocase, member,
                                        pattern);
  }

  return check_static_pcre_create (match, nomatch, nocase, member, pattern);
}


/**
 * COMMENT
 */
check_t *
check_static_pcre_create (int match, int nomatch, bool nocase, int member,
                          const char *pattern)
{
  check_t *check;
  check_info_static_pcre_t *info;

  pcre *regex;
  pcre_extra *regex_extra;

  const char *str;
  char *error;
  int offset;
  int options;


  check = CHECK_ALLOC (check_info_static_pcre_t);

  if (check == NULL)
    bail ("%s: %s", __func__, strerror (errno));

  info = (check_info_static_pcre_t *) check->info;

  if (! (str = check_unescape_pattern (pattern)))
    bail ("%s: check_unescape_pattern: %s", __func__, strerror (errno));

  options = PCRE_UTF8;

  if (nocase)
    options |= PCRE_CASELESS;

  regex = pcre_compile (str, options, &error, &offset, NULL);

  if (! regex)
    bail ("%s: pcre_compile: compilation failed at offset %d: %s", __func__,
          offset, error);

  regex_extra = pcre_study (regex, 0, &error);

  if (! regex_extra && error)
    bail ("%s: pcre_study: %s", __func__, error);


  info->match = match;
  info->nomatch = nomatch;
  info->member = member;
  info->regex = regex;
  info->regex_extra = regex_extra;

fprintf ("%s: member: %d\n", __func__, member);

//  check->info = info;
  check->check_fn = &check_static_pcre;
  check->free_fn = &check_static_pcre_free;

  return check;
}


check_t *
check_dynamic_pcre_create (int match, int nomatch, bool nocase, int member,
                           const char *pattern)
{
  check_t *check;
  check_info_dynamic_pcre_t *info;

  if ((check = CHECK_ALLOC (check_info_dynamic_pcre_t)) == NULL)
    bail ("%s: CHECK_ALLOC: %s", __func__, strerror (errno));

  info = (check_info_dynamic_pcre_t *) check->info;
  info->match = match;
  info->nomatch = nomatch;
  info->member = member;
  if ((info->pattern = strdup (pattern)) == NULL)
    bail ("%s: strdup: %s", __func__, strerror (errno));
  info->options = PCRE_UTF8;

  if (nocase)
    info->options |= PCRE_CASELESS;

  check->info = (void *) info;
  check->check_fn = &check_dynamic_pcre;
  check->free_fn = &check_dynamic_pcre_free;

  return check;
}


/**
 * COMMENT
 */
int
check_static_pcre (check_t *check, request_t *request, score_t *score)
{
  char *member;
  check_info_static_pcre_t *info;
  int result;

  info = (check_info_static_pcre_t *) check->info;
  member = request_memberid (request, info->member);

  if (! member)
    panic ("%s: no member with id %d", __func__, info->member);

  result = pcre_exec (info->regex, info->regex_extra, member, strlen (member),
                      0, 0, NULL, 0);

  if (result < 0) {
    switch (result) {
      case PCRE_ERROR_NOMATCH:
        fprintf (stderr, "%s: it didn't match, score is %d", __func__, info->nomatch);
        score_update (score, info->nomatch);
        break;
      //case PCRE_ERROR_NOMEMORY:
      //  // XXX: something to handle error as temporary!
      //  break;
      default:
        panic ("%s: a permanent error has occurred, this might indicate a bug",
                 __func__);
    }
  } else {
    fprintf (stderr, "%s: it matched, score is %d", __func__, info->match);
    score_update (score, info->match);
  }

  return CHECK_SUCCESS;
}


int
check_dynamic_pcre (check_t *check, request_t *request, score_t *score)
{
  char *buf, *errptr, *mem1, *mem2, *p1, *p2, *p3;
  check_info_dynamic_pcre_t *info;
  int erroffset, ret;
  pcre *regex;
  ssize_t len;

  info = (check_info_dynamic_pcre_t *) check->info;
  mem1 = request_memberid (request, info->member);

  /* calculate buffer length needed to store pattern */
  for (len=1, p1=info->pattern; *p1; p1++) {
    if (p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL)
        return CHECK_ERR_PATTERN;

      mem2 = request_membern (request, p1, (size_t) (p2 - p1));

      if (mem2)
        len += strlen (mem2);

    } else {
      len++;
    }
  }

  fprintf (stderr, "required length: %ld\n", len);

  if ((buf = malloc (len)) == NULL) {
    bail ("%s: malloc: %s", __func__, strerror (errno));
    return CHECK_ERR_SYSTEM;
  }

  /* create pattern */
  for (p1=info->pattern, p3=buf; *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL) /* unlikely */
        return CHECK_ERR_PATTERN;

      if ((mem2 = request_membern (request, p1, (size_t) (p2 - p1)))) {
        for (; *mem2; *p3++ = *mem2++)
          ;
      }

      p1 = p2;

    } else {
      *p3++ = *p1;
    }
  }

  *p3 = '\0';

  // DEBUG
  fprintf (stderr, "%s: should match: %s\n", __func__, buf);
  // /DEBUG


  regex = pcre_compile (buf, info->options, &errptr, &erroffset, NULL);

  if (regex == NULL) {
    bail ("%s: pcre_compile: compilation failed at offset %d: %s", __func__,
          erroffset, errptr);
  }
  
  ret = pcre_exec (regex, NULL, mem1, strlen (mem1), 0, 0, NULL, 0);

  pcre_free (regex);


  switch (ret) {
    case 0:
fprintf (stderr, "%s equals %s\n", mem1, buf);
  free (buf);
      score_update (score, info->match);
      break;
    case PCRE_ERROR_NOMATCH:
fprintf (stderr, "%s doesn't equal %s\n", mem1, buf);
  free (buf);
      score_update (score, info->nomatch);
      break;
    //case PCRE_ERROR_NOMEMORY:
    //  // XXX: something to handle error as temporary!
    //  error ();
    //  break;
    default:
      panic ("%s: a permanent error has occurred, this might indicate a bug",
             __func__);
  }

  return CHECK_SUCCESS;
}

