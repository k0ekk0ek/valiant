/* system includes */
#include <assert.h>
#include <arpa/inet.h>
#include <confuse.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <spf2/spf.h>
#include <spf2/spf_dns.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

/* valiant includes */
#include "check.h"
#include "check_dnsbl.h"
#include "slist.h"
#include "thread_pool.h"
#include "utils.h"

/* defines */
#define TIME_FRAME (60)
#define MAX_ERRORS (10)
#define HALF_HOUR_IN_SECONDS (1800) /* minimum back off time */
#define BACK_OFF_TIME HALF_HOUR_IN_SECONDS /* half hour in seconds */
#define MAX_BACK_OFF_TIME (86400) /* 1 day in seconds */

#define CHECK_DNSBL_SKIP (0)
#define CHECK_DNSBL_NO_SKIP (1)
#define CHECK_DNSBL_ERROR (2)

typedef struct dnsbl_weight_struct dnsbl_weight_t;

struct dnsbl_weight_struct {
  unsigned long subnet;
  unsigned long netmask;
  int weight;
};

typedef struct check_info_dnsbl_struct check_info_dnsbl_t;

struct check_info_dnsbl_struct {
  char *zone;
  time_t start;
  time_t back_off;
  slist_t *weights;
  int errors; /* number of errors within time frame */
  int power; /* number of times dnsbl has been disabled */
  int max_power; /* maximum power... if this is reached... MAX_BACK_OFF_TIME is used instead */
  pthread_rwlock_t *lock;
};

SPF_server_t *spf_server;
thread_pool_t *thread_pool;

/* prototypes */
dnsbl_weight_t *dnsbl_weight_create (const char *, float);
int slist_sort_dnsbl_weight (void *, void *);
int check_dnsbl_free (check_t *);





int
check_dnsbl_skip (check_t *check)
{
  check_info_dnsbl_t *info;
  int errnum, skip;
  time_t now;

  assert (check);

  info = (check_info_dnsbl_t *) check->info;
  now = time (NULL);

  if ((errnum = pthread_rwlock_rdlock (info->lock))) {
    error ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (errnum));
    return CHECK_DNSBL_ERROR;
  }

  if (info->back_off && info->back_off > now) {
    skip = CHECK_DNSBL_SKIP;
  } else {
    skip = CHECK_DNSBL_NO_SKIP;
  }

  if ((errnum = pthread_rwlock_unlock (info->lock))) {
    panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (errnum));
  }

  return skip;
}


void
check_dnsbl_error (check_t *check)
{
  check_info_dnsbl_t *info;
  int ret;

  info = (check_info_dnsbl_t *) check->info;

  time_t now = time(NULL);


  if ((ret = pthread_rwlock_wrlock (info->lock))) {
    //return CHECK_DNSBL_ERROR;
    return;
  }

  // well ... increase the counter... but should we start over???

  if (now > info->back_off) {
    /* error counter should only be increased if we're within the allowed time
frame. */
    if (info->start > info->back_off && info->start > (now - TIME_FRAME)) {
      /* ... */
      if (++info->errors >= MAX_ERRORS) {
        info->errors = 0;

        /*
* The current power must be updated, since the check might not have
* failed for a long time.
here. The general idea here is to get
* the difference between back off time and now and divide it by the
* number of seconds in BACK_OFF_TIME. That gives us the multiplier.
* then we substrac the value of power either until power reaches 1
* or the multiplier is smaller than one!
*/
        int power = info->power;
        double multiplier;
        time_t time_slot = (now - info->back_off);

        if (info->power >= info->max_power) {
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
        info->power = power;

        if (info->power < info->max_power) {
          info->power >>= 1;
          info->back_off = now + (BACK_OFF_TIME * info->power);
        } else {
          info->power = info->max_power;
          info->back_off = now + MAX_BACK_OFF_TIME;
        }
      }

    } else {
      info->start = time (NULL);
      info->errors = 1;
    }
  }


  if ((ret = pthread_rwlock_unlock (info->lock)))
    return;
    //return CHECK_DNSBL_ERROR;

  return;
}

void
check_dnsbl_worker (void *arg)
{
  unsigned long address;
  char query[HOST_NAME_MAX], reverse[INET_ADDRSTRLEN];
  check_t *check;
  check_info_dnsbl_t *info;
  dnsbl_weight_t *weight, *heaviest;
  int i, n;
  request_t *request;
  score_t *score;
  slist_t *cur;
  SPF_dns_rr_t *dns_rr;

  check = ((check_arg_t *) arg)->check;
  info = (check_info_dnsbl_t *) check->info;
  request = ((check_arg_t *) arg)->request;
  score = ((check_arg_t *) arg)->score;

  n = reverse_inet_addr (request->client_address, reverse, INET_ADDRSTRLEN);
  if (n < 0)
    panic ("%s: reverse_inet_addr: %s", __func__, strerror (errno));

  n = snprintf (query, HOST_NAME_MAX, "%s.%s", reverse, info->zone);
  if (n < 0 || n > HOST_NAME_MAX)
    panic ("%s: dnsbl query exceeded maximum hostname length", __func__);

  dns_rr = SPF_dns_lookup (spf_server->resolver, query, ns_t_a, 0);

  switch (dns_rr->herrno) {
    case NO_DATA:
      /* This ugly hack is necessary because of what I think is a bug in
         SPF_dns_lookup_resolve. */
      for (i=0; i < dns_rr->rr_buf_num && dns_rr->rr[i]; i++)
        ;

      if (i)
        dns_rr->num_rr = i;

      /* fall through */
    case NETDB_SUCCESS:
      heaviest = NULL;
      for (i=0; i < dns_rr->num_rr && dns_rr->rr[i]; i++) {
        address = ntohl (dns_rr->rr[i]->a.s_addr);

        for (cur=info->weights; cur; cur=cur->next) {
          weight = (dnsbl_weight_t *)cur->data;

          if ((address & weight->netmask) == (weight->subnet & weight->netmask)) {
            if (heaviest == NULL || heaviest->weight < weight->weight)
              heaviest = weight;
          }
        }
      }

      if (heaviest)
        score_update (score, heaviest->weight);
      break;
    case TRY_AGAIN: // SERVFAIL
      check_dnsbl_error (check);
      break;
  }

  SPF_dns_rr_free (dns_rr);
  score_unlock (score);
  return;
}


int
check_dnsbl (check_t *check, request_t *request, score_t *score)
{
  check_arg_t *arg;
  check_info_dnsbl_t *info;


  info = (check_info_dnsbl_t *) check->info;

  score_writers_up (score);


  // now fire the request... aka put it on the queue!
  // XXX: but first create the argument
  //
  arg = check_arg_create (check, request, score);
  // XXX: handle errors!
  thread_pool_task_push (thread_pool, (void *) arg);
  // XXX: handle errors!

  return 0;

  //fprintf (stderr, "%s: entered", __func__);
  // XXX: implement assertions here!
  //g_printf ("%s: hostname: %s\n", __func__, ptr);
  //score_callback_t *arg = g_new0 (score_callback_t, 1);
  //arg->check = check;
  //arg->score = score;

  //g_printf ("%s: memaddr: %X\n", __func__, resolver);
  //ares_gethostbyname (*resolver, "www.tweakers.net", PF_INET, &check_dnsbl_callback, (gpointer) arg);
  //ares_gethostbyname (resolver, ptr, PF_INET, &check_dnsbl_callback, (gpointer) arg);
  //
  //return 0;
  // bla!
  // reverse the ip address
  // attach the zone
  // concatenate both
  // fire request!
  // update score writers first!
}


#define check_dnsbl_alloc() (check_alloc (sizeof (check_info_dnsbl_t)))

check_t *
check_dnsbl_create (cfg_t *section)
{
  cfg_t *in;
  check_t *check;
  check_info_dnsbl_t *info;
  dnsbl_weight_t *weight;
  slist_t *weights, *cur;

  char *type, *zone;
  unsigned int i, n;

fprintf (stderr, "%s (%d)\n", __func__, __LINE__);

  assert (section);
//  assert (MAX_BACK_OFF_TIME < INT_MAX);
//  assert (MAX_BACK_OFF_TIME > BACK_OFF_TIME);
//  assert (BACK_OFF_TIME > HALF_HOUR_IN_SECONDS);

  check = NULL;
  info = NULL;
  weights = NULL;

  /* allocate and initialize check */
  if ((check = check_dnsbl_alloc ()) == NULL) {
    error ("%s: check_dnsbl_alloc: %s", __func__, strerror (errno));
    goto failure;
  }
fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  info = (check_info_dnsbl_t *) check->info;
fprintf (stderr, "%s (%d)\n", __func__, __LINE__);

  info->lock = malloc (sizeof (pthread_rwlock_t));

  if (pthread_rwlock_init (info->lock, NULL)) {
    error ("%s: pthread_rwlock_info: %s", __func__, strerror (errno));
    goto failure;
  }
fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  if ((type = cfg_getstr (section, "type")) == NULL) {
    error ("%s: type should be one of dnsbl, dnswl, rhsbl", __func__);
    goto failure;
  }
fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  if (strncasecmp (type, "dnsbl", 5) != 0 &&
      strncasecmp (type, "dnswl", 5) != 0 &&
      strncasecmp (type, "rhsbl", 5) != 0) {
    error ("%s: type should be one of dnsbl, dnswl, rhsbl", __func__);
    goto failure;
  }

  if ((zone = cfg_getstr (section, "zone")) == NULL) {
    error ("%s: zone empty, rendering check useless", __func__);
    goto failure;
  }

  if (cfg_getbool (section, "ipv6")) {
    warning ("%s: IPv6 not yet supported, ignoring option", __func__);
  }

  if (! cfg_getbool (section, "ipv4")) {
    error ("%s: IPv6 not yet supported and IPv4 disabled rendering check useless", __func__);
    goto failure;
  }

  /* parse "in" sections and append them to weights */
  for (i=0, n=cfg_size (section, "in"); i < n; i++) {
    in = cfg_getnsec (section, "in", 0);

    if (in) {
      weight = dnsbl_weight_create (cfg_title (in), cfg_getfloat (in, "weight"));
      if (weight == NULL)
        goto failure;

      // FIXME: handle errors
      cur = slist_append (weights, weight);
      if (weights == NULL)
        weights = cur;
    }
  }

  /* parse section weight and append it to weights */
  weight = dnsbl_weight_create ("127.0.0.0/8", cfg_getfloat (section, "weight"));
  if (weight == NULL)
    goto failure;

  // FIXME: handle errors
  cur = slist_append (weights, weight);
  if (weights == NULL)
    weights = cur;
  else
    weights = slist_sort (weights, &slist_sort_dnsbl_weight);

  if (strncasecmp (type, "dnsbl", 5) == 0) {
    check->check_fn = &check_dnsbl;
    check->free_fn = &check_dnsbl_free;
  } else {
    error ("%s: type %s not yet supported", __func__, type);
    goto failure;
  }

  if ((info->zone = strdup (zone)) == NULL) {
    error ("%s: strdup: %s", __func__, strerror (errno));
    goto failure;
  }

  for (i=1, n = ceil ((MAX_BACK_OFF_TIME / BACK_OFF_TIME)); i < n; i<<=1)
    ;

  info->weights = weights;
  info->max_power = i;

  // FIXME: should be done differently
  thread_pool = thread_pool_create ("dnsbl", 100, &check_dnsbl_worker);
  spf_server = SPF_server_new(SPF_DNS_CACHE, 2);

  return check;

failure:
  // FIXME: not freeing everything
  check_dnsbl_free (check);

  return NULL;
}

#undef check_dnsbl_alloc

int
check_dnsbl_free (check_t *check)
{
  // FIXME: implement
  return 0;
}

dnsbl_weight_t *
dnsbl_weight_create (const char *network, float weight)
{
  // FIXME: cleanup
  dnsbl_weight_t *w;
  const char *func = "dnsbl_weight_create";
  char addr[INET_ADDRSTRLEN], *p1, *p2;
  int r, i;
  struct in_addr address;
  unsigned long netmask, subnet;
  long bits;

  assert (network);

  for (p1=(char*)network, p2=addr;
      *p1 != '\0' && *p1 != '/' && p2 < (addr+INET_ADDRSTRLEN);
      *p2++ = *p1++)
    ;

  if (*p1 == '/') {
    bits = strtol ((++p1), NULL, 10);

    if (bits < 0 || bits > 32 || (bits == 0 && errno == EINVAL)) {
      errno = EINVAL;
      error ("%s: invalid presentation of network/bitmask: %s", func, network);
      return NULL;
    }
  } else if (*p1 != '\0') {
    error ("%s: invalid presentation of network/bitmask: %s", func, network);
    return NULL;
  }

 *p2 = '\0';

fprintf (stderr, "and the network is %s\n", addr);

  r = inet_pton (AF_INET, addr, &address);
  if (r <= 0) {
    if (r == 0)
      error ("%s: invalid notation... error must be fancied", func);
    else
      error ("%s: %s", func, strerror (errno));
    return NULL;
  }

  for (netmask=0, i=0; i < bits; i++)
    netmask |= 1 << (31 - i);


fprintf (stderr, "%s (%d): network: %s\n", __func__, __LINE__, inet_ntop(AF_INET, &address, addr, INET_ADDRSTRLEN));
  subnet = ntohl (address.s_addr);
fprintf (stderr, "%s (%d): network: %s\n", __func__, __LINE__, inet_ntop(AF_INET, &subnet, addr, INET_ADDRSTRLEN));
  // XXX: handle possible errors!
  //
  //
  w = malloc (sizeof (dnsbl_weight_t));
  if (w == NULL) {
    error ("%s: malloc: %s", func, strerror (errno));
    return NULL;
  }

  w->subnet = subnet;
  w->netmask = netmask;
  w->weight = (int) weight;

  return w;
}

void
dnsbl_weight_free (void *weight)
{
  if (weight)
    free (weight);
  return;
}

int
slist_sort_dnsbl_weight (void *p1, void *p2)
{
  dnsbl_weight_t *w1, *w2;

  w1 = (dnsbl_weight_t *)p1;
  w2 = (dnsbl_weight_t *)p2;

  if (w1->netmask > w2->netmask)
    return -1;
  if (w1->netmask < w2->netmask)
    return  1;

  return 0;
}









// XXX: If I'm to include timeout information... I should also include when it
// was last checked etc...
// XXX: I probably also need the interval etc...
// XXX: time_t time_slot; // XXX: interval could be statically defined...
// we don't have to make everything configurable!
// TIME_FRAME defines how many seconds from start errors incremented.

// XXX: I'm better of having a max multiplier here!
// unfortunately... a multiplier doesn't speak to anyones imagination...!



  /* The idea here is to get the maximum value for multiplier and find the
first available power of two. By doing this we have a window of half the
value and thus every preceding power of two has a lower value */

  // the last power is check using max_back_off_time...
  // so ... what we do here... round it up, then decide what the maximum
  // power should be! we do this by shifting until we reach a higher value...
  // at that point we know enough
  // at this point we have the maximum multiplier
  /* result is exactly */

/*
check spamhaus {
  type = dnsbl
  zone = zen.spamhaus.org
  in 127.0.0.4/30 {
    weight = 4
  }
  in 127.0.0.2/32 {
    weight = 8
  }
  # same as in 127.0.0.0/8 { weight = 1 }
  weight = 1
}
*/


  // XXX: must be done in a different way... this will cause trouble if we have
  // multiple dnsbl checks... blablabla
  // XXX: should probably be moved to init... but what the heck right...

 /* seems I must be able to do more than one read lock! */





































