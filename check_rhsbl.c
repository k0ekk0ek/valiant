/* system includes */
#include <confuse.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_rhsbl.h"
#include "constants.h"
#include "rbl.h" /* share code between dnsbl and rhsbl */
#include "thread_pool.h"
#include "type.h"
#include "utils.h"

SPF_server_t *rhsbl_spf_server = NULL;
vt_thread_pool_t *rhsbl_thread_pool = NULL;
pthread_mutex_t rhsbl_thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* prototypes */
int vt_rhsbl_init (const cfg_t *);
int vt_rhsbl_deinit (void);
int vt_rhsbl_destroy (vt_check_t *);
int vt_rhsbl_check (vt_check_t *, vt_request_t *, vt_score_t *, vt_stats_t *);
void vt_rhsbl_worker (void *);

vt_type_t *
vt_rhsbl_type (void)
{
  vt_type_t *type;

  if ((type = malloc (sizeof (vt_type_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, __LINE__, strerror (errno));
    return NULL;
  }

  memset (type, 0, sizeof (vt_type_t));

  type->name = "rhsbl";
  type->flags = VT_TYPE_FLAG_NEED_INIT;
  type->type_opts = vt_rbl_type_opts;
  type->check_opts = vt_rbl_check_opts;
  type->init_func = &vt_rhsbl_init;
  type->deinit_func = &vt_rhsbl_deinit;
  type->create_check_func = &vt_rhsbl_create;

  return type;
}

int
vt_rhsbl_init (const cfg_t *sec)
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
			&vt_rhsbl_worker);

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
vt_rhsbl_deinit (void)
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
vt_rhsbl_create (vt_check_t **dest, const cfg_t *sec)
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
  check->check_func = &vt_rhsbl_check;
  check->destroy_func = &vt_rhsbl_destroy;

  *dest = check;

  return VT_SUCCESS;
}

int
vt_rhsbl_destroy (vt_check_t *check)
{
  // IMPLEMENT
}

int
vt_rhsbl_check (vt_check_t *check,
                vt_request_t *request,
                vt_score_t *score,
                vt_stats_t *stats)
{
  return vt_rbl_check (check, request, score, stats, rhsbl_thread_pool);
}

void
vt_rhsbl_worker (void *param)
{
  char query[HOST_NAME_MAX];
  char *ptr;
  vt_check_t *check;
  vt_rbl_t *rbl;
  vt_request_t *request;
  vt_score_t *score;

  check = ((vt_rbl_param_t *)param)->check;
  rbl = (vt_rbl_t *)check->info;
  request = ((vt_rbl_param_t *)param)->request;
  score = ((vt_rbl_param_t *)param)->score;

  if (request->sender) {
    for (ptr=request->sender; *ptr != '@' && *ptr != '\0'; ptr++)
      ;

    if (*ptr != '\0') {
      if (snprintf (query, HOST_NAME_MAX, "%s.%s", ++ptr, rbl->zone) >= HOST_NAME_MAX)
        vt_panic ("%s: rhsbl query exceeded maximum hostname length", __func__);

      vt_rbl_worker ((vt_rbl_param_t *)param, rhsbl_spf_server, query);
    }
  }

  vt_score_unlock (score);

  return;
}
