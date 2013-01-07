/* system includes */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* valiant includes */
#include "rbl.h"
#include "slist.h"
#include "state.h"

typedef struct _vt_rbl_weight vt_rbl_weight_t;

struct _vt_rbl_weight {
  unsigned long network;
  unsigned long netmask;
  float weight;
};

/* prototypes */
void vt_rbl_error (vt_rbl_t *);
vt_rbl_weight_t *vt_rbl_weight_create (const char *, float, vt_error_t *);
int vt_rbl_weight_destroy (void *);
int vt_rbl_weight_sort (void *, void *);
int vt_rbl_weight (vt_rbl_t *, int);

vt_rbl_t *
vt_rbl_create (cfg_t *sec, vt_error_t *err)
{
  cfg_t *in;
  vt_rbl_t *rbl;
  vt_rbl_weight_t *weight;
  vt_slist_t *root, *next;
  char *fmt, *network, *type, *zone;
  float points;
  int i, n;
  int ret;

  root = NULL;

  if (! (rbl = calloc (1, sizeof (vt_rbl_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }
  if (vt_state_init (&rbl->back_off, 1, err)) {
    goto failure;
  }
  /* specifying debug value larger than one changes the behaviour of
     SPF_server, don't do that */
  if (! (rbl->spf_server = SPF_server_new (SPF_DNS_CACHE, 0))) {
    fmt = "%s: SPF_server_new: %s";
    if (errno != ENOMEM)
      vt_panic (fmt, __func__, strerror (errno));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error (fmt, __func__, strerror (errno));
    goto failure;
  }

  zone = cfg_getstr (sec, "zone");

  if (! (rbl->zone = strdup (zone))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    goto failure;
  }

  network = "127.0.0.0/8";
  points = cfg_getfloat (sec, "weight");
  i = 0;
  n = cfg_size (sec, "in");

  do {
    if (! (weight = vt_rbl_weight_create (network, points, err)))
      goto failure;

    if ((next = vt_slist_append (root, weight)) == NULL) {
      vt_rbl_weight_destroy (weight);
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: slist_append: %s", __func__, strerror (errno));
      goto failure;
    }

    if (root == NULL)
      root = next;
    if (i >= n)
      break;

    in = cfg_getnsec (sec, "in", i++);
    network = (char *)cfg_title (in);
    points = cfg_getfloat (in, "weight");
  } while (in);

  rbl->weights = vt_slist_sort (root, &vt_rbl_weight_sort);

  return rbl;
failure:
  (void)vt_rbl_destroy (rbl, NULL);
  vt_slist_free (root, &vt_rbl_weight_destroy, 0);
  return NULL;
}

int
vt_rbl_destroy (vt_rbl_t *rbl, vt_error_t *err)
{
  int ret;

  if (rbl) {
    if (rbl->spf_server)
      SPF_server_free (rbl->spf_server);

    (void)vt_state_deinit (&rbl->back_off, NULL);

    if (rbl->zone)
      free (rbl->zone);
    if (rbl->weights)
      vt_slist_free (rbl->weights, &vt_rbl_weight_destroy, 0);
    free (rbl);
  }

  return (0);
}

int
vt_rbl_check (vt_dict_t *dict, const char *query, vt_result_t *result, int pos,
  vt_error_t *err)
{
  int i;
  vt_rbl_t *rbl;
  vt_rbl_weight_t *weight, *heaviest;
  vt_slist_t *cur;
  SPF_dns_rr_t *dns_rr;
  unsigned long address;

  rbl = (vt_rbl_t *)dict->data;
  dns_rr = SPF_dns_lookup (rbl->spf_server->resolver, query, ns_t_a, 0);

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

          // FIXME: must be done differently
          if ((address & weight->netmask) == (weight->network & weight->netmask)) {
            if (heaviest == NULL || heaviest->weight < weight->weight)
              heaviest = weight;
          }
        }
      }

      if (heaviest) {
        vt_error ("%s:%d: query: %s, weight: %f", __func__, __LINE__, query, heaviest->weight);
        vt_result_update (result, pos, heaviest->weight);
      }
      break;
    case TRY_AGAIN: // SERVFAIL
      vt_state_error (&rbl->back_off);
      break;
  }

  SPF_dns_rr_free (dns_rr);

  return;
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

int
vt_rbl_max_weight (vt_rbl_t *rbl)
{
  int n = 0.0;
  vt_slist_t *p;
  vt_rbl_weight_t *q;

  for (p = rbl->weights; p; p = p->next) {
    q = (vt_rbl_weight_t *)p->data;
    if (q->weight > n)
      n = q->weight;
  }

  return n;
}

int
vt_rbl_min_weight (vt_rbl_t *rbl)
{
  int n = 0.0;
  vt_slist_t *p;
  vt_rbl_weight_t *q;

  for (p = rbl->weights; p; p = p->next) {
    q = (vt_rbl_weight_t *)p->data;
    if (q->weight < n)
      n = q->weight;
  }

  return n;
}
