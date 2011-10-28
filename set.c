/* system includes */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "constants.h"
#include "set.h"
#include "map.h"
#include "request.h"
#include "utils.h"

/* definitions */
typedef struct vt_set_cache_struct vt_set_cache_t;

struct vt_set_cache_struct {
  int *maps;
  int nmaps;
};

/* prototypes */
void vt_set_cache_init_once (void);
int vt_set_cache_init (vt_set_t *);

/* constants */
pthread_key_t vt_set_cache_key;
pthread_once_t vt_set_init_done = PTHREAD_ONCE_INIT;

int
vt_set_create (vt_set_t **dest)
{
  int ret;
  vt_set_t *set;

  if (! (set = malloc0 (sizeof (vt_set_t)))) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }
  if ((ret = pthread_rwlock_init (&set->lock, NULL)) != 0) {
    vt_error ("%s: pthread_rwlock_init: %s", __func__, strerror (ret));
    return (ret == ENOMEM) ? VT_ERR_NOMEM : VT_ERR_SYSTEM;
  }

  *dest = set;
  return VT_SUCCESS;
}

int
vt_set_destroy (vt_set_t *set, int maps)
{
  // IMPLEMENT
}

int
vt_set_insert_map (int *id, vt_set_t *set, const vt_map_t *map)
{
  int ret, err;
  vt_map_t **maps;

  if (! (maps = realloc (set->maps, (set->nmaps + 2)))) {
    vt_error ("%s: realloc: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  maps[set->nmaps++] = (vt_map_t*)map;
  maps[set->nmaps] = NULL;
  set->maps = maps;

  return VT_SUCCESS;
}

int
vt_set_select_map_id (int *id, const vt_set_t *set, const char *name)
{
  int i;

  for (i=0; i < set->nmaps; i++) {
    if ((set->maps)[i] && strcmp (name, (set->maps)[i]->name) == 0) {
      *id = i;
      return VT_SUCCESS;
    }
  }

  *id = -1;
  return VT_ERR_INVAL;
}

int
vt_dict_lock (vt_set_t *set)
{
  int ret;

  if ((ret = pthread_rwlock_wrlock (&set->lock)) != 0) {
    vt_error ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (errno));
    return VT_ERR_SYSTEM;
  }

  return VT_SUCCESS;
}

int
vt_dict_unlock (vt_set_t *set)
{
  int ret;

  if ((ret = pthread_rwlock_unlock (&set->lock)) != 0) {
    vt_error ("%s: pthread_rwlock_unlock: %s", __func__, strerror (errno));
    return VT_ERR_SYSTEM;
  }

  return VT_SUCCESS;
}

void
vt_set_cache_init_once (void)
{
  int ret;
  vt_set_cache_t *cache;

  pthread_key_create (&vt_set_cache_key, vt_set_cache_deinit);

  if (! (cache = malloc0 (sizeof (vt_set_cache_t)))) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    return;
  }
  if ((ret = pthread_setspecific (vt_set_cache_key, cache))) {
    vt_error ("%s: pthread_setspecific: %s", __func__, strerror (errno));
    return;
  }
}

int
vt_set_cache_init (vt_set_t *set)
{
  int *maps, ret;
  vt_set_cache_t *cache;

  if ((ret = pthread_once (&vt_set_init_done, vt_set_cache_init_once)) != 0)
    vt_panic ("%s: ptrhead_once: %s", __func__, strerror (errno));
  if (! (cache = pthread_getspecific (vt_set_cache_key)))
    vt_panic ("%s: map cache unavailable");

  if ((ret = pthread_rwlock_rdlock (&set->lock)) != 0) {
    vt_error ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (errno));
    return VT_ERR_SYSTEM;
  }

  if (set->nmaps > cache->nmaps) {
    if (! (maps = realloc (cache->maps, set->nmaps))) {
      vt_error ("%s: realloc: %s", __func__, strerror (errno));
      return VT_ERR_NOMEM;
    }
    cache->maps = maps;
    cache->nmaps = set->nmaps;
  }

  memset (cache->maps, VT_MAP_RESP_EMPTY, cache->nmaps * sizeof (int));

  return VT_SUCCESS;
}

void
vt_set_cache_deinit (void *ptr)
{
  int ret;

  if ((ret = pthread_rwlock_unlock (&set->lock)) != 0) {
    vt_error ("%s: pthread_rwlock_unlock: %s", __func__, strerror (errno));
    return VT_ERR_SYSTEM;
  }

  return VT_SUCCESS;
}

int
vt_set_lookup (int *res, const vt_set_t *set, const int id, const vt_request_t *request)
{
  char *key;
  size_t len;
  vt_map_t *map;
  vt_set_cache_t *cache;

  if (! (cache = pthread_getspecific (vt_set_cache_key)))
    vt_panic ("%s: map cache unavailable", __func__);
  if (id < 0 || id > cache->nmaps)
    vt_panic ("%s: map identifier invalid", __func__);

  if (cache->maps[id] == VT_MAP_RESP_EMPTY) {
    map = set->maps[id];

    if ((key = vt_request_attrbyid (request, map->attrib)) == NULL)
      vt_panic ("%s: request attribute with id %d unavailable", __func__, map->attrib);
    if (map->lookup_func (res, map, key, len) != VT_SUCCESS)
      vt_panic ("%s: lookup failed... FIXME: fix this message");
    cache->maps[id] = *res;
  } else {
    vt_debug ("%s: cache result for map %s", __func__, map->name);
  }

  *res = cache->maps[id];
  return VT_SUCCESS;
}
