/* system includes */
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* valiant includes */
#include "valiant.h"
#include "map.h"
#include "request.h"
#include "utils.h"

/* definitions */
typedef struct vt_map_list_cache_struct vt_map_list_cache_t;

struct vt_map_list_cache_struct {
  int *maps;
  int nmaps;
};

/* prototypes */
void vt_map_list_cache_init (void);
void vt_map_list_cache_deinit (void *); /* invoked when thread exits */

/* constants */
pthread_key_t vt_map_list_cache_key;
pthread_once_t vt_map_list_init_done = PTHREAD_ONCE_INIT;

int
vt_map_create (vt_map_t **dest, size_t size, const cfg_t *sec)
{
  char *title, *mbr;
  int err, ret;
  vt_map_t *map = NULL;
  vt_request_member_t mbrid;

  if (! (title = cfg_title ((cfg_t *)sec))) {
    vt_error ("%s: map title unavailable", __func__);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }
  if (! (mbr = cfg_getstr ((cfg_t *)sec, "member"))) {
    vt_error ("%s: member unavailable for map %s", __func__, title);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }
  if ((ret = vt_request_mbrtoid (&mbrid, mbr)) != VT_SUCCESS) {
    vt_error ("%s: invalid member %s in map %s", __func__, mbr, title);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }
  if (! (map = malloc0 (sizeof (vt_map_t))) || ! (map->data = malloc0 (size))) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  if (! (map->name = strdup (title))) {
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }

  map->member = mbrid;

 *dest = map;
  return VT_SUCCESS;

FAILURE:
  vt_map_destroy (map);
  return err;
}

int
vt_map_destroy (vt_map_t *map)
{
  if (map) {
    if (map->data)
      free (map->data);
    free (map);
  }
  return VT_SUCCESS;
}

int
vt_map_ids_create (vt_map_id_t **dest, const vt_map_list_t *maps,
  const cfg_t *sec)
{
  vt_map_id_t *maps = NULL;
  char *map;
  int i, id, n;

  if ((n = cfg_size ((cfg_t *)sec, "maps"))) {
    if (! (maps = malloc0 (sizeof (int) * (n + 1)))) {
      vt_error ("%s: malloc0: %s", __func__, strerror (errno));
      err = VT_ERR_NOMEM;
      goto FAILURE;
    }

    for (i=0; i < n; i++) {
      map = cfg_getnstr ((cfg_t *)sec, "maps", i);
      if ((ret = vt_map_list_get_map_id (&id, maps, map)) != VT_SUCCESS) {
        vt_error ("%s: map %s not in map list", __func__, name);
        err = ret;
        goto FAILURE;
      }
      maps[i] = id;
    }
    maps[n] = (vt_map_id_t)-1;
  } else {
    maps = NULL;
  }

  *dest = maps;
  return VT_SUCCESS;

FAILURE:
  if (maps)
    free (maps);
  return err;
}

int
vt_set_create (vt_map_list_t **dest)
{
  int err, ret;
  vt_map_list_t *list = NULL;

  if (! (list = malloc0 (sizeof (vt_map_list_t)))) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  if ((ret = pthread_rwlock_init (&list->lock, NULL)) != 0) {
    vt_error ("%s: pthread_rwlock_init: %s", __func__, strerror (ret));
    if (ret == ENOMEM)
      err = VT_ERR_NOMEM;
    else
      err = VT_ERR_SYS;
    goto FAILURE;
  }

  *dest = list;
  return VT_SUCCESS;
FAILURE:
  vt_map_list_destroy (list, false);
  return err;
}

int
vt_map_list_destroy (vt_map_list_t *list, bool maps)
{
  int i;
  int ret;

  if (list) {
    if ((ret = pthread_rwlock_destroy (&list->lock)) != 0) {
      vt_error ("%s: pthread_rwlock_destroy: %s", __func__, strerrno (ret));
      return VT_ERR_SYS;
    }
    if (maps && list->maps) {
      for (i=0; list->maps[i]; i++) {
        if ((ret = list->maps[i]->destroy_func (list->maps[i])) != VT_SUCCESS)
          return ret;
      }
    }
    if (list->maps) {
      free (list->maps);
    }
    free (list);
  }

  return VT_SUCCESS;
}

int
vt_map_list_cache_reset (const vt_map_list_t *list)
{
  int *maps, ret;
  vt_map_list_cache_t *cache;

  ret = pthread_once (&vt_map_list_init_done, vt_map_list_cache_init);
  if (ret != 0)
    vt_panic ("%s: ptrhead_once: %s", __func__, strerror (ret));
  if (! (cache = pthread_getspecific (vt_map_list_cache_key)))
    vt_panic ("%s: map cache unavailable");

  ret = pthread_rwlock_rdlock (&list->lock);
  if (ret != 0) {
    vt_error ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));
    return VT_ERR_SYS;
  }

  if (list->nmaps > cache->nmaps) {
    if (! (maps = realloc (cache->maps, list->nmaps))) {
      vt_error ("%s: realloc: %s", __func__, strerror (errno));
      return VT_ERR_NOMEM;
    }
    cache->maps = maps;
    cache->nmaps = list->nmaps;
  }

  memset (cache->maps, VT_MAP_RESULT_EMPTY, cache->nmaps * sizeof (int));
  return VT_SUCCESS;
}

int
vt_map_list_set_map (vt_map_id_t *id, vt_map_list_t *list, const vt_map_t *map)
{
  int ret, err;
  unsigned int nmaps;
  vt_map_t **maps;

  if (! (maps = realloc (list->maps, (list->nmaps + 2)))) {
    vt_error ("%s: realloc: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  maps[list->nmaps++] = (vt_map_t*)map;
  maps[list->nmaps] = NULL;
  list->maps = list;
  *id = list->nmaps - 1;

  return VT_SUCCESS;
}

int
vt_map_list_get_map_id (vt_map_id_t *id, const vt_map_list_t *list,
  const char *name)
{
  vt_map_id_t i;

  for (i=0; i < list->nmaps; i++) {
    if ((list->maps)[i] && strcmp (name, (list->maps)[i]->name) == 0) {
      *id = i;
      return VT_SUCCESS;
    }
  }

  *id = (vt_map_id_t)-1;
  return VT_ERR_INVAL;
}

int
vt_map_list_search (vt_map_result_t *res, const vt_map_list_t *list,
  const vt_map_id_t id, const vt_request_t *request)
{
  char *key;
  size_t len;
  vt_map_t *map;
  vt_map_list_cache_t *cache;

  if (! (cache = pthread_getspecific (vt_map_list_cache_key)))
    vt_panic ("%s: map cache unavailable", __func__);
  if (id < 0 || id > cache->nmaps)
    vt_panic ("%s: invalid map id", __func__);

  if (cache->maps[id] == VT_MAP_RESP_EMPTY) {
    map = list->maps[id];

    if ((key = vt_request_mbrbyid (request, map->member)) == NULL)
      vt_panic ("%s: request member with id %d unavailable", __func__,
        map->member);
    if (map->lookup_func (res, map, key, len) != VT_SUCCESS)
      vt_panic ("%s: lookup failed... FIXME: fix this message");
    cache->maps[id] = *res;
  } else {
    vt_debug ("%s: cache result for map %s", __func__, map->name);
  }

  *res = cache->maps[id];
  return VT_SUCCESS;
}

int
vt_map_list_evaluate (vt_map_result_t *res, const vt_map_list_t *list,
  const vt_map_id_t *maps, const vt_request_t *request)
{
  int ret;
  vt_map_result_t tmp = VT_MAP_RESULT_DUNNO;
  vt_map_id_t *map;

  if (maps) {
    for (map=(vt_map_id_t *)maps; tmp == VT_MAP_RESULT_DUNNO && map; map++) {
      if ((ret = vt_map_list_search (&tmp, list, *map, request)) != VT_SUCCESS)
        return ret;
    }
  }

  *res = tmp;
  return VT_SUCCESS;
}

int
vt_map_list_lock (vt_map_list_t *list)
{
  int ret;

  if ((ret = pthread_rwlock_wrlock (&list->lock)) != 0) {
    vt_error ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));
    return VT_ERR_SYS;
  }

  return VT_SUCCESS;
}

int
vt_map_list_unlock (vt_map_list_t *list)
{
  int ret;

  if ((ret = pthread_rwlock_unlock (&list->lock)) != 0) {
    vt_error ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
    return VT_ERR_SYS;
  }

  return VT_SUCCESS;
}

void
vt_map_list_cache_init (void)
{
  int ret;
  vt_map_list_cache_t *cache;

  pthread_key_create (&vt_map_list_cache_key, vt_map_list_cache_deinit);

  if (! (cache = malloc0 (sizeof (vt_map_list_cache_t)))) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    return;
  }
  if ((ret = pthread_setspecific (vt_map_list_cache_key, cache))) {
    vt_error ("%s: pthread_setspecific: %s", __func__, strerror (ret));
    return;
  }
}

void
vt_map_list_cache_deinit (void *ptr)
{
  vt_map_list_cache_t *cache = (vt_map_list_cache_t *)ptr;

  if (cache) {
    if (cache->maps)
      free (cache->maps);
    free (cache);
  }
}
