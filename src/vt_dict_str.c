/* system includes */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "dict_priv.h"
#include "dict_str.h"

/* definitions */
typedef struct _vt_dict_str vt_dict_str_t;

struct _vt_dict_str {
  vt_request_member_t member;
  char *pattern;
  int nocase;
  int invert;
  float weight;
};

/* prototypes */
vt_dict_t *vt_dict_str_create (vt_dict_type_t *, cfg_t *, cfg_t *, vt_error_t *);
int vt_dict_str_destroy (vt_dict_t *, vt_error_t *);
int vt_dict_stat_str_check (vt_dict_t *, vt_request_t *, vt_result_t *, int,
  vt_error_t *);
int vt_dict_dyn_str_check (vt_dict_t *, vt_request_t *, vt_result_t *, int,
  vt_error_t *);
int vt_dict_str_weight (vt_dict_t *, int);

vt_dict_type_t _vt_dict_str_type = {
  .name = "str",
  .create_func = &vt_dict_str_create,
};

vt_dict_type_t *
vt_dict_str_type (void)
{
  return &_vt_dict_str_type;
}

vt_dict_t *
vt_dict_str_create (vt_dict_type_t *type,
                    cfg_t *type_sec,
                    cfg_t *dict_sec,
                    vt_error_t *err)
{
  char *pattern;
  int ret;
  vt_dict_t *dict;
  vt_dict_str_t *data;

  if (! (dict = vt_dict_create_common (dict_sec, err)))
    goto failure;
  if (! (data = calloc (1, sizeof (vt_dict_str_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  dict->data = (void *)data;
  data->member = vt_request_mbrtoid (cfg_getstr (dict_sec, "member"));
  data->invert = cfg_getbool (dict_sec, "invert") ? 1 : 0;
  data->nocase = cfg_getbool (dict_sec, "nocase") ? 1 : 0;
  data->weight = cfg_getfloat (dict_sec, "weight");

  pattern = cfg_getstr (dict_sec, "pattern");
  if (vt_dict_dynamic_pattern (pattern)) {
    dict->check_func = &vt_dict_dyn_str_check;
    data->pattern = strdup (pattern);
  } else {
    dict->check_func = &vt_dict_stat_str_check;
    data->pattern = vt_dict_unescape_pattern (pattern);
  }

  if (data->pattern == NULL) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto failure;
  }

  dict->max_diff = data->weight > 0.0 ? data->weight : 0.0;
  dict->min_diff = data->weight < 0.0 ? data->weight : 0.0;
  dict->destroy_func = &vt_dict_str_destroy;

  return dict;
failure:
  (void)vt_dict_str_destroy (dict, NULL);
  return NULL;
}

int
vt_dict_str_destroy (vt_dict_t *dict, vt_error_t *err)
{
  vt_dict_str_t *data;

  if (dict) {
    if (dict->data) {
      data = (vt_dict_str_t *)dict->data;
      if (data->pattern)
        free (data->pattern);
      free (data);
    }
    return vt_dict_destroy_common (dict, err);
  }
  return 0;
}

int
vt_dict_stat_str_check (vt_dict_t *dict,
                        vt_request_t *req,
                        vt_result_t *res,
                        int pos,
                        vt_error_t *err)
{
  char *member;
  float weight;
  int (*func)(const char *, const char *);
  vt_dict_str_t *data;

  assert (dict);
  assert (req);
  assert (res);
  data = (vt_dict_str_t *)dict->data;
  assert (data);

  if (! (member = vt_request_mbrbyid (req, data->member))) {
    if (data->invert)
      weight = data->weight;
    else
      weight = 0.0;
    goto update;
  }

  if (data->nocase)
    func = &strcasecmp;
  else
    func = &strcmp;

  if (func (member, data->pattern) == 0) {
    if (data->invert)
      weight = 0.0;
    else
      weight = data->weight;
  } else {
    if (data->invert)
      weight = data->weight;
    else
      weight = 0.0;
  }

update:
  vt_debug ("%s: pos: %d, weight: %f", __func__, pos, weight);
  vt_result_update (res, pos, weight);
  return 0;
}

#define chrcasecmp(b,c1,c2) \
  ((c1) == (c2) || ((b) && tolower ((c1)) == tolower ((c2))))

int
vt_dict_dyn_str_check (vt_dict_t *dict,
                       vt_request_t *req,
                       vt_result_t *res,
                       int pos,
                       vt_error_t *err)
{
  char *member;
  char *ptr1, *ptr2, *str;
  float weight;
  vt_dict_str_t *data;

  assert (dict);
  assert (req);
  assert (res);
  data = (vt_dict_str_t *)dict->data;
  assert (data);

  if (! (member = vt_request_mbrbyid (req, data->member))) {
    if (data->invert)
      weight = data->weight;
    else
      weight = 0.0;
    goto update;
  }

  for (ptr1 = data->pattern; *member && *ptr1; ptr1++) {
    if (*ptr1 == '%' && *(++ptr1) != '%') {
      if (! (ptr2 = strchr (ptr1, '%')) ||
          ! (str = vt_request_mbrbynamen (req, ptr1, (size_t)(ptr2 - ptr1))))
        vt_panic ("%s: bad pattern %s", __func__, data->pattern);

      for (ptr1 = ptr2; *member && chrcasecmp (data->nocase, *member, *str); member++, str++)
        ;

      if (*str != '\0')
        break;
    } else if (chrcasecmp (data->nocase, *member, *ptr1)) {
      member++;
    } else {
      break;
    }
  }

  if (*member == *ptr1) {
    if (data->invert)
      weight = 0.0;
    else
      weight = data->weight;
  } else {
    if (data->invert)
      weight = data->weight;
    else
      weight = 0.0;
  }

update:
  vt_result_update (res, pos, data->weight);
  return 0;
}

#undef chrcasecmp
