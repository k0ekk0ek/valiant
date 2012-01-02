/* system includes */
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <spf2/spf.h>
#include <spf2/spf_dns.h>
#include <stdlib.h>

/* valiant includes */
#include "dict_priv.h"
#include "dict_spf.h"

typedef struct _vt_dict_spf vt_dict_spf_t;

struct _vt_dict_spf {
  SPF_server_t *spf_server;
  float pass;
  float fail;
  float soft_fail;
  float neutral;
  float none;
  float perm_error;
  float temp_error;
};

/* only used to make it easier to loop through sections */
typedef struct _vt_dict_spf_result vt_dict_spf_result_t;

struct _vt_dict_spf_result {
  char *name;
  float *weight;
};

/* prototypes */
vt_dict_t *vt_dict_spf_create (vt_dict_type_t *, cfg_t *, cfg_t *,
  vt_error_t *);
int vt_dict_spf_destroy (vt_dict_t *, vt_error_t *);
int vt_dict_spf_check (vt_dict_t *, vt_request_t *, vt_result_t *,
  vt_error_t *);
float vt_dict_spf_max_diff (vt_dict_t *);
float vt_dict_spf_min_diff (vt_dict_t *);

vt_dict_type_t _vt_dict_spf_type = {
  .name = "spf",
  .create_func = &vt_dict_spf_create
};

vt_dict_type_t *
vt_dict_spf_type (void)
{
  return &_vt_dict_spf_type;
}

vt_dict_t *
vt_dict_spf_create (vt_dict_type_t *type,
                    cfg_t *type_sec,
                    cfg_t *dict_sec,
                    vt_error_t *err)
{
  cfg_t *rslt_sec;
  cfg_opt_t *rslt_opt;
  float weight;
  vt_dict_t *dict;
  vt_dict_spf_t *data;
  vt_dict_spf_result_t *rslt;

  assert (type);
  assert (dict_sec);

  if (! (dict = vt_dict_create_common (dict_sec, err)))
    goto failure;
  if (! (data = calloc (1, sizeof (vt_dict_spf_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }
  if (! (data->spf_server = SPF_server_new (SPF_DNS_CACHE, 0))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: SPF_server_new: %s", __func__, strerror (errno));
    goto failure;
  }

  /* only used to make it easier to loop through sections */
  vt_dict_spf_result_t rslts[] = {
    { "pass",       &data->pass       },
    { "fail",       &data->fail       },
    { "soft_fail",  &data->soft_fail  },
    { "neutral",    &data->neutral    },
    { "none",       &data->none       },
    { "perm_error", &data->perm_error },
    { "temp_error", &data->temp_error },
    { NULL,         NULL              }
  };

  weight = cfg_getfloat (dict_sec, "weight");
  for (rslt = rslts; rslt->name; rslt++) {
    if ((rslt_sec = cfg_getnsec (dict_sec, rslt->name, 0))) {
      /* retrieve option instead of value to determine if it's set by the user
         or not. */
      if ((rslt_opt = cfg_getopt (rslt_sec, "weight"))) {
        if (rslt_opt->nvalues && ! (rslt_opt->flags & CFGF_RESET))
          *(rslt->weight) = cfg_opt_getnfloat (rslt_opt, 0);
        else
          *(rslt->weight) = weight;
      } else {
        *(rslt->weight) = weight;
      }
    }
  }

  dict->async = 1;
  dict->data = (void *)data;
  dict->max_diff = vt_dict_spf_max_diff (dict);
  dict->min_diff = vt_dict_spf_min_diff (dict);
  dict->check_func = &vt_dict_spf_check;
  dict->destroy_func = &vt_dict_spf_destroy;

  return dict;
failure:
  (void)vt_dict_spf_destroy (dict, NULL);
  return NULL;
}

int
vt_dict_spf_destroy (vt_dict_t *dict, vt_error_t *err)
{
  vt_dict_spf_t *data;

  if (dict) {
    data = (vt_dict_spf_t *)dict->data;
    if (data) {
      if (data->spf_server)
        SPF_server_free (data->spf_server);
      free (data);
    }
    return vt_dict_destroy_common (dict, err);
  }
  return 0;
}

int
vt_dict_spf_check (vt_dict_t *dict,
                   vt_request_t *req,
                   vt_result_t *res,
                   vt_error_t *err)
{
  char *client_address, *helo_name, *sender;
  float weight;
  SPF_errcode_t ret;
  SPF_request_t *spf_request = NULL;
  SPF_response_t *spf_response = NULL;
  vt_dict_spf_t *data;

  assert (dict);
  assert (req);
  assert (res);
  data = (vt_dict_spf_t *)dict->data;
  assert (data);

  client_address = vt_request_mbrbyid (req, VT_REQUEST_MEMBER_CLIENT_ADDRESS);
  helo_name = vt_request_mbrbyid (req, VT_REQUEST_MEMBER_HELO_NAME);
  sender = vt_request_mbrbyid (req, VT_REQUEST_MEMBER_SENDER);

  if (! client_address || ! helo_name || ! sender)
    return (0);

  /* SPF_request_new malloc's and returns directly if it fails. */
  if (! (spf_request = SPF_request_new (data->spf_server))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: SPF_request_new: %s", __func__, strerror (errno));
    goto failure;
  }
  if ((ret = SPF_request_set_ipv4_str (spf_request, client_address)) != 0) {
    vt_set_error (err, VT_ERR_BADREQUEST);
    vt_error ("%s: SPF_request_set_ipv4_str: %s", __func__, SPF_strerror (ret));
    goto failure;
  }
  if ((ret = SPF_request_set_helo_dom (spf_request, helo_name)) != 0) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: SPF_request_set_helo_dom: %s", __func__, SPF_strerror (ret));
    goto failure;
  }
  if ((ret = SPF_request_set_env_from (spf_request, sender)) != 0) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: SPF_request_set_env_from: %s", __func__, SPF_strerror (ret));
    goto failure;
  }

  switch (SPF_request_query_mailfrom (spf_request, &spf_response)){
    case SPF_E_SUCCESS:
      break;
    case SPF_E_NO_MEMORY:
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: SPF_request_query_mailfrom: %s", __func__, SPF_strerror (SPF_E_NO_MEMORY));
      goto failure;
    default:
      break;
  }

  switch (SPF_response_result (spf_response)) {
    case SPF_RESULT_INVALID:
      vt_panic ("%s: SPF_response_result gave SPF_RESULT_INVALID", __func__);
    case SPF_RESULT_PASS:
      weight = data->pass;
      break;
    case SPF_RESULT_FAIL:
      weight = data->fail;
      break;
    case SPF_RESULT_SOFTFAIL:
      weight = data->soft_fail;
      break;
    case SPF_RESULT_NEUTRAL:
      weight = data->neutral;
      break;
    case SPF_RESULT_NONE:
      weight = data->none;
      break;
    case SPF_RESULT_TEMPERROR:
      weight = data->temp_error;
      break;
    case SPF_RESULT_PERMERROR:
      weight = data->perm_error;
      break;
    default:
      vt_panic ("%s: SPF_response_result gave unknown result", __func__);
  }

  vt_result_update (res, dict->pos, weight);
  SPF_response_free (spf_response);

  return 0;
failure:
  if (spf_request)
    SPF_request_free (spf_request);
  if (spf_response)
    SPF_response_free (spf_response);
  return -1;
}

float
vt_dict_spf_max_diff (vt_dict_t *dict)
{
  float weight;
  vt_dict_spf_t *data;

  assert (dict);
  data = (vt_dict_spf_t *)dict->data;
  assert (data);

  /* it's ugly, I know */
  weight = data->pass;
  if (weight < data->fail)
    weight = data->fail;
  if (weight < data->soft_fail)
    weight = data->soft_fail;
  if (weight < data->neutral)
    weight = data->neutral;
  if (weight < data->none)
    weight = data->none;
  if (weight < data->perm_error)
    weight = data->perm_error;
  if (weight < data->temp_error)
    weight = data->temp_error;

  return weight;
}

float
vt_dict_spf_min_diff (vt_dict_t *dict)
{
  float weight;
  vt_dict_spf_t *data;

  assert (dict);
  data = (vt_dict_spf_t *)dict->data;
  assert (data);

  /* again, it's ugly, I know */
  weight = data->pass;
  if (weight > data->fail)
    weight = data->fail;
  if (weight > data->soft_fail)
    weight = data->soft_fail;
  if (weight > data->neutral)
    weight = data->neutral;
  if (weight > data->none)
    weight = data->none;
  if (weight > data->perm_error)
    weight = data->perm_error;
  if (weight > data->temp_error)
    weight = data->temp_error;

  return weight;
}
