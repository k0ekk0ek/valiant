/* system includes */
#include <confuse.h>
#include <errno.h>
#include <pcre.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check.h"
#include "check_pcre.h"
#include "constants.h"
#include "utils.h"

typedef struct vt_static_pcre_struct vt_static_pcre_t;

struct vt_static_pcre_struct {
  int attrib;
  pcre *regex;
  pcre_extra *regex_extra;
  int negate;
  int weight;
};

typedef struct vt_dynamic_pcre_struct vt_dynamic_pcre_t;

struct vt_dynamic_pcre_struct {
  int attrib;
  char *pattern;
  int options;
  int negate;
  int weight;
};


/* prototypes */
int vt_static_pcre_create (vt_check_t **dest, const cfg_t *);
int vt_dynamic_pcre_create (vt_check_t **dest, const cfg_t *);
int vt_static_pcre_destroy (vt_check_t *);
int vt_dynamic_pcre_destroy (vt_check_t *);
int vt_static_pcre_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
size_t vt_dynamic_pcre_escaped_len (const char *);
ssize_t vt_dynamic_pcre_escape (char *, size_t, const char *);
int vt_dynamic_pcre_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);

cfg_opt_t vt_pcre_check_opts[] = {
  CFG_STR ("attribute", 0, CFGF_NODEFAULT),
  CFG_STR ("pattern", 0, CFGF_NODEFAULT),
  CFG_BOOL ("negate", cfg_false, CFGF_NONE),
  CFG_BOOL ("nocase", cfg_false, CFGF_NONE),
  CFG_FLOAT ("weight", 1.0, CFGF_NONE),
  CFG_END ()
};

vt_type_t *
vt_pcre_type (void)
{
  vt_type_t *type;

  if ((type = malloc (sizeof (vt_type_t))) == NULL) {
    vt_error ("%s: malloc: %s", __func__, __LINE__, strerror (errno));
    return NULL;
  }

  memset (type, 0, sizeof (vt_type_t));

  type->name = "pcre";
  type->flags = VT_TYPE_FLAG_NONE;
  type->type_opts = NULL;
  type->check_opts = vt_pcre_check_opts;
  type->init_func = NULL;
  type->deinit_func = NULL;
  type->create_check_func = &vt_pcre_create;

  return type;
}

int
vt_pcre_create (vt_check_t **dest, const cfg_t *sec)
{
  char *value;

  if ((value = cfg_getstr ((cfg_t *)sec, "pattern")) == NULL) {
    vt_error ("%s: pattern mandatory", __func__);
    return VT_ERR_INVAL;
  }

  if (vt_check_dynamic_pattern (value))
    return vt_dynamic_pcre_create (dest, sec);

  return vt_static_pcre_create (dest, sec);
}

int
vt_static_pcre_create (vt_check_t **dest, const cfg_t *sec)
{
  char *value, *pattern, *errptr;
  vt_check_t *check;
  vt_static_pcre_t *info;
  int erroffset, options;

  if ((value = (char *)cfg_title ((cfg_t *)sec)) == NULL) {
    vt_error ("%s: title for check section missing", __func__);
    return VT_ERR_INVAL;
  }
  if ((check = vt_check_alloc (sizeof (vt_static_pcre_t), value)) == NULL) {
    vt_error ("%s: vt_check_alloc: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  info = (vt_static_pcre_t *) check->info;

  if ((value = cfg_getstr ((cfg_t *)sec, "attribute")) == NULL) {
    vt_error ("%s: attribute mandatory", __func__);
    vt_static_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  if ((info->attrib = vt_request_attrtoid (value)) == -1) {
    vt_error ("%s: invalid attribute: %s", __func__, value);
    vt_static_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  if ((value = cfg_getstr ((cfg_t *)sec, "pattern")) == NULL) {
    vt_error ("%s: pattern mandatory", __func__);
    vt_static_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  info->negate = cfg_getbool ((cfg_t *)sec, "negate") ? 1 : 0;
  info->weight = vt_check_weight (cfg_getfloat ((cfg_t *)sec, "weight"));

  if ((pattern = vt_check_unescape_pattern (value)) == NULL) {
    vt_error ("%s: %s", __func__, strerror (errno));
    vt_static_pcre_destroy (check);
    return VT_ERR_NOMEM;
  }

  if (cfg_getbool ((cfg_t *)sec, "nocase"))
    options = PCRE_UTF8 | PCRE_CASELESS;
  else
    options = PCRE_UTF8;

  info->regex = pcre_compile (pattern, options, (const char **) &errptr, &erroffset, NULL);
  free (pattern);

  if (info->regex == NULL) {
    vt_error ("%s: pcre_compile: compilation failed at offset %d: %s",
      __func__, erroffset, errptr);
    vt_static_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  if ((info->regex_extra = pcre_study (info->regex, 0, (const char **) &errptr)) == NULL) {
    vt_error ("%s: pcre_study: %s", __func__, errptr);
    vt_static_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  check->prio = 2;
  check->check_func = &vt_static_pcre_check;
  check->destroy_func = &vt_static_pcre_destroy;

 *dest = check;
  return VT_SUCCESS;
}

int
vt_dynamic_pcre_create (vt_check_t **dest, const cfg_t *sec)
{
  char *value;
  vt_check_t *check;
  vt_dynamic_pcre_t *info;

  if ((value = (char *)cfg_title ((cfg_t *)sec)) == NULL) {
    vt_error ("%s: title for check section missing", __func__);
    return VT_ERR_INVAL;
  }
  if ((check = vt_check_alloc (sizeof (vt_dynamic_pcre_t), value)) == NULL) {
    vt_error ("%s: vt_check_alloc: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  info = (vt_dynamic_pcre_t *) check->info;

  if ((value = cfg_getstr ((cfg_t *)sec, "attribute")) == NULL) {
    vt_error ("%s: attribute mandatory", __func__);
    vt_dynamic_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  if ((info->attrib = vt_request_attrtoid (value)) == -1) {
    vt_error ("%s: invalid attribute: %s", __func__, value);
    vt_dynamic_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  if ((value = cfg_getstr ((cfg_t *)sec, "pattern")) == NULL) {
    vt_error ("%s: pattern mandatory", __func__);
    vt_dynamic_pcre_destroy (check);
    return VT_ERR_INVAL;
  }

  info->negate = cfg_getbool ((cfg_t *)sec, "negate") ? 1 : 0;
  info->weight = vt_check_weight (cfg_getfloat ((cfg_t *)sec, "weight"));

  if ((info->pattern = strdup (value)) == NULL) {
    vt_error ("%s: %s", __func__, strerror (errno));
    vt_dynamic_pcre_destroy (check);
    return VT_ERR_NOMEM;
  }

  if (cfg_getbool ((cfg_t *)sec, "nocase"))
    info->options = PCRE_UTF8 | PCRE_CASELESS;
  else
    info->options = PCRE_UTF8;

  check->prio = 3;
  check->check_func = &vt_dynamic_pcre_check;
  check->destroy_func = &vt_dynamic_pcre_destroy;

 *dest = check;
  return VT_SUCCESS;
}

int
vt_static_pcre_destroy (vt_check_t *check)
{
  vt_static_pcre_t *info;

  if (check) {
    if (check->info) {
      info = (vt_static_pcre_t *) check->info;

      if (info->regex)
        pcre_free (info->regex);
      if (info->regex_extra)
        pcre_free (info->regex_extra);
      free (info);
    }

    free (check);
  }

  return VT_SUCCESS;
}

int
vt_dynamic_pcre_destroy (vt_check_t *check)
{
  vt_dynamic_pcre_t *info;

  if (check) {
    if (check->info) {
      info = (vt_dynamic_pcre_t *) check->info;

      if (info->pattern)
        free (info->pattern);
      free (info);
    }

    free (check);
  }

  return VT_SUCCESS;
}

int
vt_static_pcre_check (vt_check_t *check,
                      vt_request_t *request,
                      vt_score_t *score,
                      vt_stats_t *stats)
{
  char *attrib;
  vt_static_pcre_t *info;
  int ret;

  info = (vt_static_pcre_t *) check->info;
  attrib = vt_request_attrbyid (request, info->attrib);

  ret = pcre_exec (info->regex, info->regex_extra, attrib, strlen (attrib), 0,
    0, NULL, 0);

  switch (ret) {
    case 0:
      if (! info->negate)
        vt_score_update (score, info->weight);
    case PCRE_ERROR_NOMATCH:
      if (  info->negate)
        vt_score_update (score, info->weight);
      break;
    case PCRE_ERROR_NOMEMORY:
      return VT_ERR_NOMEM;
    default:
      vt_panic ("%s: pcre_exec: unrecoverable error (%d)", __func__, ret);
  }

  return VT_SUCCESS;
}

#define escape_character(c) ((c) == '.' || (c) == '-' || (c) == ':' || (c) == '[' || (c) == ']')

size_t
vt_dynamic_pcre_escaped_len (const char *s)
{
  char *p;
  size_t n;

  n = 0;

  if (s) {
    for (p=(char*)s; *p; p++)
      n += escape_character (*p) ? 2 : 1;
  }

  return n;
}

ssize_t
vt_dynamic_pcre_escape (char *s1, size_t n, const char *s2)
{
  char *p1, *p2;

  for (p1=s1, p2=(char*)s2; *p2 && (p1-s1) < n; *p1++ = *p2++) {
    if (escape_character (*p2)) {
      *p1++ = '\'';

      if ((p1-s1) >= n)
        break;
    }
  }

  *p1 = '\0'; /* always null terminate */

  return *p2 == '\0' ? -1 : (size_t)(p1-s1);
}

#undef escape_character

int
vt_dynamic_pcre_check (vt_check_t *check,
                       vt_request_t *request,
                       vt_score_t *score,
                       vt_stats_t *stats)
{
  char *buf, *errptr, *attr1, *attr2, *p1, *p2, *p3;
  vt_dynamic_pcre_t *info;
  int erroffset, ret;
  pcre *re;
  size_t n;
  ssize_t m;

  info = (vt_dynamic_pcre_t *) check->info;
  attr1 = vt_request_attrbyid (request, info->attrib);

  /* calculate buffer size needed to store pattern */
  for (n=1, p1=info->pattern; *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL)
        return VT_ERR_INVAL;

      attr2 = vt_request_attrbynamen (request, p1, (size_t) (p2 - p1));
      p1 = p2;

      if (attr2)
        n += vt_dynamic_pcre_escaped_len (attr2);
    } else {
      n++;
    }
  }

  if ((buf = malloc0 (n)) == NULL) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  /* create pattern */
  for (p1=info->pattern, p3=buf; *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL) /* unlikely */
        return VT_ERR_INVAL;

      if ((attr2 = vt_request_attrbynamen (request, p1, (size_t)(p2-p1)))) {
        m = vt_dynamic_pcre_escape (p3, (size_t)(n - (p3-buf)), attr2);
        if (m < 0)
          vt_panic ("%s: not enough space in output buffer", __func__);
        p3 += m;
      }

      p1 = p2;

    } else {
     *p3++ = *p1;
    }
  }

 *p3 = '\0'; /* always null terminate */
  re = pcre_compile (buf, info->options, (const char **) &errptr, &erroffset, NULL);

  vt_debug ("%s: pattern: %s", __func__, buf);
  free (buf);

  if (re == NULL)
    vt_panic ("%s: pcre_compile: compilation failed at offset %d: %s",
      __func__, erroffset, errptr);

  ret = pcre_exec (re, NULL, attr1, strlen (attr1), 0, 0, NULL, 0);
  pcre_free (re);

  switch (ret) {
    case 0:
      if (! info->negate)
        vt_score_update (score, info->weight);
      break;
    case PCRE_ERROR_NOMATCH:
      if (  info->negate)
        vt_score_update (score, info->weight);
      break;
    case PCRE_ERROR_NOMEMORY:
      return VT_ERR_NOMEM;
    default:
      vt_panic ("%s: pcre_exec: unrecoverable error (%d)", __func__, ret);
  }

  return VT_SUCCESS;
}
