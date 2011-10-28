/* system includes */
#include <errno.h>
#include <pthread.h>
#include <string.h>

/* valiant includes */
#include "constants.h"
#include "map.h"
#include "utils.h"

int
vt_map_create (vt_map_t **dest,
               const char *title,
               const char *member,
               size_t size)
{
  int attrib, err;
  vt_map_t *map = NULL;

  if ((attrib = vt_request_attrtoid (member)) < 0) {
    vt_error ("%s: invalid member %s", __func__, member);
    err = VT_ERR_INVAL;
    goto FAILURE;
  }
  if (! (map = malloc0 (sizeof (vt_map_t))) ||
      ! (map->context = malloc0 (size)))
  {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  if (! (map->name = strdup (title))) {
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }

  map->attrib = attrib;

 *dest = map;
  return VT_SUCCESS;

FAILURE:
  if (map) {
    if (map->context)
      free (map->context);
  }
  return err;
}

int
vt_map_destroy (vt_map_t *map)
{
  if (map) {
    if (map->context)
      free (map->context);
    free (map);
  }

  return VT_SUCCESS;
}
