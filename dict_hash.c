/* system includes */
#include <assert.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* valaint includes */
#include "dict_priv.h"
#include "dict_hash.h"
#include "request.h"
#include "state.h"

/* See section "Multithreaded applications" in Oracle Berkeley DB, Programmer's
   Reference Guide for information on how to use Berkeley DB in multithread
   applications. */

#define VT_DICT_HASH_TIME_DIFF (300)

typedef struct _vt_dict_hash vt_dict_hash_t;

struct _vt_dict_hash {
  vt_request_member_t member;
  char *path; /* full path to Berkeley DB database */
  int invert;
  float weight;
  vt_state_t state; /* generic structure to handle back off logic */
  int ready;
  time_t atime; /* time we last tried to access */
  time_t mtime; /* time of last data modification */
  pthread_rwlock_t lock;
  DB *db;
};

/* prototypes */
vt_dict_t *vt_dict_hash_create (vt_dict_type_t *, cfg_t *, cfg_t *,
  vt_error_t *);
int vt_dict_hash_destroy (vt_dict_t *, vt_error_t *);
int vt_dict_hash_check (vt_dict_t *, vt_request_t *, vt_result_t *, int,
  vt_error_t *);
int vt_dict_hash_open (vt_dict_hash_t *, vt_error_t *);
void vt_map_bdb_error (const DB_ENV *, const char *, const char *);

vt_dict_type_t _vt_dict_hash_type = {
  .name = "hash",
  .create_func = &vt_dict_hash_create
};

vt_dict_type_t *
vt_dict_hash_type (void)
{
  return &_vt_dict_hash_type;
}

vt_dict_t *
vt_dict_hash_create (vt_dict_type_t *type,
                     cfg_t *type_sec,
                     cfg_t *dict_sec,
                     vt_error_t *err)
{
  cfg_opt_t *opt;
  char *fmt;
  DB *db;
  int ret;
  vt_dict_t *dict;
  vt_dict_hash_t *data;

  assert (type);
  assert (dict_sec);

  if (! (dict = vt_dict_create_common (dict_sec, err)))
    goto failure;
  if (! (data = calloc (1, sizeof (vt_dict_hash_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  dict->data = (void *)data;

  /* initialize complex members */
  (void)vt_state_init (&data->state, 0, NULL);

  if ((ret = pthread_rwlock_init (&data->lock, NULL)) != 0) {
    fmt = "%s: pthread_rwlock_init: %s";
    if (ret != ENOMEM)
      vt_panic (fmt, __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error (fmt, __func__, strerror (ret));
    goto failure;
  }

  data->member = vt_request_mbrtoid (cfg_getstr (dict_sec, "member"));
  data->path = strdup (cfg_getstr (dict_sec, "path"));

  if (! data->path) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  if (cfg_getbool (dict_sec, "invert")) {
    data->invert = 1;
    data->weight = cfg_getfloat (dict_sec, "weight");
  } else {
    data->invert = 0;
    opt = cfg_getopt (dict_sec, "weight");
    if (opt && opt->nvalues && ! (opt->flags &CFGF_RESET))
      data->weight = cfg_opt_getnfloat (opt, 0);
  }

  // FIXME: implement "infinity"
  //dict->max_diff =
  //dict->min_diff =
  dict->check_func = &vt_dict_hash_check;
  dict->destroy_func = &vt_dict_hash_destroy;

  return dict;
failure:
  (void)vt_dict_hash_destroy (dict, NULL);
  return NULL;
}

int
vt_dict_hash_destroy (vt_dict_t *dict, vt_error_t *err)
{
  int ret;
  vt_dict_hash_t *data;

  if (dict) {
    if (dict->data) {
      data = (vt_dict_hash_t *)dict->data;
      if (data->path)
        free (data->path);
      if ((ret = pthread_rwlock_destroy (&data->lock)) != 0)
        vt_panic ("%s: pthread_rwlock_destroy: %s", __func__, strerror (ret));
      if (data->db) {
        if ((ret = data->db->close (data->db, 0)) != 0)
          vt_panic ("%s: DB->close: %s", __func__, db_strerror (ret));
      }
      free (data);
    }
    return vt_dict_destroy_common (dict, err);
  }
  return 0;
}

int
vt_dict_hash_open (vt_dict_hash_t *data, vt_error_t *err)
{
  DB *db;
  char *fmt;
  int loaddb, ret, res, wrlock;
  struct stat st_buf;
  time_t now;

  assert (data);

  now = time (NULL);
  wrlock = 0;
  res = -1;

again:
  if ((! data->db || ! data->ready) && ! vt_state_omit (&data->state))
    loaddb = 1;
  else
    loaddb = 0;

  if (loaddb || (data->atime + VT_DICT_HASH_TIME_DIFF) > now) {
    fmt = "%s: stat %s: %s";
    if (stat (data->path, &st_buf) == -1) {
      switch (errno) {
        case EACCES:
        case ELOOP:
        case ENOENT:
        case ENOTDIR:
          break;
        default:
          vt_panic (fmt, __func__, data->path, strerror (errno));
      }

      vt_set_error (err, VT_ERR_CONNFAILED);
      vt_error (fmt, __func__, data->path, strerror (errno));
      return -1;
    }

    if (! loaddb && data->mtime != st_buf.st_mtime)
      loaddb = 1;
  }

  if (loaddb) {
    if (! wrlock) {
      if ((ret = pthread_rwlock_unlock (&data->lock)) != 0)
        vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
      if ((ret = pthread_rwlock_wrlock (&data->lock)) != 0)
        vt_panic ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));
      wrlock = 1;
      goto again;
    }

    if (data->db && (ret = db->close (data->db, 0)) != 0)
      vt_panic ("%s: DB->close: %s", __func__, db_strerror (ret));

    data->db = NULL;
    data->ready = 0;

    if ((ret = db_create (&db, NULL, 0)) != 0) {
      fmt = "%s: db_create: %s";
      if (ret != ENOMEM)
        vt_panic (fmt, __func__, db_strerror (ret));
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error (fmt, __func__, db_strerror (ret));
      return -1;
    }

    data->db = db;

    switch ((ret = db->open (data->db, NULL, data->path, NULL, DB_HASH, DB_RDONLY, DB_THREAD))) {
      case 0:
        vt_info ("%s: DB->open %s", __func__, data->path);
        data->ready = 1;
        data->atime = time (NULL);
        data->mtime = st_buf.st_mtime;
        break;
      case ENOENT:
      case DB_OLD_VERSION:
        vt_set_error (err, VT_ERR_CONNFAILED);
        vt_state_error (&data->state);
        vt_error ("%s: DB->open %s: %s", __func__, data->path, db_strerror (ret));
        return -1;
      default:
        vt_panic ("%s: DB->open %s: %s", __func__, data->path, db_strerror (ret));
    }
  }

  return 0;
}

int
vt_dict_hash_check (vt_dict_t *dict,
                    vt_request_t *req,
                    vt_result_t *res,
                    int pos,
                    vt_error_t *err)
{
  char *member;
  DBT key, value;
  float weight;
  int ret;
  vt_dict_hash_t *data;
  vt_error_t tmperr;

  assert (dict);
  assert (req);
  assert (res);
  data = (vt_dict_hash_t *)dict->data;
  assert (data);

  if ((ret = pthread_rwlock_rdlock (&data->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));

  tmperr = 0;
  if (! (member = vt_request_mbrbyid (req, data->member))) {
    if (data->invert)
      weight = data->weight;
    else
      weight = 0.0;
    goto update;
  }

  if ((ret = vt_dict_hash_open (data, &tmperr)) != 0) {
    vt_set_error (err, tmperr);
    goto unlock;
  }

  memset (&key, 0, sizeof (DBT));
  memset (&value, 0, sizeof (DBT));
  key.data = (char *)member;
  key.size = strlen (member) + 1;
  value.data = &weight;
  value.ulen = sizeof (weight);
  value.flags = DB_DBT_USERMEM;

  switch ((ret = data->db->get (data->db, NULL, &key, &value, 0))) {
    case 0:
      if (data->invert)
        weight = 0.0;
      else if (data->weight)
        weight = data->weight;
      break;
    case DB_NOTFOUND:
    case DB_KEYEMPTY:
      if (data->invert)
        weight = data->weight;
      else
        weight = 0.0;
      break;
    case DB_BUFFER_SMALL:
      tmperr = VT_ERR_NOBUFS;
      vt_set_error (err, tmperr);
      weight = 0.0;
      break;
    default:
      vt_panic ("%s: DB->get %s: %s", __func__, data->path, db_strerror (ret));
  }

update:
  vt_result_update (res, pos, weight);
unlock:
  if ((ret = pthread_rwlock_unlock (&data->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
  return tmperr ? -1 : 0;
}

void
vt_map_bdb_error (const DB_ENV *dbenv, const char *prefix, const char *message)
{
  vt_error ("%s: %s", prefix, message);
}

