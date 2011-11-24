/* system includes */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_priv.h"
#include "check_dnsbl.h"
#include "rbl.h"
#include "thread_pool.h"
#include "utils.h"

SPF_server_t *dnsbl_spf_server = NULL;
vt_thread_pool_t *dnsbl_thread_pool = NULL;
pthread_mutex_t dnsbl_thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* prototypes */
int vt_check_dnsbl_init (cfg_t *, vt_error_t *);
int vt_check_dnsbl_deinit (vt_error_t *);
vt_check_t *vt_check_dnsbl_create (const vt_map_list_t *, cfg_t *,
  vt_error_t *);
int vt_check_dnsbl_destroy (vt_check_t *, vt_error_t *);
int vt_check_dnsbl_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_error_t *);
void vt_check_dnsbl_worker (void *);

const vt_check_type_t _vt_check_dnsbl_type = {
  .name = "dnsbl",
  .flags = VT_CHECK_TYPE_FLAG_NEED_INIT,
  .init_func = &vt_check_dnsbl_init,
  .deinit_func = &vt_check_dnsbl_deinit,
  .create_check_func = &vt_check_dnsbl_create
};

const vt_check_type_t *
vt_check_dnsbl_type (void)
{
  return &_vt_check_dnsbl_type;
}

int
vt_check_dnsbl_init (cfg_t *sec, vt_error_t *err)
{
  int ret;
  int max_threads, max_idle_threads, max_tasks;

  if ((ret = pthread_mutex_trylock (&dnsbl_thread_pool_mutex)) != 0)
    vt_panic ("%s: pthread_mutex_trylock: %s", __func__, strerror (ret));

  if (! dnsbl_thread_pool) {
    max_threads = cfg_getint (sec, "max_threads");
    max_idle_threads = cfg_getint (sec, "max_idle_threads");
    max_tasks = cfg_getint (sec, "max_tasks");
    dnsbl_thread_pool = vt_thread_pool_create ("dnsbl", max_threads,
      &vt_check_dnsbl_worker, NULL);

		if (dnsbl_thread_pool == NULL)
      vt_fatal ("%s: vt_thread_pool_create: %s", __func__, strerror (errno));

    /* specifying debug value larger than one changes the behaviour of
       SPF_server, don't do that */
    dnsbl_spf_server = SPF_server_new(SPF_DNS_CACHE, 0);

    if (dnsbl_spf_server == NULL)
      vt_fatal ("%s: SPF_server_new: %s", __func__, strerror (errno));
	}

  if ((ret = pthread_mutex_unlock (&dnsbl_thread_pool_mutex)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

	return 0;
}

int
vt_check_dnsbl_deinit (vt_error_t *err)
{
  int ret;

  if ((ret = pthread_mutex_trylock (&dnsbl_thread_pool_mutex)) != 0)
    vt_panic ("%s: pthread_mutex_trylock: %s", __func__, strerror (errno));

  if (dnsbl_thread_pool) {
    (void)vt_thread_pool_destroy (dnsbl_thread_pool, NULL);
    dnsbl_thread_pool = NULL;
  }

  if (dnsbl_spf_server) {
    SPF_server_free (dnsbl_spf_server);
    dnsbl_spf_server = NULL;
  }

  if ((ret = pthread_mutex_unlock (&dnsbl_thread_pool_mutex)) != 0)
    vt_panic ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

  return 0;
}

vt_check_t *
vt_check_dnsbl_create (const vt_map_list_t *list, cfg_t *sec, vt_error_t *err)
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
  check->check_func = &vt_check_dnsbl_check;
  check->destroy_func = &vt_check_dnsbl_destroy;

  return check;
FAILURE:
  (void)vt_check_dnsbl_destroy (check, NULL);
  return NULL;
}

int
vt_check_dnsbl_destroy (vt_check_t *check, vt_error_t *err)
{
  int ret;

  if (check) {
    if (check->data)
      vt_rbl_destroy ((vt_rbl_t *)check->data, err);
    (void)vt_check_destroy (check, err);
  }
}

int
vt_check_dnsbl_check (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  return vt_rbl_check (check, request, score, dnsbl_thread_pool, err);
}

void
vt_check_dnsbl_worker (void *arg)
{
  char *client_address;
  char query[HOST_NAME_MAX], reverse[INET_ADDRSTRLEN];
  vt_check_t *check;
  vt_rbl_t *rbl;
  vt_request_t *request;
  vt_score_t *score;

  check = ((vt_rbl_arg_t *)arg)->check;
  rbl = (vt_rbl_t *)check->data;
  request = ((vt_rbl_arg_t *)arg)->request;
  score = ((vt_rbl_arg_t *)arg)->score;
  client_address = vt_buf_str (&request->client_address);

  if (client_address) {
    if (reverse_inet_addr (client_address, reverse, INET_ADDRSTRLEN) < 0)
      vt_panic ("%s: reverse_inet_addr: %s", __func__, strerror (errno));
vt_error ("%s (%d): reverse: %s", __func__, __LINE__, reverse);
    if (snprintf (query, HOST_NAME_MAX, "%s.%s", reverse, rbl->zone) >= HOST_NAME_MAX)
      vt_panic ("%s: dnsbl query exceeded maximum hostname length", __func__);

    vt_rbl_worker ((vt_rbl_arg_t *)arg, dnsbl_spf_server, query);
  }

  vt_score_unlock (score);
  free (arg);
}
