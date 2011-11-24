/* system includes */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "conf.h"
#include "map.h"
#include "map_priv.h"
#include "request.h"

/* definitions */
typedef struct _vt_map_list_cache vt_map_list_cache_t;

struct _vt_map_list_cache {
  float *results;
  int *maps;
  unsigned int nmaps;
};

/* prototypes */
void vt_map_list_cache_init (void);
void vt_map_list_cache_deinit (void *); /* invoked when thread exits */

static pthread_key_t vt_map_list_cache_key;
static pthread_once_t vt_map_list_init_done = PTHREAD_ONCE_INIT;


vt_map_t *
vt_map_create (cfg_t *sec, vt_error_t *err)
{
  vt_map_t *map;

  if (! (map = calloc (1, sizeof (vt_map_t))) ||
      ! (map->name = vt_cfg_titledup (sec)))
  {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  map->member = vt_request_mbrtoid (cfg_getstr (sec, "member"));

  return map;
FAILURE:
  vt_map_destroy (map);
  return NULL;
}

void
vt_map_destroy (vt_map_t *map)
{
  if (map) {
    if (map->data)
      free (map->data);
    if (map->name)
      free (map->name);
    free (map);
  }
}

int *
vt_map_lineup_create (const vt_map_list_t *list, cfg_t *sec, vt_error_t *err)
{
  char *name;
  int *lineup = NULL;
  int pos;
  int i, n;

  if ((n = cfg_size (sec, "maps"))) {
    if (! (lineup = calloc (n, sizeof (int)))) {
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: calloc: %s", __func__, strerror (errno));
      goto FAILURE;
    }

    for (i=0; i < n; i++) {
      name = cfg_getnstr (sec, "maps", i);
      if ((pos = vt_map_list_get_map_pos (list, name)) < 0) {
        vt_set_error (err, VT_ERR_BADCFG);
        vt_error ("%s: invalid option 'maps' with value '%s'", __func__, name);
        goto FAILURE;
      }
      lineup[i] = pos;
    }
    lineup[n] = -1;
  }

  return lineup;
FAILURE:
  if (lineup)
    free (lineup);
  return NULL;
}

vt_map_list_t *
vt_map_list_create (vt_error_t *err)
{
  int ret;
  vt_map_list_t *list = NULL;

  if (! (list = calloc (1, sizeof (vt_map_list_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    goto FAILURE;
  }
  if ((ret = pthread_rwlock_init (&list->lock, NULL)) != 0) {
    if (ret != ENOMEM)
      vt_panic ("%s: pthread_rwlock_init: %s", __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: pthread_rwlock_init: %s", __func__, strerror (ret));
    goto FAILURE;
  }

  return list;
FAILURE:
  (void)vt_map_list_destroy (list, 0, NULL);
  return NULL;
}

int
vt_map_list_destroy (vt_map_list_t *list, int maps, vt_error_t *err)
{
  int i;
  int ret;
  vt_error_t lerr;
  vt_map_t *map;

  if (list) {
    if ((ret = pthread_rwlock_destroy (&list->lock)) != 0)
      vt_panic ("%s: pthread_rwlock_destroy: %s", __func__, strerror (ret));

    if (maps && list->maps) {
      for (i=0; list->maps[i]; ) {
        map = list->maps[i];
        if (map->destroy_func (map, &lerr) == 0)
          i++; /* must be temporary error, or destroy_func should bail */
      }
    }

    if (list->maps)
      free (list->maps);
    free (list);
  }
  return 0;
}

int
vt_map_list_cache_reset (const vt_map_list_t *list, vt_error_t *err)
{
  float *results;
  int *maps, ret;
  vt_map_list_cache_t *cache;

  ret = pthread_once (&vt_map_list_init_done, vt_map_list_cache_init);
  if (ret != 0)
    vt_panic ("%s: ptrhead_once: %s", __func__, strerror (ret));
  if (! (cache = pthread_getspecific (vt_map_list_cache_key)))
    vt_panic ("%s: map cache unavailable", __func__);

  ret = pthread_rwlock_rdlock (&((vt_map_list_t *)list)->lock);
  if (ret != 0)
    vt_panic ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));

  if (list->nmaps > cache->nmaps) {
    if (! (maps = realloc (cache->maps, list->nmaps * sizeof (int))) ||
        ! (results = realloc (cache->results, list->nmaps * sizeof (float))))
    {
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: realloc: %s", __func__, strerror (errno));
      return -1;
    }
    cache->results = results;
    cache->maps = maps;
    cache->nmaps = list->nmaps;
  }

  memset (cache->results, 0, cache->nmaps * sizeof (float));
  memset (cache->maps, 0, cache->nmaps * sizeof (int));
  return 0;
}

int
vt_map_list_add_map (vt_map_list_t *list, const vt_map_t *map, vt_error_t *err)
{
  int ret;
  unsigned int nmaps;
  vt_map_t **maps;

  if (! (maps = realloc (list->maps, (list->nmaps+2) * sizeof (vt_map_t *)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: realloc: %s", __func__, strerror (errno));
    return -1;
  }

  maps[list->nmaps++] = (vt_map_t*)map;
  maps[list->nmaps] = NULL;
  list->maps = maps;

  return list->nmaps - 1;
}

int
vt_map_list_get_map_pos (const vt_map_list_t *list, const char *name)
{
  int i;

  for (i=0; i < list->nmaps; i++) {
    if ((list->maps)[i] && strcmp (name, (list->maps)[i]->name) == 0)
      return i;
  }

  return -1;
}

float
vt_map_list_search (const vt_map_list_t *list, int pos,
  const vt_request_t *request, vt_error_t *err)
{
  char *str;
  float res;
  size_t len;
  vt_error_t lerr;
  vt_map_t *map;
  vt_map_list_cache_t *cache;

  if (! (cache = pthread_getspecific (vt_map_list_cache_key)))
    vt_panic ("%s: map cache unavailable", __func__);
  if (pos < 0 || pos > cache->nmaps)
    vt_panic ("%s: invalid map position %d", __func__, pos);

  if (cache->maps[pos] == 0) {
    lerr = 0;
    map = list->maps[pos];

    if (! (str = vt_request_mbrbyid (request, map->member)))
      vt_panic ("%s: invalid request member %d", __func__, map->member);

    if (! (res = map->search_func (map, str, len, &lerr)) && lerr != 0) {
      vt_set_error (err, lerr);
      return 0.0;
    }

    cache->maps[pos] = res;
  }

  return cache->results[pos];
}

float
vt_map_list_evaluate (const vt_map_list_t *list, const int *lineup,
  const vt_request_t *req, vt_error_t *err)
{
  int ret;
  int *pos;
  float res;

  if (lineup) {
    for (pos=(int *)lineup; pos; pos++) {
      if ((res = vt_map_list_search (list, *pos, req, &ret)) || ret == 0) {
        return ret;
      } else {
        vt_set_error (err, ret);
        return 0.0;
      }
    }
  }

  return 0.0;
}

void
vt_map_list_lock (vt_map_list_t *list)
{
  int ret;

  if ((ret = pthread_rwlock_wrlock (&list->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));
}

void
vt_map_list_unlock (vt_map_list_t *list)
{
  int ret;

  if ((ret = pthread_rwlock_unlock (&list->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
}

void
vt_map_list_cache_init (void)
{
  int ret;
  vt_map_list_cache_t *cache;

  pthread_key_create (&vt_map_list_cache_key, vt_map_list_cache_deinit);

  if (! (cache = calloc (1, sizeof (vt_map_list_cache_t))))
    vt_error ("%s (%d): malloc0: %s",
      __func__, __LINE__, strerror (errno));
  if (cache && (ret = pthread_setspecific (vt_map_list_cache_key, cache)) != 0)
    vt_error ("%s (%d): pthread_setspecific: %s",
      __func__, __LINE__, strerror (ret));
}

void
vt_map_list_cache_deinit (void *ptr)
{
  vt_map_list_cache_t *cache = (vt_map_list_cache_t *)ptr;

  if (cache) {
    if (cache->results)
      free (cache->results);
    if (cache->maps)
      free (cache->maps);
    free (cache);
  }
}
