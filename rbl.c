/* system includes */
#include <arpa/inet.h>
#include <confuse.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <spf2/spf.h>
#include <spf2/spf_dns.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

/* valiant includes */
#include "constants.h"
#include "rbl.h"
#include "slist.h"

/*
 * type dnsbl {
 *	 max_idle_threads = 10
 *	 max_threads = 200
 *   min_threads = 10
 * }
 *
 * check spamhaus {
 *   zone = zen.spamhaus.org
 *	 ipv4 = true
 *   ipv6 = false
 *	 weight = 1.0
 *   in 127.0.0.1/32 {
 *     weight = 2.0
 *   }
 * }
 */

typedef struct vt_rbl_weight_struct vt_rbl_weight_t;

struct vt_rbl_weight_struct {
  unsigned long network;
  unsigned long netmask;
  int weight;
};

/* parameters used in "type dnsbl { }" */
const cfg_opt_t vt_rbl_type_opts[] = {
  CFG_INT ("max_idle_threads", 10, CFGF_NONE),
  CFG_INT ("max_tasks", 200, CFGF_NONE),
  CFG_INT ("max_threads", 100, CFGF_NONE),
  CFG_INT ("min_threads", 10, CFGF_NONE),
  CFG_END ()
};

/* parameters used in "in <subnet> { }" */
const cfg_opt_t vt_rbl_check_in_opts[] = {
	CFG_FLOAT ("weight", 1.0, CFGF_NONE),
	CFG_END ()
};

/* parameters used in "check <name> { }" */
const cfg_opt_t vt_rbl_check_opts[] = {
  CFG_SEC ("in", (cfg_opt_t *) vt_rbl_check_in_opts, CFGF_MULTI | CFGF_TITLE),
	CFG_STR ("type", 0, CFGF_NODEFAULT),
  CFG_STR ("zone", 0, CFGF_NODEFAULT),
  CFG_FLOAT ("weight", 1.0, CFGF_NONE),
  CFG_BOOL ("ipv4", cfg_true, CFGF_NONE),
  CFG_BOOL ("ipv6", cfg_false, CFGF_NONE),
  CFG_END ()
};

/* prototypes */
void vt_rbl_destroy (vt_rbl_t *);
int vt_rbl_skip (int *, vt_rbl_t *);
int vt_rbl_error (vt_rbl_t *);
vt_rbl_weight_t *vt_rbl_weight_create (const char *, float );
int vt_rbl_weight_destroy (void *);
int vt_rbl_weight_sort (void *, void *);

/* defines */
#define TIME_FRAME (60)
#define MAX_ERRORS (10)
#define HALF_HOUR_IN_SECONDS (1800) /* minimum back off time */
#define BACK_OFF_TIME HALF_HOUR_IN_SECONDS /* half hour in seconds */
#define MAX_BACK_OFF_TIME (86400) /* 1 day in seconds */

int
vt_rbl_create (vt_rbl_t **dest, const cfg_t *sec)
{
  cfg_t *in;
  vt_rbl_t *rbl;
  vt_rbl_weight_t *weight;
  vt_slist_t *root, *next;
  char *network, *type, *zone;
  float points;
  int i, n;

  root = NULL;

  if ((rbl = malloc (sizeof (vt_rbl_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, strerror (errno));
    goto failure;
  }

  if ((rbl->lock = malloc (sizeof (pthread_rwlock_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, strerror (errno));
    goto failure;
  }

  if (pthread_rwlock_init (rbl->lock, NULL)) {
    vt_error ("%s: pthread_rwlock_info: %s", __func__, strerror (errno));
    goto failure;
  }

  if ((type = cfg_getstr ((cfg_t *)sec, "type")) == NULL) {
    vt_error ("%s: type mandatory", __func__);
    errno = EINVAL;
    goto failure;
  }

  if ((zone = cfg_getstr ((cfg_t *)sec, "zone")) == NULL) {
    vt_error ("%s: zone mandatory", __func__);
    errno = EINVAL;
    goto failure;
  }

  if ((rbl->zone = strdup (zone)) == NULL) {
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    goto failure;
  }

  if (cfg_getbool ((cfg_t *)sec, "ipv6")) {
    vt_warning ("%s: ipv6 not supported yet, ignoring option", __func__);
  }

  if (! cfg_getbool ((cfg_t *)sec, "ipv4")) {
    vt_error ("%s: ipv6 not supported yet, ipv4 disabled", __func__);
    errno = EINVAL;
    goto failure;
  }

  network = "127.0.0.0/8";
  points = vt_check_weight (cfg_getfloat ((cfg_t *)sec, "weight"));
  i = 0;
  n = cfg_size ((cfg_t *)sec, "in");

  do {
    if ((weight = vt_rbl_weight_create (network, points)) == NULL) {
      /* vt_rbl_weight_create logs */
      goto failure;
    }

    if ((next = vt_slist_append (root, weight)) == NULL) {
      vt_error ("%s: slist_append: %s", __func__, strerror (errno));
      goto failure;
    }

    if (root == NULL)
      root = next;
    if (i >= n)
      break;

    in = cfg_getnsec ((cfg_t *)sec, "in", i++);
    network = (char *)cfg_title (in);
    points = vt_check_weight (cfg_getfloat (in, "weight"));
  } while (in);

  root = vt_slist_sort (root, &vt_rbl_weight_sort);
  rbl->weights = root;

  for (i=1, n=ceil ((MAX_BACK_OFF_TIME / BACK_OFF_TIME)); i < n; i<<=1)
    ;

  rbl->max_power = i;
  *dest = rbl;

  return VT_SUCCESS;

failure:
  vt_rbl_destroy (rbl);
  vt_slist_free (root, &vt_rbl_weight_destroy, 0);

  if (errno == ENOMEM)
    return VT_ERR_NOMEM;

  return VT_ERR_INVAL;
}

void
vt_rbl_destroy (vt_rbl_t *rbl)
{
  // IMPLEMENT
  return;
}

int
vt_rbl_check (vt_check_t *check,
              vt_request_t *request,
              vt_score_t *score,
              vt_stats_t *stats,
              vt_thread_pool_t *thread_pool)
{
  int ret, skip;
  vt_rbl_t *rbl = (vt_rbl_t *)check->info;
  vt_rbl_param_t *param;

  if ((ret = vt_rbl_skip (&skip, rbl)) != VT_SUCCESS)
    return ret;
  if (skip)
    return VT_SUCCESS;  

  vt_score_lock (score);

  if ((param = malloc (sizeof (vt_rbl_param_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  param->check = check;
  param->request = request;
  param->score = score;
  param->stats = stats;

  vt_thread_pool_task_push (thread_pool, (void *)param);

  return VT_SUCCESS;
}

void
vt_rbl_worker (const vt_rbl_param_t *param,
               const SPF_server_t *spf_server,
               const char *query)
{
  int i;
  vt_check_t *check;
  vt_rbl_t *rbl;
  vt_rbl_weight_t *weight, *heaviest;
  vt_score_t *score;
  vt_slist_t *cur;
  SPF_dns_rr_t *dns_rr;
  unsigned long address;

  check = param->check;
  rbl = (vt_rbl_t *)check->info;
  score = param->score;

  dns_rr = SPF_dns_lookup (spf_server->resolver, query, ns_t_a, 0);

  switch (dns_rr->herrno) {
    case NO_DATA:
      /* This ugly hack is necessary because of what I think is a bug in
         SPF_dns_lookup_resolve. Correction: not a bug, this situation occurs
         when debug is more than 1. */
      for (i=0; i < dns_rr->rr_buf_num && dns_rr->rr[i]; i++)
        ;

      if (i)
        dns_rr->num_rr = i;

      /* fall through */
    case NETDB_SUCCESS:
      heaviest = NULL;
      for (i=0; i < dns_rr->num_rr && dns_rr->rr[i]; i++) {
        address = ntohl (dns_rr->rr[i]->a.s_addr);

        for (cur=rbl->weights; cur; cur=cur->next) {
          weight = (vt_rbl_weight_t *)cur->data;

          if ((address & weight->netmask) == (weight->network & weight->netmask)) {
            if (heaviest == NULL || heaviest->weight < weight->weight)
              heaviest = weight;
          }
        }
      }

      if (heaviest)
        vt_score_update (score, heaviest->weight);
      break;
    case TRY_AGAIN: // SERVFAIL
      vt_rbl_error (rbl);
      break;
  }

  SPF_dns_rr_free (dns_rr);

  return;
}

int
vt_rbl_skip (int *skip, vt_rbl_t *rbl)
{
  int ret;
  time_t now = time (NULL);

  if ((ret = pthread_rwlock_rdlock (rbl->lock))) {
    vt_error ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));
    return VT_ERR_SYSTEM;
  }

  *skip = (rbl->back_off > now) ? 1 : 0;

  if ((ret = pthread_rwlock_unlock (rbl->lock))) {
    vt_error ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
    return VT_ERR_SYSTEM;
  }

  return VT_SUCCESS;
}


int
vt_rbl_error (vt_rbl_t *rbl)
{
  int ret, power;
  double multiplier;
  time_t time_slot, now = time(NULL);

  if ((ret = pthread_rwlock_wrlock (rbl->lock))) {
    vt_error ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));
    return VT_ERR_SYSTEM;
  }

  if (now > rbl->back_off) {
    /* error counter should only be increased if we're within the allowed time
       frame. */
    if (rbl->start > rbl->back_off && rbl->start > (now - TIME_FRAME)) {
      if (++rbl->errors >= MAX_ERRORS) {
        rbl->errors = 0;

        // FIXME: update comment
        /* current power must be updated, since the check might not have
           failed for a long time. here. The general idea here is to get
           the difference between back off time and now and divide it by the
           number of seconds in BACK_OFF_TIME. That gives us the multiplier.
           then we substrac the value of power either until power reaches 1
           or the multiplier is smaller than one! */
        power = rbl->power;
        time_slot = (now - rbl->back_off);

        if (rbl->power >= rbl->max_power) {
          if (time_slot > MAX_BACK_OFF_TIME) {
            power <<= 1;
            time_slot -= MAX_BACK_OFF_TIME;
          } else {
            time_slot = 0;
          }
        }

        multiplier = (time_slot / BACK_OFF_TIME);

        for (; power > 1 && power < multiplier; power<<=1)
          multiplier -= power;

        // now: either power is 1... which is 2^0
        // or: multiplier is less than 1!
        // if multiplier is smaller than 1... we have our current power
        // the one thing that is funny here is the max_power thing... if we've
        // reached that we should first substract that one!
        // we should update the power... it's only fair... right?
        // we do that by doing a log... so... that's the current time
        // minus the current back off time... divided by the back off time and
        // then the log2 of the multiplier

        /* now that the power is updated we increase it... and set the back
           off time! */
        rbl->power = power;

        if (rbl->power < rbl->max_power) {
          rbl->power >>= 1;
          rbl->back_off = now + (BACK_OFF_TIME * rbl->power);
        } else {
          rbl->power = rbl->max_power;
          rbl->back_off = now + MAX_BACK_OFF_TIME;
        }
      }
    } else {
      rbl->start = time (NULL);
      rbl->errors = 1;
    }
  }

  if ((ret = pthread_rwlock_unlock (rbl->lock))) {
    vt_error ("%s; pthread_rwlock_unlock: %s", __func__, strerror (ret));
    return ret;
  }

  return VT_SUCCESS;
}

vt_rbl_weight_t *
vt_rbl_weight_create (const char *cidr, float points)
{
  char buf[INET_ADDRSTRLEN];
  char *p1, *p2;
  int i;
  long bits;
  struct in_addr network;
  unsigned long netmask;
  vt_rbl_weight_t *weight;

  /* copy network part from cidr notation */
  for (p1=(char*)cidr, p2=buf;
      *p1 != '\0' && *p1 != '/' && p2 < (buf+INET_ADDRSTRLEN);
      *p2++ = *p1++)
    ;

  *p2 = '\0';

  /* copy number of masked bits from cidr notation */
  if (*p1 == '/')
    bits = strtol ((++p1), NULL, 10);

  if (*p1 == '\0' || bits < 0 || bits > 32 || (bits == 0 && errno == EINVAL)) {
    errno = EINVAL;
    vt_error ("%s: invalid presentation of network/bitmask: %s", __func__, cidr);
    return NULL;
  }

  switch (inet_pton (AF_INET, buf, &network)) {
    case 0:
      errno = EINVAL;
      vt_error ("%s: invalid presentation of network/bitmask: %s", __func__, cidr);
      return NULL;
    case -1:
      vt_error ("%s: inet_pton: %s", __func__, strerror (errno));
      return NULL;
  }

  for (netmask=0, i=0; i < bits; i++)
    netmask |= 1 << (31 - i);

  if ((weight = malloc (sizeof (vt_rbl_weight_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, strerror (errno));
    return NULL;
  }

  weight->network = ntohl (network.s_addr);
  weight->netmask = netmask;
  weight->weight = (int)points;

  return weight;
}

int
vt_rbl_weight_destroy (void *weight)
{
  if (weight != NULL)
    free (weight);

  return VT_SUCCESS;
}

int
vt_rbl_weight_sort (void *p1, void *p2)
{
  vt_rbl_weight_t *w1, *w2;

  w1 = (vt_rbl_weight_t *)p1;
  w2 = (vt_rbl_weight_t *)p2;

  if (w1->netmask > w2->netmask)
    return -1;
  if (w1->netmask < w2->netmask)
    return  1;

  return 0;
}
