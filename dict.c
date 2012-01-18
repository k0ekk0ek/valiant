/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "dict.h"
#include "dict_priv.h"
#include "error.h"

typedef struct _vt_async_dict_arg vt_async_dict_arg_t;

struct _vt_async_dict_arg {
  vt_request_t *request;
  vt_result_t *result;
  int pos;
};

/* prototypes */
vt_dict_t *vt_async_dict_create (vt_dict_t *, vt_dict_type_t *, cfg_t *,
  cfg_t *, vt_error_t *);
int vt_async_dict_check (vt_dict_t *, vt_request_t *, vt_result_t *, int,
  vt_error_t *);
void vt_async_dict_worker (void *, void *);
int vt_async_dict_destroy (vt_dict_t *, vt_error_t *);

vt_dict_t *
vt_dict_create (vt_dict_type_t *type,
                cfg_t *type_sec,
                cfg_t *dict_sec,
                vt_error_t *err)
{
  vt_dict_t *async_dict, *dict;

  assert (type);
  assert (dict_sec);

  if (! (dict = type->create_func (type, type_sec, dict_sec, err)))
    goto failure;
  if (dict->async) {
    if (! (async_dict = vt_async_dict_create (dict, type, type_sec, dict_sec, err)))
      goto failure;
    dict = async_dict;
  }

  return dict;
failure:
  (void)dict->destroy_func (dict, NULL);
  return NULL;
}

vt_dict_t *
vt_dict_create_common (cfg_t *sec, vt_error_t *err)
{
  char *title;
  vt_dict_t *dict;

  assert (sec);

  if (! (dict = calloc (1, sizeof (vt_dict_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  title = (char *)cfg_title (sec);

  if (! (dict->name = strdup (title))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    goto failure;
  }

  return dict;
failure:
  (void)vt_dict_destroy_common (dict, NULL);
  return NULL;
}

int
vt_dict_destroy_common (vt_dict_t *dict, vt_error_t *err)
{
  if (dict) {
    if (dict->name)
      free (dict->name);
    free (dict);
  }
  return 0;
}

vt_dict_t *
vt_async_dict_create (vt_dict_t *dict,
                      vt_dict_type_t *type,
                      cfg_t *type_sec,
                      cfg_t *dict_sec,
                      vt_error_t *err)
{
  int nthreads;
  int nidle_threads;
  int ntasks;
  vt_dict_t *async_dict;
  vt_thread_pool_t *pool;

  assert (dict);
  assert (type);
  assert (dict_sec);

  if (! (async_dict = vt_dict_create_common (dict_sec, err)))
    goto failure;
  async_dict->async = 1;
  async_dict->check_func = &vt_async_dict_check;
  async_dict->destroy_func = &vt_async_dict_destroy;

  nthreads = cfg_getint (dict_sec, "max_threads");
  if (! nthreads)
    nthreads = cfg_getint (type_sec, "max_threads");
  nidle_threads = cfg_getint (dict_sec, "max_idle_threads");
  if (! nidle_threads)
    nidle_threads = cfg_getint (type_sec, "max_idle_threads)");
  ntasks = cfg_getint (dict_sec, "max_queued");
  if (! ntasks)
    ntasks = cfg_getint (type_sec, "max_queued");

  if (! (pool = vt_thread_pool_create ((void *)dict, nthreads, &vt_async_dict_worker, err)))
    goto failure;

  vt_thread_pool_set_max_idle_threads (pool, nidle_threads);
  vt_thread_pool_set_max_queued (pool, ntasks);

  async_dict->data = (void *)pool;
  return async_dict;
failure:
  (void)vt_dict_destroy_common (async_dict, NULL);
  return NULL;
}

int
vt_async_dict_check (vt_dict_t *async_dict,
                     vt_request_t *req,
                     vt_result_t *res,
                     int pos,
                     vt_error_t *err)
{
  vt_dict_t *dict;
  vt_async_dict_arg_t *data;
  vt_thread_pool_t *pool;

  assert (async_dict);
  assert (req);
  assert (res);
  pool = (vt_thread_pool_t *)async_dict->data;
  assert (pool);

  if (! (data = calloc (1, sizeof (vt_async_dict_arg_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return -1;
  }

  data->request = req;
  data->result = res;
  data->pos = pos;

  vt_result_lock (res);
vt_debug ("%s:%d: ", __func__, __LINE__);
  if (vt_thread_pool_push (pool, (void *)data, err) != 0) {
    free (data);
    return -1;
  }
vt_debug ("%s:%d: ", __func__, __LINE__);
  return 0;
}

void
vt_async_dict_worker (void *data, void *user_data)
{
  vt_dict_t *dict;
  vt_request_t *req;
  vt_result_t *res;
  int pos;

  assert (data);
  assert (user_data);

  dict = (vt_dict_t *)user_data;
  req = ((vt_async_dict_arg_t *)data)->request;
  res = ((vt_async_dict_arg_t *)data)->result;
  pos = ((vt_async_dict_arg_t *)data)->pos;
  free (data); /* cleanup */
vt_debug ("%s:%d: ", __func__, __LINE__);
  (void)dict->check_func (dict, req, res, pos, NULL);
vt_debug ("%s:%d: ", __func__, __LINE__);
  vt_result_unlock (res);
}

int
vt_async_dict_destroy (vt_dict_t *async_dict, vt_error_t *err)
{
  vt_dict_t *dict;
  vt_thread_pool_t *pool;

  assert (async_dict);
  pool = (vt_thread_pool_t *)async_dict->data;
  assert (pool);

  dict = pool->user_data;
  if (! vt_thread_pool_destroy (pool, err)) /* blocking */
    return -1;
  if (! dict->destroy_func (dict, err))
    return -1;

  return 0;
}

int
vt_dict_dynamic_pattern (const char *s)
{
  char *p;

  for (p=(char*)s; *p; p++) {
    if (*p == '%' && *++p != '%')
      return 1;
  }

  return 0;
}

char *
vt_dict_unescape_pattern (const char *s1)
{
  char *s2, *p1, *p2;
  size_t n;

  n = strlen (s1);

  if ((s2 = calloc (1, n + 1)) == NULL)
    return NULL;

  for (p1=(char*)s1, p2=s2; *p1;) {
    *p2++ = *p1;

    if (*p1++ == '%' && *p1 == '%')
      ++p1;
  }

 *p2 = '\0';

  return s2;
}
