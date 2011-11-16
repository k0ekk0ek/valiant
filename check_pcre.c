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
#include "map.h"
#include "utils.h"
#include "consts.h"

typedef struct vt_check_static_pcre_struct vt_check_static_pcre_t;

struct vt_check_static_pcre_struct {
  vt_request_mbr_t member;
  pcre *regex;
  pcre_extra *regex_extra;
  int negate;
  int weight;
};

typedef struct vt_check_dynamic_pcre_struct vt_check_dynamic_pcre_t;

struct vt_check_dynamic_pcre_struct {
  vt_request_mbr_t member;
  char *pattern;
  int options;
  int negate;
  int weight;
};


/* prototypes */
int vt_check_pcre_create (vt_check_t **dest, const vt_map_list_t *, const cfg_t *);
int vt_check_static_pcre_create (vt_check_t **dest, const vt_map_list_t *, const cfg_t *);
int vt_check_dynamic_pcre_create (vt_check_t **dest, const vt_map_list_t *, const cfg_t *);
void vt_check_static_pcre_destroy (vt_check_t *);
void vt_check_dynamic_pcre_destroy (vt_check_t *);
int vt_check_static_pcre_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
size_t vt_check_dynamic_pcre_escaped_len (const char *);
ssize_t vt_check_dynamic_pcre_escape (char *, size_t, const char *);
int vt_check_dynamic_pcre_check (vt_check_t *, vt_request_t *, vt_score_t *,
  vt_stats_t *);
int vt_check_static_pcre_weight (vt_check_t *, int);
int vt_check_dynamic_pcre_weight (vt_check_t *, int);

const vt_check_type_t vt_check_type_pcre = {
  .name = "pcre",
  .flags = VT_CHECK_TYPE_FLAG_NONE,
  .init_func = NULL,
  .deinit_func = NULL,
  .create_check_func = &vt_check_pcre_create
};

const vt_check_type_t *
vt_check_pcre_type (void)
{
  return &vt_check_type_pcre;
}

int
vt_check_pcre_create (vt_check_t **dest, const vt_map_list_t *list,
  const cfg_t *sec)
{
  char *ptrn, *title;

  if ((title = (char *)cfg_title ((cfg_t *)sec)) == NULL) {
    vt_error ("%s: check title unavailable", __func__);
    return VT_ERR_BADCFG;
  }
  if ((ptrn = cfg_getstr ((cfg_t *)sec, "pattern")) == NULL) {
    vt_error ("%s: pattern unavailable for check %s", __func__, title);
    return VT_ERR_BADCFG;
  }

  if (vt_check_dynamic_pattern (ptrn))
    return vt_check_dynamic_pcre_create (dest, list, sec);

  return vt_check_static_pcre_create (dest, list, sec);
}

int
vt_check_static_pcre_create (vt_check_t **dest, const vt_map_list_t *list,
  const cfg_t *sec)
{
  char *ptrn1, *ptrn2, *errptr;
  int erroffset, options, ret, err;
  vt_check_t *check = NULL;
  vt_check_static_pcre_t *data;

  ret = vt_check_create (&check, sizeof (vt_check_static_pcre_t), list, sec);
  if (ret != VT_SUCCESS) {
    err = ret;
    goto FAILURE;
  }

  data = (vt_check_static_pcre_t *)check->data;
  vt_request_mbrtoid (&data->member, cfg_getstr ((cfg_t *)sec, "member"));
  data->negate = cfg_getbool ((cfg_t *)sec, "negate") ? 1 : 0;
  data->weight = vt_check_weight (cfg_getfloat ((cfg_t *)sec, "weight"));

  ptrn1 = cfg_getstr ((cfg_t *)sec, "pattern");
  if ((ptrn2 = vt_check_unescape_pattern (ptrn1)) == NULL) {
    vt_error ("%s: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }

  if (cfg_getbool ((cfg_t *)sec, "nocase"))
    options = PCRE_UTF8 | PCRE_CASELESS;
  else
    options = PCRE_UTF8;

  data->regex = pcre_compile (ptrn2, options, (const char **)&errptr,
    &erroffset, NULL);
  free (ptrn2);
  if (! data->regex) {
    vt_error ("%s: pcre_compile: compilation failed at offset %d: %s",
      __func__, erroffset, errptr);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }

  data->regex_extra = pcre_study (data->regex, 0, (const char **) &errptr);
  if (! data->regex_extra) {
    vt_error ("%s: pcre_study: %s", __func__, errptr);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }

  check->prio = 2;
  check->check_func = &vt_check_static_pcre_check;
  check->weight_func = &vt_check_static_pcre_weight;
  check->destroy_func = &vt_check_static_pcre_destroy;

  *dest = check;
  return VT_SUCCESS;
FAILURE:
  vt_check_static_pcre_destroy (check);
  return err;
}

int
vt_check_dynamic_pcre_create (vt_check_t **dest, const vt_map_list_t *list,
  const cfg_t *sec)
{
  char *ptrn;
  int ret, err;
  vt_check_t *check = NULL;
  vt_check_dynamic_pcre_t *data;

  ret = vt_check_create (&check, sizeof (vt_check_static_pcre_t), list, sec);
  if (ret != VT_SUCCESS) {
    err = ret;
    goto FAILURE;
  }

  data = (vt_check_dynamic_pcre_t *)check->data;
  vt_request_mbrtoid (&data->member, cfg_getstr ((cfg_t *)sec, "member"));
  data->negate = cfg_getbool ((cfg_t *)sec, "negate") ? 1 : 0;
  data->weight = vt_check_weight (cfg_getfloat ((cfg_t *)sec, "weight"));

  ptrn = cfg_getstr ((cfg_t *)sec, "pattern");
  if ((data->pattern = strdup (ptrn)) == NULL) {
    vt_error ("%s: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }

  if (cfg_getbool ((cfg_t *)sec, "nocase"))
    data->options = PCRE_UTF8 | PCRE_CASELESS;
  else
    data->options = PCRE_UTF8;

  check->prio = 3;
  check->check_func = &vt_check_dynamic_pcre_check;
  check->weight_func = &vt_check_dynamic_pcre_weight;
  check->destroy_func = &vt_check_dynamic_pcre_destroy;

  *dest = check;
  return VT_SUCCESS;
FAILURE:
  vt_check_dynamic_pcre_destroy (check);
  return err;
}

void
vt_check_static_pcre_destroy (vt_check_t *check)
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
    vt_check_destroy (check);
  }
}

void
vt_check_dynamic_pcre_destroy (vt_check_t *check)
{
  vt_check_dynamic_pcre_t *data;

  if (check) {
    if (check->data) {
      data = (vt_check_dynamic_pcre_t *)check->data;
      if (data->pattern)
        free (data->pattern);
    }
    vt_check_destroy (check);
  }
}

int
vt_check_static_pcre_check (vt_check_t *check,
                            vt_request_t *request,
                            vt_score_t *score,
                            vt_stats_t *stats)
{
  char *attrib;
  vt_check_static_pcre_t *data;
  int ret;

  data = (vt_check_static_pcre_t *)check->data;
  vt_request_mbrbyid (&attrib, request, data->member);

  ret = pcre_exec (data->regex, data->regex_extra, attrib, strlen (attrib), 0,
    0, NULL, 0);

  switch (ret) {
    case 0:
      if (! data->negate)
        vt_score_update (score, check->id, data->weight);
    case PCRE_ERROR_NOMATCH:
      if (  data->negate)
        vt_score_update (score, check->id, data->weight);
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
vt_check_dynamic_pcre_check (vt_check_t *check,
                             vt_request_t *request,
                             vt_score_t *score,
                             vt_stats_t *stats)
{
  char *buf, *errptr, *attr1, *attr2, *p1, *p2, *p3;
  vt_check_dynamic_pcre_t *data;
  int erroffset, ret;
  pcre *re;
  size_t n;
  ssize_t m;

  data = (vt_check_dynamic_pcre_t *)check->data;
  vt_request_mbrbyid (&attr1, request, data->member);

  /* calculate buffer size needed to store pattern */
  for (n=1, p1=data->pattern; *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL)
        return VT_ERR_INVAL;

      vt_request_mbrbynamen (&attr2, request, p1, (size_t) (p2 - p1));
      p1 = p2;

      if (attr2)
        n += vt_check_dynamic_pcre_escaped_len (attr2);
    } else {
      n++;
    }
  }

  if ((buf = malloc0 (n)) == NULL) {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  /* create pattern */
  for (p1=data->pattern, p3=buf; *p1; p1++) {
    if (*p1 == '%' && *(++p1) != '%') {
      if ((p2 = strchr (p1, '%')) == NULL) /* unlikely */
        return VT_ERR_INVAL;

      if (vt_request_mbrbynamen (&attr2, request, p1, (size_t)(p2-p1)) == VT_SUCCESS) {
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
        vt_score_update (score, check->id, data->weight);
      break;
    case PCRE_ERROR_NOMATCH:
      if (  data->negate)
        vt_score_update (score, check->id, data->weight);
      break;
    case PCRE_ERROR_NOMEMORY:
      return VT_ERR_NOMEM;
    default:
      vt_panic ("%s: pcre_exec: unrecoverable error (%d)", __func__, ret);
  }

  return VT_SUCCESS;
}

int
vt_check_static_pcre_weight (vt_check_t *check, int maximum)
{
  int weight = 0;
  vt_check_static_pcre_t *data = (vt_check_static_pcre_t *)check->data;

  if (maximum) {
    if (data->weight > weight)
      weight = data->weight;
  } else {
    if (data->weight < weight)
      weight = data->weight;
  }

  return weight;
}

int
vt_check_dynamic_pcre_weight (vt_check_t *check, int maximum)
{
  int weight = 0;
  vt_check_dynamic_pcre_t *data = (vt_check_dynamic_pcre_t *)check->data;

  if (maximum) {
    if (data->weight > weight)
      weight = data->weight;
  } else {
    if (data->weight < weight)
      weight = data->weight;
  }

  return weight;
}
