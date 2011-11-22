/* system includes */
#include <errno.h>
#include <pcre.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_priv.h"
#include "check_pcre.h"
#include "conf.h"

typedef struct _vt_check_static_pcre vt_check_static_pcre_t;

struct _vt_check_static_pcre {
  vt_request_member_t member;
  pcre *regex;
  pcre_extra *regex_extra;
  int negate;
  int weight;
};

typedef struct _vt_check_dynamic_pcre vt_check_dynamic_pcre_t;

struct _vt_check_dynamic_pcre {
  vt_request_member_t member;
  char *pattern;
  int options;
  int negate;
  int weight;
};

/* prototypes */
vt_check_t *vt_check_pcre_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
vt_check_t *vt_check_static_pcre_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
vt_check_t *vt_check_dynamic_pcre_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
int vt_check_static_pcre_destroy (vt_check_t *, vt_error_t *);
int vt_check_dynamic_pcre_destroy (vt_check_t *, vt_error_t *);
int vt_check_static_pcre_check (vt_check_t *, vt_request_t *, vt_score_t *, vt_error_t *);
size_t vt_check_dynamic_pcre_escaped_len (const char *);
ssize_t vt_check_dynamic_pcre_escape (char *, size_t, const char *);
int vt_check_dynamic_pcre_check (vt_check_t *, vt_request_t *, vt_score_t *, vt_error_t *);

const vt_check_type_t _vt_check_pcre_type = {
  .name = "pcre",
  .flags = VT_CHECK_TYPE_FLAG_NONE,
  .init_func = NULL,
  .deinit_func = NULL,
  .create_check_func = &vt_check_pcre_create
};

const vt_check_type_t *
vt_check_pcre_type (void)
{
  return &_vt_check_pcre_type;
}

vt_check_t *
vt_check_pcre_create (const vt_map_list_t *list, cfg_t *sec, vt_error_t *err)
{
  if (vt_check_dynamic_pattern (cfg_getstr (sec, "pattern")))
    return vt_check_dynamic_pcre_create (list, sec, err);
  return vt_check_static_pcre_create (list, sec, err);
}

vt_check_t *
vt_check_static_pcre_create (const vt_map_list_t *list, cfg_t *sec,
  vt_error_t *err)
{
  char *pattern, *errptr;
  int erroffset, options;
  vt_check_t *check = NULL;
  vt_check_static_pcre_t *data = NULL;

  if (! (check = vt_check_create (list, sec, err)))
    goto FAILURE;
  if (! (data = calloc (1, sizeof (vt_check_static_pcre_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  check->data = (void *)data;
  data->member = vt_request_mbrtoid (cfg_getstr (sec, "member"));
  data->negate = cfg_getbool (sec, "negate") ? 1 : 0;
  data->weight = cfg_getfloat (sec, "weight");

  if (! (pattern = vt_check_unescape_pattern (cfg_getstr (sec, "pattern")))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  if (cfg_getbool (sec, "nocase"))
    options = PCRE_UTF8 | PCRE_CASELESS;
  else
    options = PCRE_UTF8;

  data->regex = pcre_compile (pattern, options, (const char **)&errptr,
    &erroffset, NULL);
  free (pattern);
  if (! data->regex) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: pcre_compile: compilation failed at offset %d: %s",
      __func__, erroffset, errptr);
    goto FAILURE;
  }

  data->regex_extra = pcre_study (data->regex, 0, (const char **) &errptr);
  if (! data->regex_extra) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: pcre_study: %s", __func__, errptr);
    goto FAILURE;
  }

  check->prio = 2;
  check->max_weight = data->weight > 0 ? data->weight : 0;
  check->min_weight = data->weight < 0 ? data->weight : 0;
  check->check_func = &vt_check_static_pcre_check;
  check->destroy_func = &vt_check_static_pcre_destroy;

  return check;
FAILURE:
  (void)vt_check_static_pcre_destroy (check, NULL);
  return NULL;
}

vt_check_t *
vt_check_dynamic_pcre_create (const vt_map_list_t *list, cfg_t *sec,
  vt_error_t *err)
{
  vt_check_t *check = NULL;
  vt_check_dynamic_pcre_t *data = NULL;

  if (! (check = vt_check_create (list, sec, err)))
    goto FAILURE;
  if (! (data = calloc (1, sizeof (vt_check_dynamic_pcre_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  check->data = (void *)data;
  data->member = vt_request_mbrtoid (cfg_getstr (sec, "member"));
  data->negate = cfg_getbool (sec, "negate") ? 1 : 0;
  data->weight = cfg_getfloat (sec, "weight");

  if (! (data->pattern = vt_cfg_getstrdup (sec, "pattern"))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto FAILURE;
  }

  if (cfg_getbool (sec, "nocase"))
    data->options = PCRE_UTF8 | PCRE_CASELESS;
  else
    data->options = PCRE_UTF8;

  check->prio = 3;
  check->check_func = &vt_check_dynamic_pcre_check;
  check->max_weight = data->weight > 0 ? data->weight : 0;
  check->min_weight = data->weight < 0 ? data->weight : 0;
  check->destroy_func = &vt_check_dynamic_pcre_destroy;

  return check;
FAILURE:
  (void)vt_check_dynamic_pcre_destroy (check, NULL);
  return NULL;
}

int
vt_check_static_pcre_destroy (vt_check_t *check, vt_error_t *err)
{
  vt_check_static_pcre_t *data;

  if (check) {
    if (check->data) {
      data = (vt_check_static_pcre_t *)check->data;
      if (data->regex)
        pcre_free (data->regex);
      if (data->regex_extra)
        pcre_free (data->regex_extra);
    }
    vt_check_destroy (check, err);
  }
  return 0;
}

int
vt_check_dynamic_pcre_destroy (vt_check_t *check, vt_error_t *err)
{
  vt_check_dynamic_pcre_t *data;

  if (check) {
    if (check->data) {
      data = (vt_check_dynamic_pcre_t *)check->data;
      if (data->pattern)
        free (data->pattern);
    }
    vt_check_destroy (check, err);
  }
  return 0;
}

int
vt_check_static_pcre_check (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  char *member;
  vt_check_static_pcre_t *data;
  int ret;

  data = (vt_check_static_pcre_t *)check->data;
  if (! (member = vt_request_mbrbyid (request, data->member))) {
    if (data->negate)
      vt_score_update (score, check->cntr, data->weight);
    return 0;
  }

  ret = pcre_exec (data->regex, data->regex_extra, member, strlen (member), 0,
    0, NULL, 0);

  switch (ret) {
    case 0:
      if (! data->negate)
        vt_score_update (score, check->cntr, data->weight);
    case PCRE_ERROR_NOMATCH:
      if (  data->negate)
        vt_score_update (score, check->cntr, data->weight);
      break;
    case PCRE_ERROR_NOMEMORY:
      vt_set_error (err, VT_ERR_NOMEM);
      return -1;
    default:
      vt_panic ("%s: pcre_exec: unrecoverable error (%d)", __func__, ret);
  }

  return 0;
}

#define escape_character(c) ((c) == '.' || (c) == '-' || (c) == ':' || (c) == '[' || (c) == ']')

size_t
vt_check_dynamic_pcre_escaped_len (const char *s)
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
vt_check_dynamic_pcre_escape (char *s1, size_t n, const char *s2)
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
vt_check_dynamic_pcre_check (vt_check_t *check, vt_request_t *request,
  vt_score_t *score, vt_error_t *err)
{
  char *member;
  char *buf, *errptr, *attr1, *attr2, *p1, *p2, *p3;
  vt_check_dynamic_pcre_t *data;
  int erroffset, ret;
  pcre *re;
  size_t n;
  ssize_t m;

  data = (vt_check_dynamic_pcre_t *)check->data;
  if (! (attr1 = vt_request_mbrbyid (request, data->member))) {
    if (data->negate)
      vt_score_update (score, check->cntr, data->weight);
    return 0;
  }

  /* calculate buffer size needed to store pattern */
  for (n=1, p1=data->pattern; *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL)
        vt_panic ("%s (%d): invalid pattern %s", __func__, __LINE__, data->pattern);

      attr2 = vt_request_mbrbynamen (request, p1, (size_t) (p2 - p1));
      p1 = p2;

      if (attr2)
        n += vt_check_dynamic_pcre_escaped_len (attr2);
    } else {
      n++;
    }
  }

  if (! (buf = calloc (1, n))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return -1;
  }

  /* create pattern */
  for (p1=data->pattern, p3=buf; *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL) /* unlikely */
        vt_panic ("%s: bad pattern %s", __func__, data->pattern);

      if ((attr2 = vt_request_mbrbynamen (request, p1, (size_t)(p2-p1)))) {
        m = vt_check_dynamic_pcre_escape (p3, (size_t)(n - (p3-buf)), attr2);
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
  re = pcre_compile (buf, data->options, (const char **) &errptr, &erroffset, NULL);

  free (buf);

  if (re == NULL)
    vt_panic ("%s: pcre_compile: compilation failed at offset %d: %s",
      __func__, erroffset, errptr);

  ret = pcre_exec (re, NULL, attr1, strlen (attr1), 0, 0, NULL, 0);
  pcre_free (re);

  switch (ret) {
    case 0:
      if (! data->negate)
        vt_score_update (score, check->cntr, data->weight);
      break;
    case PCRE_ERROR_NOMATCH:
      if (  data->negate)
        vt_score_update (score, check->cntr, data->weight);
      break;
    case PCRE_ERROR_NOMEMORY:
      vt_set_error (err, VT_ERR_NOMEM);
      return -1;
    default:
      vt_panic ("%s: pcre_exec: unrecoverable error (%d)", __func__, ret);
  }

  return 0;
}
