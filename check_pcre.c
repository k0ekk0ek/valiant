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
#include <pcre.h> /* support for Perl-compatible regular expressions */
#include <stdlib.h>
#include <string.h>

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

check_t *check_static_pcre_alloc (void);
check_t *check_dynamic_pcre_alloc (void);
void check_static_pcre_free (check_t *);
void check_dynamic_pcre_free (check_t *);
check_t *check_static_pcre_create (int, int, bool, int, const char *);
check_t *check_dynamic_pcre_create (int, int, bool, int, const char *);
int check_static_pcre (check_t *, request_t *, score_t *);
int check_dynamic_pcre (check_t *, request_t *, score_t *);


check_t *
check_static_pcre_alloc (void)
{
  check_t *check;
  check_info_static_pcre_t *info;

  if ((check = check_alloc ())) {
    info = malloc (sizeof (check_info_static_pcre_t));

    if (info) {
      memset (info, 0, sizeof (check_info_static_pcre_t));
      check->extra = (void *) info;
    } else {
      free (check);
      check = NULL;
    }
  }

  return check;
}


check_t *
check_dynamic_pcre_alloc (void)
{
  check_t *check;
  check_info_dynamic_pcre_t *info;

  if ((check = check_alloc ())) {
    info = malloc (sizeof (check_info_dynamic_pcre_t));

    if (info) {
      memset (info, 0, sizeof (check_info_dynamic_pcre_t));
      check->extra = (void *) info;
    } else {
      free (check);
      check = NULL;
    }
  }

  return check;
}


void
check_static_pcre_free (check_t *check)
{
  check_info_static_pcre_t *info;

  if (check) {
    if (check->extra) {
      info = (check_info_static_pcre_t *) check->extra;

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
    if (check->extra) {
      info = (check_info_dynamic_pcre_t *) check->extra;

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

  if (! (check = check_static_pcre_alloc ()))
    bail ("%s: %s", __func__, strerror (errno));

  info = (check_info_static_pcre_t *) check->extra;

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

  check->extra = info;
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

  const char *str;

  str = check_shorten_pattern (pattern);

  if (! (check = check_dynamic_pcre_alloc ()))
    bail ("%s: check_dynamic_pcre_alloc: %s", __func__, strerror (errno));

  info = (check_info_dynamic_pcre_t *) check->extra;
  info->match = match;
  info->nomatch = nomatch;
  info->member = member;
  info->pattern  = str;
  info->options = PCRE_UTF8;

  if (nocase)
    info->options |= PCRE_CASELESS;

  check->extra = (void *) info;
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

  info = (check_info_static_pcre_t *) check->extra;
  member = request_member (request, info->member);

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

  return 0;
}


int
check_dynamic_pcre (check_t *check, request_t *request, score_t *score)
{
  check_info_dynamic_pcre_t *info;

  char *buf, *mem, *p1, *p2, *p3;
  int memid;
  size_t len;

  char *error;
  int offset;
  int result;
  pcre *regex;

  info = (check_info_dynamic_pcre_t *) check->extra;

  for (len = 1, p1=info->pattern; *p1; p1++) {
    if (p2) {
      if (p1[0] == '%') {
        buf = request_member_id_len (p2, p1 - p2);
        len += strlen (buf);
        p2 = NULL;
      }
    } else {
      if (p1[0] == '%') {
        if (p1[1] == '%') {
          p1++;
          len++;
        } else {
          p2 = p1+1;
        }
      } else {
        len++;
      }
    }
  }

  // 1. first things first... decide how long our temporary buffer needs to
  // be...!
  // I need this because all strings need to be concatenated before we can
  // actually compile the regular expression!

  // now that we have the length... allocate the memory!
  //
  if (! (buf = malloc (len))) {
    panic ("%s: malloc: %s", __func__, strerror (errno));
    return 1;
  }

  memset (buf, '\0', len);


  /* copy request member values into pattern */
  for (p1=info->pattern, p2=NULL, p3=buf; *p1; ) {
    if (p1[0] == '%') {
      if (p1[1] == '%') {
       *p2++ = *p1++;
      } else {
        memid = strtol (p1, &p1, 0);
        mem = request_member (request, memid);

        // XXX: really should take into account . (dot) escaping here
        for (p3=mem; *p3; *p2++ = *p3++)
          ;
      }

      ++p1;
    } else {
     *p2++ = *p1++;
    }
  }

  /* compile pattern into regular expression */

  fprintf (stder, "%s: should match: %s", __func__, buf);
  regex = pcre_compile (buf, info->options, &error, &offset, NULL);

  if (! regex)
    bail ("%s: pcre_compile: compilation failed at offset %d: %s", __func__,
          offset, error);


  // XXX: we seem to be ok!
  mem = request_member (request, info->member);
  
  result = pcre_exec (regex, NULL, mem, strlen (mem),
                      0, 0, NULL, 0);

  pcre_free (regex);
  free (mem);

  switch (result) {
    case 0:
      score_update (score, info->match);
      break;
    case PCRE_ERROR_NOMATCH:
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

  return 0;
}


























