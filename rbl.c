/* system includes */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* valiant includes */
#include "conf.h"
#include "rbl.h"
#include "slist.h"

typedef struct _vt_rbl_weight vt_rbl_weight_t;

struct _vt_rbl_weight {
  unsigned long network;
  unsigned long netmask;
  int weight;
};

/* prototypes */
void vt_rbl_error (vt_rbl_t *);
vt_rbl_weight_t *vt_rbl_weight_create (const char *, float, vt_error_t *);
int vt_rbl_weight_destroy (void *);
int vt_rbl_weight_sort (void *, void *);
int vt_rbl_weight (vt_rbl_t *, int);

/* defines */
#define TIME_FRAME (60)
#define MAX_ERRORS (10)
#define HALF_HOUR_IN_SECONDS (1800) /* minimum back off time */
#define BACK_OFF_TIME HALF_HOUR_IN_SECONDS /* half hour in seconds */
#define MAX_BACK_OFF_TIME (86400) /* 1 day in seconds */

vt_rbl_t *
vt_rbl_create (cfg_t *sec, vt_error_t *err)
{
  cfg_t *in;
  vt_rbl_t *rbl;
  vt_rbl_weight_t *weight;
  vt_slist_t *root, *next;
  char *network, *type, *zone;
  float points;
  int i, n;
  int ret;

  root = NULL;

  if (! (rbl = calloc (1, sizeof (vt_rbl_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto FAILURE;
  }
  if ((ret = pthread_rwlock_init (&rbl->lock, NULL)) != 0) {
    if (ret != ENOMEM)
      vt_panic ("%s: pthread_rwlock_init: %s", __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: pthread_rwlock_init: %s", __func__, strerror (ret));
    goto FAILURE;
  }
  if (! (rbl->zone = vt_cfg_getstrdup (sec, "zone"))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: vt_cfg_getstrdup: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  network = "127.0.0.0/8";
  points = cfg_getfloat (sec, "weight");
  i = 0;
  n = cfg_size (sec, "in");

  do {
    if (! (weight = vt_rbl_weight_create (network, points, err)))
      goto FAILURE;

    if ((next = vt_slist_append (root, weight)) == NULL) {
      vt_rbl_weight_destroy (weight);
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: slist_append: %s", __func__, strerror (errno));
      goto FAILURE;
    }

    if (root == NULL)
      root = next;
    if (i >= n)
      break;

    in = cfg_getnsec (sec, "in", i++);
    network = (char *)cfg_title (in);
    points = cfg_getfloat (in, "weight");
  } while (in);

  for (i=1, n=ceil ((MAX_BACK_OFF_TIME / BACK_OFF_TIME)); i < n; i<<=1)
    ;

  rbl->max_power = i;
  rbl->weights = vt_slist_sort (root, &vt_rbl_weight_sort);

  return rbl;
FAILURE:
  (void)vt_rbl_destroy (rbl, NULL);
  vt_slist_free (root, &vt_rbl_weight_destroy, 0);
  return NULL;
}

int
vt_rbl_destroy (vt_rbl_t *rbl, vt_error_t *err)
{
  int ret;

  if (rbl) {
    if ((ret = pthread_rwlock_destroy (&rbl->lock)) != 0)
      vt_panic ("%s: pthread_rwlock_destroy: %s", __func__, strerror (ret));
    if (rbl->zone)
      free (rbl->zone);
    if (rbl->weights)
      vt_slist_free (rbl->weights, &vt_rbl_weight_destroy, 0);
    free (rbl);
  }

  return 0;
}

int
vt_rbl_check (vt_check_t *check, vt_request_t *request, vt_score_t *score,
  vt_thread_pool_t *thread_pool, vt_error_t *err)
{
  int ret, skip;
  vt_error_t lerr = 0;
  vt_rbl_t *rbl = (vt_rbl_t *)check->data;
  vt_rbl_arg_t *arg;

  if (! vt_rbl_skip (rbl)) {
    vt_score_lock (score);

    if (! (arg = calloc (1, sizeof (vt_rbl_arg_t)))) {
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: calloc: %s", __func__, strerror (errno));
      return -1;
    }

    arg->check = check;
    arg->request = request;
    arg->score = score;

    vt_thread_pool_task_push (thread_pool, (void *)arg, &lerr);
  }

  return 0;
}

void
vt_rbl_worker (const vt_rbl_arg_t *arg, const SPF_server_t *spf_server,
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

  check = arg->check;
  rbl = (vt_rbl_t *)check->data;
  score = arg->score;

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
        vt_score_update (score, check->cntr, heaviest->weight);
      break;
    case TRY_AGAIN: // SERVFAIL
      vt_rbl_error (rbl);
      break;
  }

  SPF_dns_rr_free (dns_rr);

  return;
}

int
vt_rbl_skip (vt_rbl_t *rbl)
{
  int ret, skip;
  time_t now = time (NULL);

  if ((ret = pthread_rwlock_rdlock (&rbl->lock)))
    vt_panic ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));

  skip = (rbl->back_off > now) ? 1 : 0;

  if ((ret = pthread_rwlock_unlock (&rbl->lock)))
    vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));

  return skip;
}


void
vt_rbl_error (vt_rbl_t *rbl)
{
  int ret, power;
  double multiplier;
  time_t time_slot, now = time(NULL);

  if ((ret = pthread_rwlock_wrlock (&rbl->lock)))
    vt_panic ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));

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

  if ((ret = pthread_rwlock_unlock (&rbl->lock)))
    vt_panic ("%s; pthread_rwlock_unlock: %s", __func__, strerror (ret));
}

vt_rbl_weight_t *
vt_rbl_weight_create (const char *cidr, float points, vt_error_t *err)
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
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: bad presentation of network/bitmask: %s", __func__, cidr);
    return NULL;
  }

  switch (inet_pton (AF_INET, buf, &network)) {
    case 0:
      vt_set_error (err, VT_ERR_BADCFG);
      vt_error ("%s: bad presentation of network/bitmask: %s", __func__, cidr);
      return NULL;
    case -1:
      vt_error ("%s: inet_pton: %s", __func__, strerror (errno));
      return NULL;
  }

  for (netmask=0, i=0; i < bits; i++)
    netmask |= 1 << (31 - i);

  if (! (weight = calloc (1, sizeof (vt_rbl_weight_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
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

float
vt_rbl_max_weight (vt_rbl_t *rbl)
{
  float n = 0.0;
  vt_slist_t *p;
  vt_rbl_weight_t *q;

  for (p = rbl->weights; p; p = p->next) {
    q = (vt_rbl_weight_t *)p->data;
    if (q->weight > n)
      n = q->weight;
  }

  return n;
}

float
vt_rbl_min_weight (vt_rbl_t *rbl)
{
  float n = 0.0;
  vt_slist_t *p;
  vt_rbl_weight_t *q;

  for (p = rbl->weights; p; p = p->next) {
    q = (vt_rbl_weight_t *)p->data;
    if (q->weight < n)
      n = q->weight;
  }

  return n;
}
