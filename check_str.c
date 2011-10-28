/* system includes */
#include <confuse.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "check.h"
#include "check_str.h"
#include "constants.h"
#include "type.h"
#include "utils.h"

/* definitions */
//typedef struct check_info_str_struct check_info_str_t;
typedef struct vt_str_struct vt_str_t;

struct vt_str_struct {
  int attrib;
  char *pattern;
  int nocase;
  int negate;
  int weight;
};

/* prototypes */
/* NOTE: "normal" checks and "nocase" checks are split up because I want to
   support internationalized domain names (IDN) eventually. */
int vt_str_destroy (vt_check_t *);
int vt_static_str_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
int vt_static_str_check_nocase (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
int vt_dynamic_str_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
int vt_dynamic_str_check_nocase (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);

/* NOTE: specifying the include/exclude options here is kind of ugly, but this
   way it does fit into my model without me having to modify a lot of code. */
cfg_opt_t vt_str_check_opts[] = {
  CFG_STR_LIST ("include", 0, CFGF_NODEFAULT),
  CFG_STR_LIST ("exclude", 0, CFGF_NODEFAULT),
  CFG_STR ("attribute", 0, CFGF_NODEFAULT),
  CFG_STR ("pattern", 0, CFGF_NODEFAULT),
  CFG_BOOL ("negate", cfg_false, CFGF_NONE),
  CFG_BOOL ("nocase", cfg_false, CFGF_NONE),
  CFG_FLOAT ("weight", 1.0, CFGF_NONE),
  CFG_END ()
};

vt_type_t *
vt_str_type (void)
{
  vt_type_t *type;

  if ((type = malloc (sizeof (vt_type_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, __LINE__, strerror (errno));
    return NULL;
  }

  memset (type, 0, sizeof (vt_type_t));

  type->name = "str";
  type->flags = VT_TYPE_FLAG_NONE;
  type->type_opts = NULL;
  type->check_opts = vt_str_check_opts;
  type->init_func = NULL;
  type->deinit_func = NULL;
  type->create_check_func = &vt_str_create;

  return type;
}

int
vt_str_create (vt_check_t **dest, const cfg_t *sec)
{
  char *value;
  vt_check_t *check;
  vt_str_t *info;

  if ((value = (char *)cfg_title ((cfg_t *)sec)) == NULL) {
    vt_error ("%s: title for check section missing", __func__);
    return VT_ERR_INVAL;
  }
  if ((check = vt_check_alloc (sizeof (vt_str_t), value)) == NULL) {
    vt_error ("%s: vt_check_alloc: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  info = (vt_str_t *) check->info;

  if ((value = cfg_getstr ((cfg_t *)sec, "attribute")) == NULL) {
    vt_error ("%s: attribute mandatory", __func__);
    vt_str_destroy (check);
    return VT_ERR_INVAL;
  }

  if ((info->attrib = vt_request_attrtoid (value)) == -1) {
    vt_error ("%s: invalid attribute: %s", __func__, value);
    vt_str_destroy (check);
    return VT_ERR_INVAL;
  }

  if ((value = cfg_getstr ((cfg_t *)sec, "pattern")) == NULL) {
    vt_error ("%s: pattern mandatory", __func__);
    vt_str_destroy (check);
    return VT_ERR_INVAL;
  }

  info->negate = cfg_getbool ((cfg_t *)sec, "negate") ? 1 : 0;
  info->nocase = cfg_getbool ((cfg_t *)sec, "nocase") ? 1 : 0;
  info->weight = vt_check_weight (cfg_getfloat ((cfg_t *)sec, "weight"));

  if (vt_check_dynamic_pattern (value)) {
    if (info->nocase)
      check->check_func = &vt_dynamic_str_check_nocase;
    else
      check->check_func = &vt_dynamic_str_check;
    info->pattern = strdup (value);
  } else {
    if (info->nocase)
      check->check_func = &vt_static_str_check_nocase;
    else
      check->check_func = &vt_static_str_check;
    info->pattern = vt_check_unescape_pattern (value);
  }

  if (info->pattern == NULL) {
    vt_error ("%s: %s", __func__, strerror (errno));
    vt_str_destroy (check);
    return VT_ERR_NOMEM;
  }

  check->prio = 1;
  check->destroy_func = &vt_str_destroy;

 *dest = check;

  return VT_SUCCESS;
}

int
vt_str_destroy (vt_check_t *check)
{
  vt_str_t *info;

  if (check) {
    if (check->info) {
      info = (vt_str_t *) check->info;

      if (info->pattern)
        free (info->pattern);
      free (info);
    }

    free (check);
  }

  return VT_SUCCESS;
}

int
vt_static_str_check (vt_check_t *check,
                     vt_request_t *request,
                     vt_score_t *score,
                     vt_stats_t *stats)
{
  char *attrib;
  vt_str_t *info;

  info = (vt_str_t *) check->info;
  attrib = vt_request_attrbyid (request, info->attrib);

  if (strcmp (attrib, info->pattern) == 0 || info->negate)
    vt_score_update (score, info->weight);

  return VT_SUCCESS;
}

int
vt_static_str_check_nocase (vt_check_t *check,
                            vt_request_t *request,
                            vt_score_t *score,
                            vt_stats_t *stats)
{
  char *attrib;
  vt_str_t *info;

  info = (vt_str_t *) check->info;
  attrib = vt_request_attrbyid (request, info->attrib);

  if (strcasecmp (attrib, info->pattern) == 0 || info->negate)
    vt_score_update (score, info->weight);

  return VT_SUCCESS;
}

int
vt_dynamic_str_check (vt_check_t *check,
                      vt_request_t *request,
                      vt_score_t *score,
                      vt_stats_t *stats)
{
  char *s1, *s2;
  char *p1, *p2;
  vt_str_t *info;

  info = (vt_str_t *) check->info;
  s1 = vt_request_attrbyid (request, info->attrib);

  for (p1=info->pattern; *s1 && *p1; p1++) {
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

  if (*s1 == *p1 || info->negate)
    vt_score_update (score, info->weight);

  return VT_SUCCESS;
}

#define CHRCASECMP(c1,c2) ((c1) == (c2) || tolower ((c1)) == tolower ((c2)))

int
vt_dynamic_str_check_nocase (vt_check_t *check,
                             vt_request_t *request,
                             vt_score_t *score,
                             vt_stats_t *stats)
{
  char *s1, *s2;
  char *p1, *p2;
  vt_str_t *info;

  info = (vt_str_t *) check->info;
  s1 = vt_request_attrbyid (request, info->attrib);

  for (p1=info->pattern; *s1 && *p1; p1++) {
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

  if (*s1 == *p1 || info->negate)
    vt_score_update (score, info->weight);

  return VT_SUCCESS;
}

#undef CHRCASECMP
