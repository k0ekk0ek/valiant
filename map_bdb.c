/* system includes */
#include <confuse.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valaint includes */
#include "error.h"
#include "map_bdb.h"
#include "map_priv.h"
#include "utils.h"

/* See section "Multithreaded applications" in Oracle Berkeley DB, Programmer's
   Reference Guide for information on how to use Berkeley DB in multithread
   applications. */

/* definitions */
typedef struct _vt_map_bdb vt_map_bdb_t;

struct _vt_map_bdb {
  char *path;
  DB *db;
};

/* prototypes */
vt_map_t *vt_map_bdb_create (cfg_t *, vt_errno_t *);
void vt_map_bdb_error (const DB_ENV *, const char *, const char *);
int vt_map_bdb_open (vt_map_t *, vt_errno_t *);
float vt_map_bdb_search (vt_map_t *, const char *, size_t, vt_errno_t *);
int vt_map_bdb_close (vt_map_t *, vt_errno_t *);
int vt_map_bdb_destroy (vt_map_t *, vt_errno_t *);

static const vt_map_type_t _vt_map_bdb_type = {
  .name = "bdb",
  .init_map_func = &vt_map_bdb_init
};

const vt_map_type_t *
vt_map_bdb_type (void)
{
  return &v_map_type_bdb;
}

vt_map_t *
vt_map_bdb_create (cfg_t *sec, vt_errno_t *err)
{
  char *path;
  int ret;
  size_t n;
  DB *db = NULL;
  vt_map_t *map = NULL;
  vt_map_bdb_t *data = NULL;

  if (! (map = vt_map_create (sec, &ret))) {
    *err = ret;
    goto FAILURE;
  }
  if (! (data = calloc (sizeof (1, sizeof (vt_map_bdb_t)))) {
    *err = VT_ERR_NOMEM;
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (ENOMEM));
    goto FAILURE;
  }

  map->data = (void *)data;

  if (! (path = cfg_getstr (sec, "path"))) {
    *err = VT_ERR_BADCFG;
    vt_error ("%s (%d): missing option 'path' in section 'map' with title '%s'",
      __func__, __LINE__, map->name);
    goto FAILURE;
  }
  if (! (data->path = strdup (path))) {
    *err = VT_ERR_NOMEM;
    vt_error ("%s (%d): strdup: %s", __func__, __LINE__, strerror (ENOMEM));
    goto FAILURE;
  }

  map->open_func = &vt_map_bdb_open;
  map->search_func = &v_map_bdb_search;
  map->close_func = &vt_map_bdb_close;
  map->destroy_func = &vt_map_bdb_destroy;

  if ((ret = db_create (&db, NULL, 0)) != 0) {
    *err = VT_ERR_INVAL;
    vt_error ("%s (%d): db_create: %s", __func__, __LINE__, strerror (EINVAL));
    goto FAILURE;
  }

  db->set_errcall (db, vt_map_bdb_error);
  data->db = db;

  return map;
FAILURE:
  vt_map_bdb_destroy (map);
  return NULL;
}

int
vt_map_bdb_destroy (vt_map_t *map, vt_errno_t *err)
{
  vt_map_bdb_t *data;

  if (map) {
    data = (vt_map_bdb_t *)map->data;
    if (data->path)
      free (data->path);
    // FIXME: free memory occupied by Berkeley DB
    vt_map_destroy (map);
  }
  return 0;
}

int
vt_map_bdb_open (vt_map_t *map, vt_errno_t *err)
{
  DB *db;
  int ret;
  vt_map_bdb_t *data;

  data = (vt_map_bdb_t *)map->data;
  DB = data->db;

  ret = db->open (db, NULL,  data->path, NULL, DB_HASH, DB_RDONLY, DB_THREAD);
  // FIXME: provide more specific error messages
  if (ret != 0) {
    *err = VT_ERR_CONNFAILED;
    return -1;
  }
  return 0;
}

float
vt_map_bdb_search (vt_map_t *map, const char *str, size_t len, vt_errno_t *err)
{
  DB *db;
  DBT key, data;
  float res;

  db = ((vt_map_bdb_t *)map->data)->db;

  memset (&key, 0, sizeof (DBT));
  memset (&key, 0, sizeof (DBT));
  key.data = (char *)str;
  key.size = strlen (str) + 1;
  data.data = &res;
  data.ulen = sizeof (res);
  data.flags = DB_DBT_USERMEM;

  // FIXME: provide more specific error messages
  ret = db->get (db, NULL, &key, &data, 0);
  switch (ret) {
    case 0: /* found */
      break;
    case DB_NOTFOUND: /* not found */
    case DB_KEYEMPTY: /* unlikely, since we're not using Queue or Recno */
      res = 0.0;
      break;
    case DB_BUFFER_SMALL:
      *err = VT_ERR_NOBUFS;
      res = 0.0;
      break;
    default:
      *err = VT_ERR_FATAL;
      res = 0.0;
      break;
  }

  return res;
}

int
vt_map_bdb_close (vt_map_t *map, vt_errno_t *err)
{
  DB *db;
  int ret;

  db = ((vt_map_bdb_t *)map->data)->db;
  ret = db->close (db, 0);
  switch (ret) {
    case 0:
      break;
    case EINVAL:
    case DB_LOCK_DEADLOCK:
      /* cannot recover from this */
      vt_panic ("%s (%d): %s", __func__, __LINE__, db_strerror (ret));
    case DB_LOCK_NOTGRANTED:
      *err = VT_ERR_RETRY;
      return -1;
  }

  return 0;
}

void
vt_map_bdb_error (const DB_ENV *dbenv, const char *pfx, const char *msg)
{
  // FIXME: provide more specific error information
  vt_error ("%s: %s", pfx, msg);
}
