/* system includes */
#include <confuse.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* valaint includes */
#include "map.h"
#include "map_bdb.h"
#include "utils.h"
#include "valiant.h"

/* See section "Multithreaded applications" in Oracle Berkeley DB, Programmer's
   Reference Guide for information on how to use Berkeley DB in multithread
   applications. */

/* definitions */
typedef struct vt_map_bdb_struct vt_map_bdb_t;

struct vt_map_bdb_struct {
  char *path;
  DB *db;
};

/* prototypes */
int vt_map_bdb_create (vt_map_t **, const cfg_t *);
int vt_map_bdb_destroy (vt_map_t *);
void vt_map_bdb_error_handler (const DB_ENV *, const char *, const char *);
int vt_map_bdb_lookup (vt_map_result_t *, const vt_map_t *map, const char *, size_t);

const vt_map_type_t vt_map_type_bdb = {
  .name = "bdb",
  .create_map_func = &vt_map_bdb_create
};

const vt_map_type_t *
vt_map_bdb_type (void)
{
  return &vt_map_type_bdb;
}

int
vt_map_bdb_create (vt_map_t **dest, const cfg_t *sec)
{
  char *path;
  DB *db = NULL;
  int ret, err;
  vt_map_t *map = NULL;
  vt_map_bdb_t *data = NULL;

  if ((ret = vt_map_create (&map, sizeof (vt_map_bdb_t), sec)) != VT_SUCCESS) {
    err = ret;
    goto FAILURE;
  }
  if (! (path = cfg_getstr ((cfg_t *)sec, "path"))) {
    vt_panic ("%s: path unavailable for map %s", __func__, map->name);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }

  map->search_func = &vt_map_bdb_lookup;
  map->destroy_func = &vt_map_bdb_destroy;
  data = (vt_map_bdb_t *)map->data;

  if (! (data->path = strdup (path))) {
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }

  ret = db_create (&db, NULL, 0);
  if (ret != 0) {
    vt_error ("%s: db_create: error creating database", __func__);
    err = VT_ERR_MAP;
    goto FAILURE;
  }

  db->set_errcall (db, vt_map_bdb_error_handler);

  ret = db->open (db, NULL, data->path, NULL, DB_HASH, DB_RDONLY, DB_THREAD);
  if (ret != 0) {
    vt_error ("%s: db_open: error opening database %s", __func__, data->path);
    err = VT_ERR_MAP;
    goto FAILURE;
  }

  data->db = db;
  *dest = map;
  return VT_SUCCESS;

FAILURE:
  vt_map_bdb_destroy (map);
  return err;
}

int
vt_map_bdb_destroy (vt_map_t *map)
{
  vt_map_bdb_t *data;

  if (map) {
    data = (vt_map_bdb_t *)map->data;
    if (data->db)
      data->db->close (data->db, 0);
    if (data->path)
      free (data->path);
    vt_map_destroy (map);
  }

  return VT_SUCCESS;
}

int
vt_map_bdb_lookup (vt_map_result_t *res, const vt_map_t *map, const char *str,
  size_t len)
{
  DB *db;
  DBT key, data;
  vt_map_result_t tmp;

  db = ((vt_map_bdb_t *)map->data)->db;

  memset (&key, 0, sizeof (DBT));
  memset (&key, 0, sizeof (DBT));
  key.data = (char *)str;
  key.size = strlen (str) + 1;
  data.data = &tmp;
  data.ulen = sizeof (tmp);
  data.flags = DB_DBT_USERMEM;

fprintf (stderr, "%s (%d): key: %s\n", __func__, __LINE__, str);

  if (db->get (db, NULL, &key, &data, 0) == DB_NOTFOUND) {
    *res = VT_MAP_RESULT_DUNNO;
  } else if (tmp == VT_MAP_RESULT_ACCEPT || tmp == VT_MAP_RESULT_REJECT) {
    *res = tmp;
  } else {
    *res = VT_MAP_RESULT_DUNNO;
  }

fprintf (stderr, "%s (%d): result %d\n", __func__, __LINE__, *res);

  return VT_SUCCESS;
}

void
vt_map_bdb_error_handler (const DB_ENV *dbenv,
                          const char *prefix,
                          const char *message)
{
  // FIXME: might need to add more information, but it will do for now
  vt_error ("%s: %s", __func__, message);
}

