#ifndef VT_SLIST_H_INCLUDED
#define VT_SLIST_H_INCLUDED

typedef int(*VT_SLIST_FREE_FUNC)(void *);
typedef int(*VT_SLIST_SORT_FUNC)(void *, void *);

typedef struct vt_slist_struct vt_slist_t;

struct vt_slist_struct {
  void *data;
  vt_slist_t *next;
};

#define vt_slist_append(list,data) vt_slist_insert((list),(data), -1)
#define vt_slist_prepend(list,data) vt_slist_insert((list),(data), 0)

vt_slist_t *vt_slist_alloc (void);
vt_slist_t *vt_slist_free (vt_slist_t *, VT_SLIST_FREE_FUNC, unsigned int);
vt_slist_t *vt_slist_insert (const vt_slist_t *, void *, int);
unsigned int vt_slist_length (const vt_slist_t *);
vt_slist_t *vt_slist_sort (const vt_slist_t *, VT_SLIST_SORT_FUNC);

#endif

