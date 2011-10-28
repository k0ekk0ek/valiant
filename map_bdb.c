/* system includes */
#include <confuse.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* valaint includes */
#include "constants.h"
#include "map.h"
#include "map_bdb.h"
#include "utils.h"

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
int vt_map_bdb_lookup (int *, const vt_map_t *map, const char *, size_t);

/* constants */
const cfg_opt_t vt_map_type_bdb_opts[] = {
  CFG_STR ("type", 0, CFGF_NODEFAULT),
  CFG_STR ("member", 0, CFGF_NODEFAULT),
  CFG_STR ("path", 0, CFGF_NODEFAULT),
  CFG_END ()
};

const vt_map_type_t vt_map_type_bdb = {
  .name = "bdb",
  .opts = vt_map_type_bdb_opts,
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
  char *member, *path, *title;
  DB *db = NULL;
  int ret, err;
  vt_map_t *map;
  vt_map_bdb_t *context = NULL;

  if (! (title = (char *)cfg_title ((cfg_t *)sec)))
    vt_panic ("%s: map title unavailable", __func__);
  if (! (member = cfg_getstr ((cfg_t *)sec, "member")))
    vt_panic ("%s: member unavailable for %s", __func__, title);
  if (! (path = cfg_getstr ((cfg_t *)sec, "path")))
    vt_panic ("%s: path unavailable for %s", __func__, title);

  ret = vt_map_create (&map, title, member, sizeof (vt_map_bdb_t));
  if (ret != VT_SUCCESS)
    return ret;

  map->lookup_func = &vt_map_bdb_lookup;
  map->destroy_func = &vt_map_bdb_destroy;
  context = (vt_map_bdb_t *)map->context;

  if (! (context->path = strdup (path))) {
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

  ret = db->open (db, NULL, context->path, NULL, DB_HASH, DB_RDONLY, DB_THREAD);

  if (ret != 0) {
    vt_error ("%s: db_open: error opening database %s", __func__, context->path);
    err = VT_ERR_MAP;
    goto FAILURE;
  }

  context->db = db;
  *dest = map;
  return VT_SUCCESS;

FAILURE:
  vt_map_bdb_destroy (map);
  return err;
}

int
vt_map_bdb_destroy (vt_map_t *map)
{
  vt_map_bdb_t *context;

  if (map) {
    context = (vt_map_bdb_t *)map->context;
    if (context->db)
      context->db->close (context->db, 0);
    if (context->path)
      free (context->path);
    vt_map_destroy (map);
  }

  return VT_SUCCESS;
}

int
vt_map_bdb_lookup (int *res, const vt_map_t *map, const char *str, size_t len)
{
  DB *db;
  DBT key;

  db = ((vt_map_bdb_t *)map->context)->db;

  memset (&key, 0, sizeof (DBT));
  key.data = (char *)str;
  key.size = strlen (str) + 1;

  if (db->exists (db, NULL, &key, 0) == DB_NOTFOUND)
    *res = VT_MAP_RESP_NOT_FOUND;
  else
    *res = VT_MAP_RESP_FOUND;

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
