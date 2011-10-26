/* system includes */
#include <confuse.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_dnsbl.h"
#include "constants.h"
#include "rbl.h" /* share code between dnsbl and rhsbl */
#include "thread_pool.h"
#include "type.h"
#include "utils.h"

SPF_server_t *dnsbl_spf_server = NULL;
vt_thread_pool_t *dnsbl_thread_pool = NULL;
pthread_mutex_t dnsbl_thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* prototypes */
int vt_dnsbl_init (const cfg_t *);
int vt_dnsbl_deinit (void);
int vt_dnsbl_destroy (vt_check_t *);
int vt_dnsbl_check (vt_check_t *, vt_request_t *, vt_score_t *, vt_stats_t *);
void vt_dnsbl_worker (void *);

vt_type_t *
vt_dnsbl_type (void)
{
  vt_type_t *type;

  if ((type = malloc (sizeof (vt_type_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, __LINE__, strerror (errno));
    return NULL;
  }

  memset (type, 0, sizeof (vt_type_t));

  type->name = "dnsbl";
  type->flags = VT_TYPE_FLAG_NEED_INIT;
  type->type_opts = vt_rbl_type_opts;
  type->check_opts = vt_rbl_check_opts;
  type->init_func = &vt_dnsbl_init;
  type->deinit_func = &vt_dnsbl_deinit;
  type->create_check_func = &vt_dnsbl_create;

  return type;
}

int
vt_dnsbl_init (const cfg_t *sec)
{
	int ret;
	int max_threads, max_idle_threads, max_tasks;

	if ((ret = pthread_mutex_trylock (&dnsbl_thread_pool_mutex)) != 0) {
		if (ret == EBUSY)
			return 0; /* not my problem */
		vt_fatal ("%s: pthread_mutex_trylock: %s", __func__, strerror (ret));
	}

	if (! dnsbl_thread_pool) {
		max_threads = (int)cfg_getint ((cfg_t *)sec, "max_threads");
		max_idle_threads = (int)cfg_getint ((cfg_t *)sec, "max_idle_threads");
		max_tasks = (int)cfg_getint ((cfg_t *)sec, "max_tasks");

		dnsbl_thread_pool = vt_thread_pool_create ("dnsbl", max_threads,
			&vt_dnsbl_worker);

		if (dnsbl_thread_pool == NULL)
      vt_fatal ("%s: vt_thread_pool_create: %s", __func__, strerror (errno));

    /* specifying debug value larger than one changes the behaviour of
       SPF_server, don't do that */
    dnsbl_spf_server = SPF_server_new(SPF_DNS_CACHE, 0);

    if (dnsbl_spf_server == NULL)
      vt_fatal ("%s: SPF_server_new: %s", __func__, strerror (errno));
	}

  if ((ret = pthread_mutex_unlock (&dnsbl_thread_pool_mutex)) != 0)
    vt_fatal ("%s (%d): pthread_mutex_unlock: %s", __func__, __LINE__,
      strerror (ret));

	return 0;
}

int
vt_dnsbl_deinit (void)
{
  int ret;

  if ((ret = pthread_mutex_trylock (&dnsbl_thread_pool_mutex)) != 0) {
    if (ret == EBUSY)
      return VT_SUCCESS;
    vt_fatal ("%s: pthread_mutex_trylock: %s", __func__, strerror (errno));
  }

  if (dnsbl_thread_pool) {
    vt_thread_pool_destroy (dnsbl_thread_pool);
    dnsbl_thread_pool = NULL;
  }

  if (dnsbl_spf_server) {
    SPF_server_free (dnsbl_spf_server);
    dnsbl_spf_server = NULL;
  }

  if ((ret = pthread_mutex_unlock (&dnsbl_thread_pool_mutex)) != 0)
    vt_fatal ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

  return VT_SUCCESS;
}

int
vt_dnsbl_create (vt_check_t **dest, const cfg_t *sec)
{
  char *title;
  int ret;
  vt_check_t *check;
  vt_rbl_t *rbl;

  if ((title = (char *)cfg_title ((cfg_t *)sec)) == NULL) {
    vt_error ("%s: title for check section missing", __func__);
    return VT_ERR_INVAL;
  }

  if ((check = vt_check_alloc (0, title)) == NULL)
    return VT_ERR_NOMEM;
  if ((ret = vt_rbl_create (&rbl, sec)) != VT_SUCCESS)
    return ret;

  check->prio = 5;
  check->info = (void *)rbl;
  check->check_func = &vt_dnsbl_check;
  check->destroy_func = &vt_dnsbl_destroy;

  *dest = check;

  return VT_SUCCESS;
}

int
vt_dnsbl_destroy (vt_check_t *check)
{
  // IMPLEMENT
  return VT_SUCCESS;
}

int
vt_dnsbl_check (vt_check_t *check,
                vt_request_t *request,
                vt_score_t *score,
                vt_stats_t *stats)
{
  return vt_rbl_check (check, request, score, stats, dnsbl_thread_pool);
}

void
vt_dnsbl_worker (void *param)
{
  char query[HOST_NAME_MAX], reverse[INET_ADDRSTRLEN];
  vt_check_t *check;
  vt_rbl_t *rbl;
  vt_request_t *request;
  vt_score_t *score;

  check = ((vt_rbl_param_t *)param)->check;
  rbl = (vt_rbl_t *)check->info;
  request = ((vt_rbl_param_t *)param)->request;
  score = ((vt_rbl_param_t *)param)->score;

  if (request->client_address) {
    if (reverse_inet_addr (request->client_address, reverse, INET_ADDRSTRLEN) < 0)
      vt_panic ("%s: reverse_inet_addr: %s", __func__, strerror (errno));
    if (snprintf (query, HOST_NAME_MAX, "%s.%s", reverse, rbl->zone) >= HOST_NAME_MAX)
      vt_panic ("%s: dnsbl query exceeded maximum hostname length", __func__);

    vt_rbl_worker ((vt_rbl_param_t *)param, dnsbl_spf_server, query);
  }

  vt_score_unlock (score);

  return;
}























































