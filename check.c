/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check.h"
#include "utils.h"

/* prototypes to functions used only internally */
//slist_t *sort_by_priority (slist_t *);

check_t *
check_alloc (size_t nbytes)
{
  check_t *check;

  if ((check = malloc0 (sizeof (check_t))) == NULL)
    return NULL;

  if (nbytes > 0) {
    if ((check->info = malloc0 (nbytes)) == NULL) {
      free (check);
      return NULL;
    }
  }

  return check;
}

/**
 * COMMENT
 *
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
*/



/*
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

*/


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

//    if (strncmp (type, "string", 6) == 0)
//      check = cfg_to_check_str (section);
    if (strncmp (type, "dnsbl", 5) == 0)
      check = check_dnsbl_create (section);
//    else if (strncmp (type, "pcre", 5) == 0)
//      check = cfg_to_check_pcre (section);
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


































