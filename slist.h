#ifndef SLIST_H_INCLUDED
#define SLIST_H_INCLUDED

typedef int(*SLIST_FREE_FN)(void *);
typedef int(*SLIST_SORT_FN)(void *, void *);

typedef struct slist_struct slist_t;

struct slist_struct {
  void *data;
  slist_t *next;
};

#define slist_append(list,data) slist_insert((list),(data), -1)
#define slist_prepend(list,data) slist_insert((list),(data), 0)

slist_t *slist_alloc (void);
slist_t *slist_free (slist_t *, SLIST_FREE_FN, unsigned int);
slist_t *slist_insert (const slist_t *, void *, int);
unsigned int slist_length (const slist_t *);
slist_t *slist_sort (const slist_t *, SLIST_SORT_FN);

#endif

