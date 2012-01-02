/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "dict_priv.h"
#include "dict_rhsbl.h"
#include "rbl.h"

/* prototypes */
vt_dict_t *vt_dict_rhsbl_create (vt_dict_type_t *, cfg_t *, cfg_t *,
  vt_error_t *);
int vt_dict_rhsbl_destroy (vt_dict_t *, vt_error_t *);
int vt_dict_rhsbl_check (vt_dict_t *, vt_request_t *, vt_result_t *,
  vt_error_t *);

vt_dict_type_t _vt_dict_rhsbl_type {
  .name = "rhsbl",
  .create_func = &vt_dict_rhsbl_create
};

vt_dict_type_t *
vt_dict_rhsbl_type (void)
{
  return &_vt_dict_rhsbl_type;
}

vt_dict_t *
vt_dict_rhsbl_create (vt_dict_type_t *type,
                      cfg_t *type_sec,
                      cfg_t *dict_sec,
                      vt_error_t *err)
{
  vt_dict_t *dict;
  vt_rbl_t *rbl;

  assert (type);
  assert (dict_sec);

  if (! (dict = vt_dict_create_common (dict_sec, err)) ||
      ! (rbl = vt_rbl_create (dict_sec, err)))
    goto failure;

  dict->async = 1;
  dict->data = (void *)rbl;
  dict->max_diff = vt_rbl_max_weight (rbl);
  dict->min_diff = vt_rbl_min_weight (rbl);
  dict->check_func = &vt_dict_rhsbl_check;
  dict->destroy_func = &vt_dict_rhsbl_destroy;

  return dict;
failure:
  (void)vt_dict_dnsbl_destroy (dict, NULL);
  return NULL;
}

int
vt_dict_rhsbl_destroy (vt_dict_t *dict, vt_error_t *err)
{
  if (dict) {
    if (vt_rbl_destroy ((vt_rbl_t *)dict->data, err) != 0)
      return -1;
    if (vt_dict_destroy_common (dict, err) != 0)
      return -1;
  }
  return 0;
}

int
vt_dict_rhsbl_check (vt_dict_t *dict,
                     vt_request_t *req,
                     vt_result_t *res,
                     vt_error_t *err)
{
  char *sender_domain;
  char query[HOST_NAME_MAX];
  int len;
  vt_rbl_t *rbl;

  assert (dict);
  assert (req);
  assert (res);
  rbl = (vt_rbl_t *)dict->data;
  assert (rbl);

  sender_domain = vt_buf_str (&req->sender_domain);

  if (sender_domain) {
    len = snprintf (query, HOST_NAME_MAX, "%s.%s", sender_domain, rbl->zone);
    if (len >= HOST_NAME_MAX)
      vt_panic ("%s: rhsbl query exceeded maximum hostname length", __func__);
    return vt_rbl_check (dict, query, result, err);
  } else {
    vt_result_update (res, dict->pos, 0.0);
  }
  return 0;
}
