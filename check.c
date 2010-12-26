/* system includes */

#include <confuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* valiant includes */

#include "check.h"
#include "check_str.h"


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
    for (cur=list; cur->next; cur = cur->next)
      ;

    if (! (cur->next = checklist_alloc ()))
      return NULL;

    cur = cur->next;
  }

  cur->check=chk;
  cur->next=NULL;

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

    if (strncmp (type, "str", 3) == 0)
      check = cfg_to_check_str (section);
    else
      check = NULL; // XXX: I think it's better to panic here... or something!


    cur = checklist_append (cur, check);
  }

  return root;
}

