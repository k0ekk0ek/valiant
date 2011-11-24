/* system includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "slist.h"
#include "utils.h"

vt_slist_t *
vt_slist_alloc (void)
{
  return (vt_slist_t *)calloc (1, sizeof (vt_slist_t));
}

vt_slist_t *
vt_slist_free (vt_slist_t *list, VT_SLIST_FREE_FUNC free_data, unsigned int num)
{
  int every;
  vt_slist_t *cur, *next;

  if (list) {
    every = num ? 0 : 1;

    for (cur=list, next=cur->next; cur && (every || num--); cur=next, next=cur->next) {
      if (free_data)
        free_data (cur->data);
      else if (cur->data)
        free (cur->data);
      free (cur);
    }
  } else {
    cur = NULL;
  }

  return cur;
}

vt_slist_t *
vt_slist_insert (const vt_slist_t *list, void *data, int pos)
{
  unsigned int i, n;
  vt_slist_t *prev, *cur, *next;

  if (pos < 0) {
    pos *= -1; /* convert negative value into positive value */
    n = vt_slist_length (list);

    if ((unsigned int)pos > n)
      n = 0;
    else
      n = (n - (unsigned int)pos)+1;
  } else {
    n  = (unsigned int)pos;
  }

  if ((cur = vt_slist_alloc ()) == NULL)
    return NULL;

  cur->data = data;

  if (list) {
    for (prev=NULL, next=(vt_slist_t*)list, i=0;
         next && i < n;
         prev=next, next=next->next, i++)
      ;

    cur->next = next;
    if (prev)
      prev->next = cur;
  }

  return cur;
}

unsigned int
vt_slist_length (const vt_slist_t *list)
{
  vt_slist_t *cur;
  unsigned int i;

  for (cur=(vt_slist_t*)list, i=0; cur; cur=cur->next, i++)
    ;

  return i;
}

vt_slist_t *
vt_slist_sort (const vt_slist_t *list, VT_SLIST_SORT_FUNC cmp)
{
  vt_slist_t *root;
  vt_slist_t *prev1, *cur1;
  vt_slist_t *prev2, *cur2;
  vt_slist_t *sub_prev, *sub;

  root = NULL;

  for (prev1=NULL, cur1=(vt_slist_t*)list; cur1; prev1=cur1, cur1=cur1->next) {
    sub_prev = NULL;
    sub = cur1;
    for (prev2=cur1, cur2=cur1->next; cur2; prev2=cur2, cur2=cur2->next) {
      if (cmp (sub, cur2) > 0) {
        sub_prev = prev2;
        sub = cur2;
      }
    }

    if (root == NULL)
      root = sub;

    if (sub != cur1) {
      if (sub_prev)
        sub_prev->next = sub->next;
      if (prev1)
        prev1->next = sub;
      sub->next = cur1;
      cur1 = sub;
    }
  }

  return root;
}
