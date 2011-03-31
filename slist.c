/* system includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "slist.h"


slist_t *
slist_alloc (void)
{
  slist_t *list;

  if ((list = malloc (sizeof (slist_t))))
    memset (list, '\0', sizeof (slist_t));

  return list;
}

slist_t *
slist_free (slist_t *list, SLIST_FREE_FN free_data, unsigned int num)
{
  int every;
  slist_t *cur, *next;

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

//slist_t *
//slist_append (const slist_t *list, void *data)
//{
  // IMPLEMENT (less resource intensive)
//}

//slist_t *
//slist_prepend (const slist_t *list, void data)
//{
  // IMPLEMENT (less resource intensive)
//}

slist_t *
slist_insert (const slist_t *list, void *data, int pos)
{
  unsigned int i, n;
  slist_t *prev, *cur, *next;

  if (pos < 0) {
    pos *= -1; /* convert negative value into positive value */
    n = slist_length (list);
//fprintf (stderr, "%s: %d: %d == %d\n", __func__, __LINE__, pos, n);
    if ((unsigned int)pos > n)
      n = 0;
    else
      n = (n - (unsigned int)pos)+1;
  } else {
    n  = (unsigned int)pos;
  }

  if ((cur = slist_alloc ()) == NULL)
    return NULL;

  cur->data = data;

  if (list) {
//fprintf (stderr, "%s: %d, n is %d, string is %s\n", __func__, __LINE__, n, (char*)cur->data);
    for (prev=NULL, next=(slist_t*)list, i=0;
         next && i < n;
         prev=next, next=next->next, i++)
      fprintf (stderr, "%d: %s\n", i, (char*) next->data);

    cur->next = next;
    if (prev)
      prev->next = cur;
  }

  return cur;
}

unsigned int
slist_length (const slist_t *list)
{
  slist_t *cur;
  unsigned int i;

  for (cur=(slist_t*)list, i=0; cur; cur=cur->next, i++)
    ;

  return i;
}

slist_t *
slist_sort (const slist_t *list, SLIST_SORT_FN cmp)
{
  slist_t *root;
  slist_t *prev1, *cur1;
  slist_t *prev2, *cur2;
  slist_t *sub_prev, *sub;

  root = NULL;

  for (prev1=NULL, cur1=(slist_t*)list; cur1; prev1=cur1, cur1=cur1->next) {
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
