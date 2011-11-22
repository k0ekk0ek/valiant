/* system includes */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_priv.h"
#include "check_rhsbl.h"
#include "rbl.h"
#include "thread_pool.h"

SPF_server_t *rhsbl_spf_server = NULL;
vt_thread_pool_t *rhsbl_thread_pool = NULL;
pthread_mutex_t rhsbl_thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* prototypes */
int vt_check_rhsbl_init (cfg_t *, vt_error_t *);
int vt_check_rhsbl_deinit (vt_error_t *);
vt_check_t *vt_check_rhsbl_create (const vt_map_list_t *, cfg_t *,
  vt_error_t *);
int vt_check_rhsbl_destroy (vt_check_t *, vt_error_t *);
int vt_check_rhsbl_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_error_t *);
void vt_check_rhsbl_worker (void *);
int vt_check_rhsbl_max_weight (vt_check_t *);
int vt_check_rhsbl_min_weight (vt_check_t *);

const vt_check_type_t vt_check_type_rhsbl = {
  .name = "rhsbl",
  .flags = VT_CHECK_TYPE_FLAG_NEED_INIT,
  .init_func = &vt_check_rhsbl_init,
  .deinit_func = &vt_check_rhsbl_deinit,
  .create_check_func = &vt_check_rhsbl_create
};

const vt_check_type_t *
vt_check_rhsbl_type (void)
{
  return &vt_check_type_rhsbl;
}

int
vt_check_rhsbl_init (cfg_t *sec, vt_error_t *err)
{
  int ret;
	int max_threads, max_idle_threads, max_tasks;

	if ((ret = pthread_mutex_trylock (&rhsbl_thread_pool_mutex)) != 0)
		vt_panic ("%s: pthread_mutex_trylock: %s", __func__, strerror (ret));

	if (! rhsbl_thread_pool) {
		max_threads = (int)cfg_getint ((cfg_t *)sec, "max_threads");
		max_idle_threads = (int)cfg_getint ((cfg_t *)sec, "max_idle_threads");
		max_tasks = (int)cfg_getint ((cfg_t *)sec, "max_tasks");

		rhsbl_thread_pool = vt_thread_pool_create ("rhsbl", max_threads,
			&vt_check_rhsbl_worker);

		if (rhsbl_thread_pool == NULL)
      vt_fatal ("%s: vt_thread_pool_create: %s", __func__, strerror (errno));

    /* specifying debug value larger than one changes the behaviour of
       SPF_server, don't do that */
    rhsbl_spf_server = SPF_server_new(SPF_DNS_CACHE, 0);

    if (rhsbl_spf_server == NULL)
      vt_fatal ("%s: SPF_server_new: %s", __func__, strerror (errno));
	}

  if ((ret = pthread_mutex_unlock (&rhsbl_thread_pool_mutex)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

	return 0;
}

int
vt_check_rhsbl_deinit (vt_error_t *err)
{
  int ret;

  if ((ret = pthread_mutex_trylock (&rhsbl_thread_pool_mutex)) != 0)
    vt_panic ("%s: pthread_mutex_trylock: %s", __func__, strerror (errno));

  if (rhsbl_thread_pool) {
    vt_thread_pool_destroy (rhsbl_thread_pool);
    rhsbl_thread_pool = NULL;
  }

  if (rhsbl_spf_server) {
    SPF_server_free (rhsbl_spf_server);
    rhsbl_spf_server = NULL;
  }

  if ((ret = pthread_mutex_unlock (&rhsbl_thread_pool_mutex)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

  return 0;
}

vt_check_t *
vt_check_rhsbl_create (const vt_map_list_t *list, cfg_t *sec, vt_error_t *err)
{
  vt_check_t *check = NULL;
  vt_rbl_t *rbl = NULL;

  if (! (check = vt_check_create (list, sec, err)) ||
      ! (rbl = vt_rbl_create (sec, err)))
    goto FAILURE;

  check->prio = 5;
  check->data = (void *)rbl;
  check->max_weight = vt_rbl_max_weight (rbl);
  check->min_weight = vt_rbl_min_weight (rbl);
  check->check_func = &vt_check_rhsbl_check;
  check->destroy_func = &vt_check_rhsbl_destroy;

  return check;
FAILURE:
  (void)vt_check_rhsbl_destroy (check, NULL);
  return NULL;
}

int
vt_check_rhsbl_destroy (vt_check_t *check, vt_error_t *err)
{
  if (check) {
    if (check->data)
      vt_rbl_destroy ((vt_rbl_t *)check->data, err);
    vt_check_destroy (check, err);
  }
  return 0;
}

int
vt_check_rhsbl_check (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  return vt_rbl_check (check, request, score, rhsbl_thread_pool, err);
}

void
vt_check_rhsbl_worker (void *arg)
{
  char *sender_domain;
  char query[HOST_NAME_MAX];
  char *ptr;
  vt_check_t *check;
  vt_rbl_t *rbl;
  vt_request_t *request;
  vt_score_t *score;

  check = ((vt_rbl_arg_t *)arg)->check;
  rbl = (vt_rbl_t *)check->data;
  request = ((vt_rbl_arg_t *)arg)->request;
  score = ((vt_rbl_arg_t *)arg)->score;
  sender_domain = vt_buf_str (&request->sender_domain);

  if (sender_domain) {
    if (snprintf (query, HOST_NAME_MAX, "%s.%s", sender_domain, rbl->zone) >= HOST_NAME_MAX)
      vt_panic ("%s: rhsbl query exceeded maximum hostname length", __func__);

    vt_rbl_worker ((vt_rbl_arg_t *)arg, rhsbl_spf_server, query);
  }

  vt_score_unlock (score);
  free (arg);
}
