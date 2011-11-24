/* system includes */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "check_priv.h"
#include "check_str.h"

/* definitions */
typedef struct _vt_check_str vt_check_str_t;

struct _vt_check_str {
  vt_request_member_t member;
  char *pattern;
  int nocase;
  int negate;
  int weight;
};

/* prototypes */
/* NOTE: "normal" checks and "nocase" checks are split up because I want to
   support internationalized domain names (IDN) eventually. */
vt_check_t *vt_check_str_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
int vt_check_str_destroy (vt_check_t *, vt_error_t *);
int vt_check_static_str_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_error_t *);
int vt_check_static_str_check_nocase (vt_check_t *, vt_request_t *,
  vt_score_t *, vt_error_t *);
int vt_check_dynamic_str_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_error_t *);
int vt_check_dynamic_str_check_nocase (vt_check_t *, vt_request_t *,
  vt_score_t *, vt_error_t *);
int vt_check_str_weight (vt_check_t *, int);

const vt_check_type_t _vt_check_str_type = {
  .name = "str",
  .flags = VT_CHECK_TYPE_FLAG_NONE,
  .init_func = NULL,
  .deinit_func = NULL,
  .create_check_func = &vt_check_str_create,
};

const vt_check_type_t *
vt_check_str_type (void)
{
  return &_vt_check_str_type;
}

vt_check_t *
vt_check_str_create (const vt_map_list_t *list, cfg_t *sec, vt_error_t *err)
{
  char *pattern;
  int ret;
  vt_check_t *check = NULL;
  vt_check_str_t *data = NULL;

  if (! (check = vt_check_create (list, sec, err)))
    goto FAILURE;
  if (! (data = calloc (1, sizeof (vt_check_str_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return NULL;
  }

  check->data = (void *)data;
  data->member = vt_request_mbrtoid (cfg_getstr (sec, "member"));
  data->negate = cfg_getbool (sec, "negate") ? 1 : 0;
  data->nocase = cfg_getbool (sec, "nocase") ? 1 : 0;
  data->weight = cfg_getfloat (sec, "weight");

  pattern = cfg_getstr (sec, "pattern");
  if (vt_check_dynamic_pattern (pattern)) {
    if (data->nocase)
      check->check_func = &vt_check_dynamic_str_check_nocase;
    else
      check->check_func = &vt_check_dynamic_str_check;
    data->pattern = strdup (pattern);
  } else {
    if (data->nocase)
      check->check_func = &vt_check_static_str_check_nocase;
    else
      check->check_func = &vt_check_static_str_check;
    data->pattern = vt_check_unescape_pattern (pattern);
  }

  if (data->pattern == NULL) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  check->prio = 1;
  check->max_weight = data->weight > 0 ? data->weight : 0;
  check->min_weight = data->weight < 0 ? data->weight : 0;
  check->destroy_func = &vt_check_str_destroy;

  return check;
FAILURE:
  (void)vt_check_str_destroy (check, NULL);
  return NULL;
}

int
vt_check_str_destroy (vt_check_t *check, vt_error_t *err)
{
  vt_check_str_t *data;

  if (check) {
    if (check->data) {
      data = (vt_check_str_t *)check->data;
      if (data->pattern)
        free (data->pattern);
    }
    vt_check_destroy (check, err);
  }
  return 0;
}

int
vt_check_static_str_check (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  char *mbr;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  mbr = vt_request_mbrbyid (request, data->member);

  if (strcmp (mbr, data->pattern) == 0 || data->negate)
    vt_score_update (score, check->cntr, data->weight);
  return 0;
}

int
vt_check_static_str_check_nocase (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  char *mbr;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  mbr = vt_request_mbrbyid (request, data->member);

  if (strcasecmp (mbr, data->pattern) == 0 || data->negate)
    vt_score_update (score, check->cntr, data->weight);
  return 0;
}

int
vt_check_dynamic_str_check (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  char *s1, *s2;
  char *p1, *p2;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  if (! (s1 = vt_request_mbrbyid (request, data->member))) {
    if (data->negate)
      vt_score_update (score, check->cntr, data->weight);
    return 0;
  }

  for (p1=data->pattern; *s1 && *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {

      if (! (p2 = strchr (p1, '%')) ||
          ! (s2 = vt_request_mbrbynamen (request, p1, (size_t)(p2 - p1))))
        vt_panic ("%s: bad pattern %s", __func__, data->pattern);

      p1 = p2;

      for (p1=p2; *s1 && *s1 == *s2; s1++, s2++)
        ;

      if (*s2 != '\0')
        break;
    } else if (*s1 == *p1) {
      s1++;
    } else {
      break;
    }
  }

  if (*s1 == *p1 || data->negate)
    vt_score_update (score, check->cntr, data->weight);
  return 0;
}

#define CHRCASECMP(c1,c2) ((c1) == (c2) || tolower ((c1)) == tolower ((c2)))

int
vt_check_dynamic_str_check_nocase (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  char *s1, *s2;
  char *p1, *p2;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  if (! (s1 = vt_request_mbrbyid (request, data->member))) {
    if (data->negate)
      vt_score_update (score, check->cntr, data->weight);
    return 0;
  }

  for (p1=data->pattern; *s1 && *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {

      if (! (p2 = strchr (p1, '%')) ||
          ! (s2 = vt_request_mbrbynamen (request, p1, (size_t)(p2 - p1))))
        vt_panic ("%s: bad pattern %s", __func__, data->pattern);

      p1 = p2;

      for (p1=p2; *s1 && CHRCASECMP (*s1, *s2); s1++, s2++)
        ;

      if (*s2 != '\0')
        break;
    } else if (CHRCASECMP (*s1, *p1)) {
      s1++;
    } else {
      break;
    }
  }

  if (*s1 == *p1 || data->negate)
    vt_score_update (score, check->cntr, data->weight);
  return 0;
}

#undef CHRCASECMP
