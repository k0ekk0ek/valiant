/* system includes */
#include <confuse.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "check_str.h"
#include "type.h"
#include "utils.h"
#include "valiant.h"

/* definitions */
typedef struct vt_check_str_struct vt_check_str_t;

struct vt_check_str_struct {
  vt_request_member_t member;
  char *pattern;
  int nocase;
  int negate;
  int weight;
};

/* prototypes */
/* NOTE: "normal" checks and "nocase" checks are split up because I want to
   support internationalized domain names (IDN) eventually. */
void vt_check_str_destroy (vt_check_t *);
int vt_check_static_str_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
int vt_check_static_str_check_nocase (vt_check_t *, vt_request_t *,
  vt_score_t *, vt_stats_t *);
int vt_check_dynamic_str_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
int vt_check_dynamic_str_check_nocase (vt_check_t *, vt_request_t *,
  vt_score_t *, vt_stats_t *);
int vt_check_str_weight (vt_check_t *, int);

const vt_check_type_t vt_check_type_str = {
  .name = "str";
  .flags = VT_CHECK_TYPE_FLAG_NONE;
  .init_func = NULL;
  .deinit_func = NULL;
  .create_check_func = &vt_check_str_create;
};

vt_check_type_t *
vt_check_str_type (void)
{
  return &vt_check_type_str;
}

int
vt_check_str_create (vt_check_t **dest, const vt_map_list_t *list,
  const cfg_t *sec)
{
  char *mbr, *ptrn;
  int ret, err;
  vt_check_t *check = NULL;
  vt_check_str_t *data = NULL;

  ret = vt_check_create (&check, sizeof (vt_check_str_t), list, sec);
  if (ret != VT_SUCCESS) {
    err = ret;
    goto FAILURE;
  }

  data = (vt_check_str_t *)check->data;

  if ((mbr = cfg_getstr ((cfg_t *)sec, "member")) == NULL) {
    vt_error ("%s: member unavailable for check %s", __func__, check->name);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }
  if (vt_request_mbrtoid (&data->member, mbr) != VT_SUCCESS) {
    vt_error ("%s: invalid member %s in check %s", __func__, mbr, check->name);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }
  if ((ptrn = cfg_getstr ((cfg_t *)sec, "pattern")) == NULL) {
    vt_error ("%s: pattern unavailable for check %s", __func__, check->name);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }

  data->negate = cfg_getbool ((cfg_t *)sec, "negate") ? 1 : 0;
  data->nocase = cfg_getbool ((cfg_t *)sec, "nocase") ? 1 : 0;
  data->weight = vt_check_weight (cfg_getfloat ((cfg_t *)sec, "weight"));

  if (vt_check_dynamic_pattern (pattern)) {
    if (data->nocase)
      check->check_func = &vt_check_dynamic_str_check_nocase;
    else
      check->check_func = &vt_check_dynamic_str_check;
    data->pattern = strdup (ptrn);
  } else {
    if (data->nocase)
      check->check_func = &vt_check_static_str_check_nocase;
    else
      check->check_func = &vt_check_static_str_check;
    data->pattern = vt_check_unescape_pattern (ptrn);
  }

  if (data->pattern == NULL) {
    vt_error ("%s: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }

  check->prio = 1;
  check->weight_func = &vt_check_str_weight;
  check->destroy_func = &vt_check_str_destroy;

  *dest = check;
  return VT_SUCCESS;
FAILURE:
  vt_check_str_destroy (check);
  return err;
}

void
vt_check_str_destroy (vt_check_t *check)
{
  vt_check_str_t *data;

  if (check) {
    if (check->data) {
      data = (vt_check_str_t *)check->data;
      if (data->pattern)
        free (data->pattern);
    }
    vt_check_destroy (check);
  }
}

int
vt_check_static_str_check (vt_check_t *check,
                           vt_request_t *request,
                           vt_score_t *score,
                           vt_stats_t *stats)
{
  char *attrib;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  attrib = vt_request_attrbyid (request, data->attrib);

  if (strcmp (attrib, data->pattern) == 0 || data->negate)
    vt_score_update (score, data->weight);

  return VT_SUCCESS;
}

int
vt_check_static_str_check_nocase (vt_check_t *check,
                                  vt_request_t *request,
                                  vt_score_t *score,
                                  vt_stats_t *stats)
{
  char *attrib;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  attrib = vt_request_attrbyid (request, data->attrib);

  if (strcasecmp (attrib, data->pattern) == 0 || data->negate)
    vt_score_update (score, data->weight);

  return VT_SUCCESS;
}

int
vt_check_dynamic_str_check (vt_check_t *check,
                            vt_request_t *request,
                            vt_score_t *score,
                            vt_stats_t *stats)
{
  char *s1, *s2;
  char *p1, *p2;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  s1 = vt_request_attrbyid (request, data->attrib);

  for (p1=data->pattern; *s1 && *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {

      // FIXME: provide more information on errors
      if ((p2 = strchr (p1, '%')) == NULL)
        return VT_ERR_INVAL;
      if ((s2 = vt_request_attrbynamen (request, p1, (size_t) (p2 - p1))) == NULL)
        return VT_ERR_INVAL;
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
    vt_score_update (score, data->weight);

  return VT_SUCCESS;
}

#define CHRCASECMP(c1,c2) ((c1) == (c2) || tolower ((c1)) == tolower ((c2)))

int
vt_check_dynamic_str_check_nocase (vt_check_t *check,
                                   vt_request_t *request,
                                   vt_score_t *score,
                                   vt_stats_t *stats)
{
  char *s1, *s2;
  char *p1, *p2;
  vt_check_str_t *data;

  data = (vt_check_str_t *)check->data;
  s1 = vt_request_attrbyid (request, data->attrib);

  for (p1=data->pattern; *s1 && *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {

      // FIXME: provide more information on errors
      if ((p2 = strchr (p1, '%')) == NULL)
        return VT_ERR_INVAL;
      if ((s2 = vt_request_attrbynamen (request, p1, (size_t) (p2 - p1))) == NULL)
        return VT_ERR_INVAL;
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
    vt_score_update (score, data->weight);

  return VT_SUCCESS;
}

#undef CHRCASECMP

int
vt_check_str_min_weight (vt_check_t *, int maximum)
{
  int weight = 0;
  vt_check_str_t *data = (vt_check_str_t *)check->data;

  if (maximum) {
    if (data->weight > weight)
      weight = data->weight;
  } else {
    if (data->weight < weight)
      weight = data->weight;
  }

  return weight;
}
