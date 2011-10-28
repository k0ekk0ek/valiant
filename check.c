/* system includes */
#include <confuse.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check.h"
#include "utils.h"

vt_check_t *
vt_check_alloc (size_t n, const char *s)
{
  vt_check_t *check;

  if ((check = malloc0 (sizeof (vt_check_t))) == NULL)
    return NULL;

  if ((check->name = strdup (s)) == NULL) {
    free (check);
    return NULL;
  }

  if (n > 0) {
    if ((check->info = malloc0 (n)) == NULL) {
      free (check->name);
      free (check);
      return NULL;
    }
  }

  return check;
}

int
vt_check_weight (float weight)
{
  return (int)(weight*100);
}

int
vt_check_sort (void *p1, void *p2)
{
  vt_check_t *c1, *c2;

  c1 = (vt_check_t *)p1;
  c2 = (vt_check_t *)p2;

  if (c1->prio > c2->prio)
    return -1;
  if (c1->prio < c2->prio)
    return  1;

  return 0;
}

int
vt_check_dynamic_pattern (const char *s)
{
  char *p;

  for (p=(char*)s; *p; p++) {
    if (*p == '%' && *++p != '%')
      return 1;
  }

  return 0;
}

char *
vt_check_unescape_pattern (const char *s1)
{
  char *s2, *p1, *p2;
  size_t n;

  n = strlen (s1);

  if ((s2 = malloc (n+1)) == NULL)
    return NULL;

  for (p1=(char*)s1, p2=s2; *p1;) {
    *p2++ = *p1;

    if (*p1++ == '%' && *p1 == '%')
      ++p1;
  }

 *p2 = '\0';

  return s2;
}
