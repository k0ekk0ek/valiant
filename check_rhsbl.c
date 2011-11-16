/* system includes */
#include <confuse.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_rhsbl.h"
#include "consts.h"
#include "rbl.h" /* share code between dnsbl and rhsbl */
#include "thread_pool.h"
#include "utils.h"

SPF_server_t *rhsbl_spf_server = NULL;
vt_thread_pool_t *rhsbl_thread_pool = NULL;
pthread_mutex_t rhsbl_thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* prototypes */
int vt_check_rhsbl_init (const cfg_t *);
int vt_check_rhsbl_deinit (void);
int vt_check_rhsbl_create (vt_check_t **, const vt_map_list_t *, const cfg_t *);
void vt_check_rhsbl_destroy (vt_check_t *);
int vt_check_rhsbl_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
void vt_check_rhsbl_worker (void *);
int vt_check_rhsbl_weight (vt_check_t *, int);

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
vt_check_rhsbl_init (const cfg_t *sec)
{
  int ret;
	int max_threads, max_idle_threads, max_tasks;

	if ((ret = pthread_mutex_trylock (&rhsbl_thread_pool_mutex)) != 0) {
		if (ret == EBUSY)
			return VT_SUCCESS; /* not my problem */
		vt_fatal ("%s: pthread_mutex_trylock: %s", __func__, strerror (ret));
	}

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
    vt_fatal ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

	return VT_SUCCESS;
}

int
vt_check_rhsbl_deinit (void)
{
  int ret;

  if ((ret = pthread_mutex_trylock (&rhsbl_thread_pool_mutex)) != 0) {
    if (ret == EBUSY)
      return VT_SUCCESS;
    vt_fatal ("%s: pthread_mutex_trylock: %s", __func__, strerror (errno));
  }

  if (rhsbl_thread_pool) {
    vt_thread_pool_destroy (rhsbl_thread_pool);
    rhsbl_thread_pool = NULL;
  }

  if (rhsbl_spf_server) {
    SPF_server_free (rhsbl_spf_server);
    rhsbl_spf_server = NULL;
  }

  if ((ret = pthread_mutex_unlock (&rhsbl_thread_pool_mutex)) != 0)
    vt_fatal ("%s: pthread_mutex_unlock: %s", __func__, strerror (ret));

  return VT_SUCCESS;
}

int
vt_check_rhsbl_create (vt_check_t **dest, const vt_map_list_t *list,
  const cfg_t *sec)
{
  int ret, err;
  vt_check_t *check = NULL;
  vt_rbl_t *rbl = NULL;

  if ((ret = vt_check_create (&check, 0, list, sec)) != VT_SUCCESS ||
      (ret = vt_rbl_create (&rbl, sec)) != VT_SUCCESS)
  {
    err = ret;
    goto FAILURE;
  }

  check->prio = 5;
  check->data = (void *)rbl;
  check->check_func = &vt_check_rhsbl_check;
  check->weight_func = &vt_check_rhsbl_weight;
  check->destroy_func = &vt_check_rhsbl_destroy;

  *dest = check;
  return VT_SUCCESS;
FAILURE:
  vt_check_rhsbl_destroy (check);
  return err;
}

void
vt_check_rhsbl_destroy (vt_check_t *check)
{
  if (check) {
    if (check->data)
      vt_rbl_destroy ((vt_rbl_t *)check->data);
    vt_check_destroy (check);
  }
}

int
vt_check_rhsbl_check (vt_check_t *check,
                      vt_request_t *request,
                      vt_score_t *score,
                      vt_stats_t *stats)
{
  return vt_rbl_check (check, request, score, stats, rhsbl_thread_pool);
}

void
vt_check_rhsbl_worker (void *arg)
{
  char query[HOST_NAME_MAX];
  char *ptr;
  vt_check_t *check;
  vt_rbl_t *rbl;
  vt_request_t *request;
  vt_score_t *score;

  check = ((vt_rbl_param_t *)arg)->check;
  rbl = (vt_rbl_t *)check->data;
  request = ((vt_rbl_param_t *)arg)->request;
  score = ((vt_rbl_param_t *)arg)->score;

  if (request->sender) {
    for (ptr=request->sender; *ptr != '@' && *ptr != '\0'; ptr++)
      ;

    if (*ptr != '\0') {
      if (snprintf (query, HOST_NAME_MAX, "%s.%s", ++ptr, rbl->zone) >= HOST_NAME_MAX)
        vt_panic ("%s: rhsbl query exceeded maximum hostname length", __func__);

      vt_rbl_worker ((vt_rbl_param_t *)arg, rhsbl_spf_server, query);
    }
  }

  vt_score_unlock (score);
}

int
vt_check_rhsbl_weight (vt_check_t *check, int maximum)
{
  return vt_rbl_weight ((vt_rbl_t *)check->data, maximum);
}
