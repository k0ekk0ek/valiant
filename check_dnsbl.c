/* system includes */
#include <arpa/inet.h>
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
#include "check_dnsbl.h"
#include "thread_pool.h"


// XXX: If I'm to include timeout information... I should also include when it
//      was last checked etc...
// XXX: I probably also need the interval etc...
// XXX: time_t time_slot; // XXX: interval could be statically defined... 
//      we don't have to make everything configurable!
// TIME_FRAME defines how many seconds from start errors incremented.



#define TIME_FRAME (60)
#define MAX_ERRORS (10)

#define HALF_HOUR_IN_SECONDS (1800) /* minimum back off time */

#define BACK_OFF_TIME HALF_HOUR_IN_SECONDS /* 30 minutes in seconds */
// XXX: I'm better of having a max multiplier here!
// unfortunately... a multiplier doesn't speak to anyones imagination...!
#define MAX_BACK_OFF_TIME (86400) /* 1 day in seconds */


#define CHECK_DNSBL_SKIP (0)
#define CHECK_DNSBL_NO_SKIP (1)
#define CHECK_DNSBL_ERROR (2)


int check_dnsbl_chk (check_t *, request_t *, score_t *);


typedef struct check_dnsbl_struct check_dnsbl_t;

struct check_dnsbl_struct {
  char *zone;
  time_t start;
  time_t back_off;
  int errors; /* number of errors within time frame */
  int power; /* number of times dnsbl has been disabled */
  int max_power; /* maximum power... if this is reached... MAX_BACK_OFF_TIME is used instead */
  pthread_rwlock_t *lock;
};

SPF_server_t *spf_server;
thread_pool_t *thread_pool;


/*
 * reverse_inet_addr	- reverse ipaddress string for dnsbl query
 *                        e.g. 1.2.3.4 -> 4.3.2.1
 */
char *
reverse_inet_addr(char *ipstr)
{
	unsigned int ipa, tmp;
	int i;
	int ret;
	struct in_addr inaddr;
	const char *ptr;
	//char tmpstr[INET_ADDRSTRLEN];
	size_t iplen;

	if ((iplen = strlen(ipstr)) > INET_ADDRSTRLEN) {
		g_error ("invalid ipaddress: %s\n", ipstr);
		return NULL;
	}

  char *tmpstr = malloc (iplen+1);
  memset (tmpstr, 0, iplen+1);

	ret = inet_pton(AF_INET, ipstr, &inaddr);
	switch (ret) {
	case -1:
		g_error("reverse_inet_addr: inet_pton");
		return NULL;
		break;
	case 0:
		g_error("not a valid ip address: %s", ipstr);
		return NULL;
		break;
	}

	/* case default */
	ipa = inaddr.s_addr;

	tmp = 0;

	for (i = 0; i < 4; i++) {
		tmp = tmp << 8;
		tmp |= ipa & 0xff;
		ipa = ipa >> 8;
	}

	/*
	 * this tmpstr hack here is because at least FreeBSD seems to handle
	 * buffer lengths differently from Linux and Solaris. Specifically,
	 * with inet_ntop(AF_INET, &tmp, ipstr, iplen) one gets a truncated
	 * address in ipstr in FreeBSD.
	 */
	ptr = inet_ntop(AF_INET, &tmp, tmpstr, INET_ADDRSTRLEN);
	if (!ptr) {
		g_error("inet_ntop");
		return NULL;
	}
	//assert(strlen(tmpstr) == iplen);
	//strncpy(ipstr, tmpstr, iplen);

	return tmpstr;
}


/**
 * COMMENT
 */
int /* seems I must be able to do more than one read lock! */
check_dnsbl_skip (check_t *check)
{
  check_dnsbl_t *extra;
  time_t now;

  extra = (check_dnsbl_t *) check->extra;
  now = time (NULL);


  int ret;
  int skip;

  if ((ret = pthread_rwlock_rdlock (extra->lock))) {
    // error
    return CHECK_DNSBL_ERROR;
  }

  // now find out if we should skip!

  if (extra->back_off && extra->back_off > now) {
    skip = CHECK_DNSBL_SKIP;
  } else {
    skip = CHECK_DNSBL_NO_SKIP;
  }

  if ((ret = pthread_rwlock_unlock (extra->lock))) {
    // should never... ever... happen
    panic ("%s: pthread_rwlock_unlock: returned an error... which isn't possible", __func__);
  }

  return skip;
}


void
check_dnsbl_error (check_t *check)
{
  check_dnsbl_t *extra;
  int ret;

  extra = (check_dnsbl_t *) check->extra;

  time_t now = time(NULL);


  if ((ret = pthread_rwlock_wrlock (extra->lock))) {
    //return CHECK_DNSBL_ERROR;
    return;
  }

  // well ... increase the counter... but should we start over???

  if (now > extra->back_off) {
    /* error counter should only be increased if we're within the allowed time
       frame. */
    if (extra->start > extra->back_off && extra->start > (now - TIME_FRAME)) {
      /* ... */
      if (++extra->errors >= MAX_ERRORS) {
        extra->errors = 0;

        /*
         * The current power must be updated, since the check might not have
         * failed for a long time.
         
          here. The general idea here is to get
         * the difference between back off time and now and divide it by the
         * number of seconds in BACK_OFF_TIME. That gives us the multiplier.
         * then we substrac the value of power either until power reaches 1
         * or the multiplier is smaller than one!
         */
        int power = extra->power;
        double multiplier;
        time_t time_slot = (now - extra->back_off);

        if (extra->power >= extra->max_power) {
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
        extra->power = power;

        if (extra->power < extra->max_power) {
          extra->power >>= 1;
          extra->back_off = now + (BACK_OFF_TIME * extra->power);
        } else {
          extra->power = extra->max_power;
          extra->back_off = now + MAX_BACK_OFF_TIME;
        }
      }

    } else {
      extra->start = time (NULL);
      extra->errors = 1;



      //
    }
  }


  if ((ret = pthread_rwlock_unlock (extra->lock)))
    return;
    //return CHECK_DNSBL_ERROR;

  return;
}


/**
 * This function actually does the real work!
 */
void
check_dnsbl_worker (void *arg)
{
  // 1. SPF_server... just a member here!
  // 2. score... you know... to update etc!
  // 3. check... you know... to update timeout behaviour etc
  // 4. and of course... the zone info... or the request etc!
  // Well... we'll need a structure to hold the information!

  fprintf (stderr, "%s: enter\n", __func__);

  SPF_dns_rr_t		*dns_rr = NULL;

  check_t *check;
  check_dnsbl_t *check_dnsbl;
  request_t *request;
  score_t *score;

  check = ((check_arg_t *) arg)->check;
  check_dnsbl = (check_dnsbl_t *) check->extra;
  request = ((check_arg_t *) arg)->request;
  score = ((check_arg_t *) arg)->score;


  //extern SPF_server *spf_server;

  char *str, *ptr;
  size_t len;

  /* length of the needed buffer is zone + 1 for dot + len for client_ip + 1 for termination! */
  // of course this might only be true for ipv4

  char *rev_inet = reverse_inet_addr (request->client_address);

  len = strlen (rev_inet) + strlen (check_dnsbl->zone) + 10;


  char *buf = malloc (len);
  memset (buf, 0, len);
  sprintf (buf, "%s.%s", rev_inet, check_dnsbl->zone);
  // XXX: handle errors!


  dns_rr = SPF_dns_lookup (spf_server->resolver, buf, ns_t_a, 0);
  // XXX: handle errors!

  // TRY_AGAIN specifies hostname lookup failure!
  //if (dns_rr->herrno == )

  // XXX: need to do some further checking here etc...

fprintf (stderr, "%s: SPF_dns_lookup->herrno: %d\n", __func__, dns_rr->herrno);

  switch (dns_rr->herrno) {
    case NETDB_SUCCESS:
      // great we have a result
      // right ... so ... fetch the score and unlock the score
      fprintf (stderr, "%s: result: success, score: %d\n", __func__, check->plus);
      score_update (score, check->plus);
      break;
    case HOST_NOT_FOUND:
      score_update (score, check->minus);
      fprintf (stderr, "%s: result: failure, score: %d\n", __func__, check->minus);
      break;
    case TRY_AGAIN:
      check_dnsbl_error (check);
      fprintf (stderr, "%s: result: error\n", __func__);
      break;
  }

        fprintf (stderr, "%s: result: success, score: %d\n", __func__, check->plus);

  score_writers_down (score);

  /* evaluate the result like postfix and postfwd different ip's mean different
     things... think of a neat way here! */

  fprintf (stderr, "%s: exit\n", __func__);


  //
  //check_t *check;
  //score_t *score;
  //
  //check = ((score_callback_t *) arg)->check;
  //score = ((score_callback_t *) arg)->score;
  //
  //g_free (arg);
  //
  //
  //switch (status) {
  //  case ARES_SUCCESS:
  //    //g_printf ("%s: it was successfull\n", __func__);
  //    break;
  //  default:
  //    g_printf ("%s: ERROR: %s\n", __func__, ares_strerror (status));
  //    return;
  //    break;
  //}
  //
  //
  // need at least the check and the score here... so a typedef for a struct
  // to hold the two of them will need to be created
  // IMPLEMENT
  // update score based on result
  // get the score and the check
  // free memory occupied by strange struct
  // evaluate result
  // update score
  // remove writer from score
  // bail... score should take over after I'm done
  //
  //score_writers_down (score);
  //g_printf ("%s: writers down\n", __func__);
  return;
}


int
check_dnsbl_chk (check_t *check, request_t *request, score_t *score)
{
  check_arg_t *arg;
  check_dnsbl_t *info;


  fprintf (stderr, "%s: entered", __func__);


  // XXX: implement assertions here!

  info = (check_dnsbl_t *) check->extra;

  score_writers_up (score);


  // now fire the request... aka put it on the queue!
  // XXX: but first create the argument
  //
  arg = check_arg_create (check, request, score);
  // XXX: handle errors!
  thread_pool_task_push (thread_pool, (void *) arg);
  // XXX: handle errors!

  return 0;

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


check_dnsbl_t *
check_dnsbl_alloc ()
{
  check_dnsbl_t *check_dnsbl;

  if (! (check_dnsbl = malloc (sizeof (check_dnsbl_t)))) {
    return NULL;
  }

  if (! (check_dnsbl->lock = malloc (sizeof (pthread_rwlock_t)))) {
    free (check_dnsbl);
    return NULL;
  }

  return check_dnsbl;
}


void
check_dnsbl_free (check_t *check)
{
  // XXX: IMPLEMENT
  return;
}


check_t *
check_dnsbl_create (int plus, int minus, const char *zone)
{
  check_t *check;
  check_dnsbl_t *check_dnsbl;

  fprintf (stderr, "%s: enter\n", __func__);

  if (! (check = check_alloc ())) {
    fprintf (stderr, "%s: error: %s\n", __func__, strerror (errno));
    return NULL;
  }

  check->plus = plus;
  check->minus = minus;
  check->check_fn = &check_dnsbl_chk;
  check->free_fn = &check_dnsbl_free;

  if (! (check_dnsbl = check_dnsbl_alloc ())) {
    free (check); /* must cleanup after ourselves */
    fprintf (stderr, "error1\n");
    return NULL;
  }

  check->extra = (void *) check_dnsbl;

  // now init some stuff
  //extra->lock
  if (pthread_rwlock_init (check_dnsbl->lock, NULL)) {
    // XXX: error occurred! handle it!
  }

  check_dnsbl->zone = (char *) zone;

  // create the thread pool!
  // init the spf_server!

  /* Maximum power is dynamically created because time limit might become
     configurable at some point. */

  if (MAX_BACK_OFF_TIME > INT_MAX)
    fprintf (stderr, "%s: maximum back off time out of range", __func__);
  if (MAX_BACK_OFF_TIME < BACK_OFF_TIME)
    fprintf (stderr, "%s: back off time exceeds maximum value", __func__);
  if (BACK_OFF_TIME < HALF_HOUR_IN_SECONDS)
    fprintf (stderr, "%s: back off time exceeds minumum value", __func__);

  /* The idea here is to get the maximum value for multiplier and find the
     first available power of two. By doing this we have a window of half the
     value and thus every preceding power of two has a lower value */

  // the last power is check using max_back_off_time...
  // so ... what we do here... round it up, then decide what the maximum
  // power should be! we do this by shifting until we reach a higher value...
  // at that point we know enough
  // at this point we have the maximum multiplier
  /* result is exactly */


  unsigned int boundry;
  unsigned int max = ceil ((MAX_BACK_OFF_TIME / BACK_OFF_TIME));

  for (boundry=1; boundry < max; boundry<<=1)
    fprintf (stderr, "%s: boundry: %d, max: %d\n", __func__, boundry, max);

  check_dnsbl->max_power = boundry;

  // XXX: must be done in a different way... this will cause trouble if we have
  //      multiple dnsbl checks... blablabla
  // XXX: should probably be moved to init... but what the heck right...
  thread_pool = thread_pool_create ("dnsbl", 100, &check_dnsbl_worker);
  spf_server = SPF_server_new(SPF_DNS_CACHE, 2);

  fprintf (stderr, "%s: exit\n", __func__);

  return check;
}


/**
 * COMMENT
 */
check_t *
cfg_to_check_dnsbl (cfg_t *section)
{
  check_t *check;

//  char *attribute;
  char *zone;
  int positive;
  int negative;
//  bool fold;

//  if (! (attribute = cfg_getstr (section, "attribute")))
//    panic ("%s: attribute undefined\n", __func__);
  if (! (zone = cfg_getstr (section, "zone")))
    panic ("%s: zone undefined\n", __func__);

  //attribute = strdup (attribute);
  zone = strdup (zone);

  positive = (int) (cfg_getfloat (section, "positive") * 100);
  negative = (int) (cfg_getfloat (section, "negative") * 100);
//  fold = cfg_getbool (section, "case-sensitive");

  g_printf ("%s: zone: %s, positive: %d, negative: %d\n",
  __func__,
  zone,
  positive,
  negative);

  return check_dnsbl_create (positive, negative, zone);
}





















