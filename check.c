/* system includes */

#include <confuse.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* valiant includes */

#include "check.h"
#include "check_str.h"
#include "check_dnsbl.h"


check_t *
check_alloc (void)
{
  check_t *chk;

  if ((chk = malloc (sizeof (check_t))))
    memset (chk, 0, sizeof (check_t));

  return chk;
}


checklist_t *
checklist_alloc (void)
{
  checklist_t *list;

  if ((list = malloc (sizeof (checklist_t))))
    memset (list, 0, sizeof (checklist_t));

  return list;
}


checklist_t *
checklist_append (checklist_t *list, check_t *chk)
{
  checklist_t *cur;

  if (! list->check) {
    cur = list;

  } else {
    for (cur=list; cur->next; cur=cur->next)
      ;

    if (! (cur->next = checklist_alloc ()))
      return NULL;

    cur = cur->next;
  }

  cur->check = chk;
  cur->next = NULL;

g_printf ("%s: appended\n", __func__);

  return cur;
}


/**
 * COMMENT
 */
check_t *
cfg_to_check_str (cfg_t *section)
{
  check_t *check;

  char *attribute;
  char *format;
  int positive;
  int negative;
  bool fold;

  if (! (attribute = cfg_getstr (section, "attribute")))
    panic ("%s: attribute undefined\n", __func__);
  if (! (format = cfg_getstr (section, "format")))
    panic ("%s: format undefined\n", __func__);

  attribute = strdup (attribute);
  format = strdup (format);

  positive = (int) (cfg_getfloat (section, "positive") * 100);
  negative = (int) (cfg_getfloat (section, "negative") * 100);
  fold = cfg_getbool (section, "case-sensitive");

  g_printf ("%s: attribute: %s, format: %s, positive: %d, negative: %d, fold: %s\n",
  __func__,
  attribute,
  format,
  positive,
  negative,
  fold ? "true" : "false");

  return check_str_create (positive, negative, attribute, format, fold);
}


/**
 * COMMENT
 */
check_t *
cfg_to_check_dnsbl (cfg_t *section)
{
  check_t *check;

//  char *attribute;
  char *zone;
  int positive;
  int negative;
//  bool fold;

//  if (! (attribute = cfg_getstr (section, "attribute")))
//    panic ("%s: attribute undefined\n", __func__);
  if (! (zone = cfg_getstr (section, "zone")))
    panic ("%s: zone undefined\n", __func__);

  //attribute = strdup (attribute);
  zone = strdup (zone);

  positive = (int) (cfg_getfloat (section, "positive") * 100);
  negative = (int) (cfg_getfloat (section, "negative") * 100);
//  fold = cfg_getbool (section, "case-sensitive");

  g_printf ("%s: zone: %s, positive: %d, negative: %d\n",
  __func__,
  zone,
  positive,
  negative);

  return check_dnsbl_create (positive, negative, zone);
}


check_t *
cfg_to_check_pcre (cfg_t *section)
{
  check_t *check;

  char *attribute;
  char *format;
  int positive;
  int negative;
  bool fold;

  if (! (attribute = cfg_getstr (section, "attribute")))
    panic ("%s: attribute undefined\n", __func__);
  if (! (format = cfg_getstr (section, "format")))
    panic ("%s: format undefined\n", __func__);

  attribute = strdup (attribute);
  format = strdup (format);

  positive = (int) (cfg_getfloat (section, "positive") * 100);
  negative = (int) (cfg_getfloat (section, "negative") * 100);
  fold = cfg_getbool (section, "case-sensitive");

  g_printf ("%s: attribute: %s, format: %s, positive: %d, negative: %d, fold: %s\n",
  __func__,
  attribute,
  format,
  positive,
  negative,
  fold ? "true" : "false");

  int member = request_member_id (attribute);

  return check_pcre_create (positive, negative, fold, member, format);
}


checklist_t *
cfg_to_checklist (cfg_t *main)
{
  checklist_t *root, *cur;
  check_t *check;
  cfg_t *section;
  char *type;
  int i, n;

  n=cfg_size (main, "check");

  root = checklist_alloc ();
  cur = root;

//printf ("%s: checks: %d\n", __func__, n);

  for (i=0, n=cfg_size (main, "check"); i < n; i++) {

    section = cfg_getnsec (main, "check", i);
    // XXX: implement error handling

    // first I need the type... thats important!
    // use cfg_getstr
    if (! (type = cfg_getstr (section, "type")))
      panic ("%s: type not specified", __func__); //XXX: include section in error report

//    printf ("%s: type is %s\n", __func__, type);

    if (strncmp (type, "string", 6) == 0)
      check = cfg_to_check_str (section);
    else if (strncmp (type, "dnsbl", 5) == 0)
      check = cfg_to_check_dnsbl (section);
    else if (strncmp (type, "pcre", 5) == 0)
      check = cfg_to_check_pcre (section);
    else
      check = NULL; // XXX: I think it's better to panic here... or something!


    cur = checklist_append (cur, check);
  }

  return root;
}

/**
 * COMMENT
 */
check_arg_t *
check_arg_create (check_t *check, request_t *request, score_t *score)
{
  check_arg_t *task;

  if ((task = malloc (sizeof (check_arg_t)))) {
    task->check = check;
    task->request = request;
    task->score = score;
  }

  return task;
}


/**
 * COMMENT
 */
bool
check_dynamic_pattern (const char *pattern)
{
  //
}


/**
 * Replace member names with numerical identifiers to speed up processing when
 * checks are actually being done.
 */
const char *
check_shorten_pattern (const char *pattern)
{
  char *buf, *p1, *p2, *p3;
  int id;
  size_t len;

  /*
   * Numerical representation of member always takes less space than string
   * respresentation. So it's safe for us to allocate the same number of bytes.
   */
  len = strlen (pattern);

  if ((buf = malloc (len+1)))
    memset (buf, 0, len+1);
  else
    bail ("%s: malloc: %s", __func__, strerror (errno));

  for (p1=(char *)pattern, p2=buf, p3=NULL; *p1; ) {
    if (p3) {
      if (p1[0] == '%') {
        len = (size_t) (p1 - p3);
        id = request_member_id_len (p3, len);

        if (id < 0)
          panic ("%s: invalid member %*s", __func__, len, p3);
        if (sprintf (p2, "%%%i%%", id) < 0)
          panic ("%s: sprintf: %s", __func__, strerror (errno));

        p3 = NULL;
      }
      
    } else {
      if (p1[0] == '%' && p1[1] != '%')
        p3 = ++p1;
      else
       *p2++ = *p1++;
    }
  }

  return buf;
}


const char *
check_unescape_pattern (const char *pattern)
{
  char *buf, *p1, *p2;
  size_t len;

  len = strlen (pattern);

  if (! (buf = malloc (len+1)))
    return NULL;

  memset (buf, '\0', len+1);

  for (p1=(char *)pattern; *p1; p1++) {
    if (p1[0] == '%' && p1[1] != '%')
      *p2++ = *p1;
  }

  return buf;
}





























