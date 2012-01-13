/* system includes */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pcre.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* valiant includes */
#include "dict_priv.h"
#include "dict_pcre.h"
#include "request.h"
#include "state.h"

#define VT_DICT_PCRE_TIME_DIFF (300)

typedef struct _vt_pcre vt_pcre_t;

struct _vt_pcre {
  pcre *re;
  pcre_extra *sd;
  int invert;
  float weight;
};

typedef struct _vt_dict_multi_pcre vt_dict_multi_pcre_t;

struct _vt_dict_multi_pcre {
  vt_request_member_t member;
  char *path;
  int invert;
  float weight;
  vt_state_t state;
  int ready;
  time_t atime;
  time_t mtime;
  pthread_rwlock_t lock;
  vt_slist_t *regexes; /* list of vt_dict_pcre_t */
};

typedef struct _vt_dict_stat_pcre vt_dict_stat_pcre_t;

struct _vt_dict_stat_pcre {
  vt_request_member_t member;
  vt_pcre_t *regex;
};

typedef struct _vt_dict_dyn_pcre vt_dict_dyn_pcre_t;

struct _vt_dict_dyn_pcre {
  vt_request_member_t member;
  char *pattern;
  int options;
  int invert;
  float weight;
};

/* prototypes */
vt_pcre_t *vt_pcre_create (const char *, int, int, float, vt_error_t *);
vt_dict_t *vt_dict_pcre_create (vt_dict_type_t *, cfg_t *, cfg_t *,
  vt_error_t *);
vt_dict_t *vt_dict_stat_pcre_create (vt_dict_type_t *, cfg_t *, cfg_t *,
  vt_error_t *);
vt_dict_t *vt_dict_dyn_pcre_create (vt_dict_type_t *, cfg_t *, cfg_t *,
  vt_error_t *);
vt_dict_t *vt_dict_multi_pcre_create (vt_dict_type_t *, cfg_t *, cfg_t *,
  vt_error_t *);
int vt_pcre_destroy (vt_pcre_t *, vt_error_t *);
int vt_dict_stat_pcre_destroy (vt_dict_t *, vt_error_t *);
int vt_dict_dyn_pcre_destroy (vt_dict_t *, vt_error_t *);
int vt_slist_pcre_free (void *);
int vt_dict_multi_pcre_destroy (vt_dict_t *, vt_error_t *);
int vt_pcre_check (float *, const char *, vt_pcre_t *, vt_error_t *);
int vt_dict_stat_pcre_check (vt_dict_t *, vt_request_t *, vt_result_t *, int,
  vt_error_t *);
size_t vt_dict_dyn_pcre_escaped_len (const char *);
ssize_t vt_dict_dyn_pcre_escape (char *, size_t, const char *);
int vt_dict_dyn_pcre_check (vt_dict_t *, vt_request_t *, vt_result_t *, int,
  vt_error_t *);
int vt_dict_multi_pcre_open (vt_dict_multi_pcre_t *data, vt_error_t *);
vt_slist_t *vt_dict_multi_pcre_parse (const char *, vt_error_t *);
int vt_dict_multi_pcre_check (vt_dict_t *, vt_request_t *, vt_result_t *, int,
  vt_error_t *);

vt_dict_type_t _vt_dict_pcre_type = {
  .name = "pcre",
  .create_func = &vt_dict_pcre_create
};

vt_dict_type_t *
vt_dict_pcre_type (void)
{
  return &_vt_dict_pcre_type;
}

vt_pcre_t *
vt_pcre_create (const char *pattern,
                int options,
                int invert,
                float weight,
                vt_error_t *err)
{
  const char *errptr;
  int erroffset;
  vt_pcre_t *data;

  data = calloc (1, sizeof (vt_pcre_t));
  if (! data) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  data->invert = invert;
  data->weight = weight;
  data->re = pcre_compile (pattern, (options | PCRE_UTF8), &errptr, &erroffset, NULL);
  if (! data->re) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: pcre_compile: compilation failed at offset %d: %s",
      __func__, erroffset, errptr);
    goto failure;
  }

  data->sd = pcre_study (data->re, 0, &errptr);
  if (! data->sd) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: pcre_study: %s", __func__, errptr);
    goto failure;
  }

  return data;
failure:
  (void)vt_pcre_destroy (data, NULL);
  return NULL;
}

vt_dict_t *
vt_dict_pcre_create (vt_dict_type_t *type,
                     cfg_t *type_sec,
                     cfg_t *dict_sec,
                     vt_error_t *err)
{
  cfg_opt_t *opt;

  assert (type);
  assert (dict_sec);

  opt = cfg_getopt (dict_sec, "pattern");
  if (opt && opt->nvalues && ! (opt->flags & CFGF_RESET)) {
    if (vt_dict_dynamic_pattern (cfg_opt_getnstr (opt, 0)))
      return vt_dict_dyn_pcre_create (type, type_sec, dict_sec, err);
    else
      return vt_dict_stat_pcre_create (type, type_sec, dict_sec, err);
  }

  /* it must be a postfix like pcre table if no pattern was provided */
  return vt_dict_multi_pcre_create (type, type_sec, dict_sec, err);
}


vt_dict_t *
vt_dict_stat_pcre_create (vt_dict_type_t *type,
                          cfg_t *type_sec,
                          cfg_t *dict_sec,
                          vt_error_t *err)
{
  char *pattern;
  int invert, options;
  float weight;
  vt_dict_t *dict;
  vt_dict_stat_pcre_t *data;

  assert (type);
  assert (dict_sec);

  if (! (dict = vt_dict_create_common (dict_sec, err)))
    goto failure;
  if (! (data = calloc (1, sizeof (vt_dict_stat_pcre_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  dict->data = (void *)data;

  /* compile regular expression */
  pattern = vt_dict_unescape_pattern (cfg_getstr (dict_sec, "pattern"));
  options = cfg_getbool (dict_sec, "nocase") ? PCRE_CASELESS : 0;
  invert = cfg_getbool (dict_sec, "invert") ? 1 : 0;
  weight = cfg_getfloat (dict_sec, "weight");

  data->member = vt_request_mbrtoid (cfg_getstr (dict_sec, "member"));
  data->regex = vt_pcre_create (pattern, options, invert, weight, err);
  if (! data->regex)
    goto failure;

  dict->max_diff = (weight > 0.0) ? weight : 0.0;
  dict->min_diff = (weight < 0.0) ? weight : 0.0;
  dict->check_func = &vt_dict_stat_pcre_check;
  dict->destroy_func = &vt_dict_stat_pcre_destroy;

  return dict;
failure:
  (void)vt_dict_stat_pcre_destroy (dict, NULL);
  return NULL;
}


vt_dict_t *
vt_dict_dyn_pcre_create (vt_dict_type_t *type,
                         cfg_t *type_sec,
                         cfg_t *dict_sec,
                         vt_error_t *err)
{
  vt_dict_t *dict;
  vt_dict_dyn_pcre_t *data;

  assert (type);
  assert (dict_sec);

  if (! (dict = vt_dict_create_common (dict_sec, err)))
    goto failure;
  if (! (data = calloc (1, sizeof (vt_dict_dyn_pcre_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  dict->data = (void *)data;
  data->member = vt_request_mbrtoid (cfg_getstr (dict_sec, "member"));
  data->pattern = strdup (cfg_getstr (dict_sec, "pattern"));
  data->options = cfg_getbool (dict_sec, "nocase") ? PCRE_CASELESS : 0;
  data->invert = cfg_getbool (dict_sec, "invert") ? 1 : 0;
  data->weight = cfg_getfloat (dict_sec, "weight");

  if (! data->pattern) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    goto failure;
  }

  dict->max_diff = (data->weight > 0.0) ? data->weight : 0.0;
  dict->min_diff = (data->weight < 0.0) ? data->weight : 0.0;
  dict->check_func = &vt_dict_dyn_pcre_check;
  dict->destroy_func = &vt_dict_dyn_pcre_destroy;

  return dict;
failure:
  (void)vt_dict_dyn_pcre_destroy (dict, NULL);
  return NULL;
}


vt_dict_t *
vt_dict_multi_pcre_create (vt_dict_type_t *type,
                           cfg_t *type_sec,
                           cfg_t *dict_sec,
                           vt_error_t *err)
{
  cfg_opt_t *opt;
  char *fmt;
  int ret;
  vt_dict_t *dict;
  vt_dict_multi_pcre_t *data;

  assert (type);
  assert (dict_sec);

  if (! (dict = vt_dict_create_common (dict_sec, err)))
    goto failure;
  if (! (data = calloc (1, sizeof (vt_dict_multi_pcre_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  dict->data = (void *)data;

  /* initialize complex members */
  (void)vt_state_init (&data->state, 0, NULL);

  if ((ret = pthread_rwlock_init (&data->lock, NULL)) != 0) {
    fmt = "%s: pthread_rwlock_init: %s";
    if (ret != ENOMEM)
      vt_panic (fmt, __func__, strerror (ret));
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error (fmt, __func__, strerror (ret));
    goto failure;
  }

  data->member = vt_request_mbrtoid (cfg_getstr (dict_sec, "member"));
  data->path = strdup (cfg_getstr (dict_sec, "path"));

  if (! data->path) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  if (cfg_getbool (dict_sec, "invert")) {
    data->invert = 1;
    data->weight = cfg_getfloat (dict_sec, "weight");
  } else {
    data->invert = 0;
    opt = cfg_getopt (dict_sec, "weight");
    if (opt && opt->nvalues && ! (opt->flags &CFGF_RESET))
      data->weight = cfg_opt_getnfloat (opt, 0);
  }

  dict->async = 1;
  // FIXME: implement "infinity"
  //dict->max_diff =
  //dict->min_diff =
  dict->check_func = &vt_dict_multi_pcre_check;
  dict->destroy_func = &vt_dict_multi_pcre_destroy;

  return dict;
failure:
  (void)vt_dict_multi_pcre_destroy (dict, NULL);
  return NULL;
}

int
vt_pcre_destroy (vt_pcre_t *expr, vt_error_t *err)
{
  if (expr) {
    if (expr->re)
      pcre_free (expr->re);
    if (expr->sd)
      pcre_free (expr->re);
    free (expr);
  }
  return 0;
}

int
vt_dict_stat_pcre_destroy (vt_dict_t *dict, vt_error_t *err)
{
  vt_dict_stat_pcre_t *data;

  if (dict) {
    if (dict->data) {
      data = (vt_dict_stat_pcre_t *)dict->data;
      if (data->regex)
        (void)vt_pcre_destroy (data->regex, NULL);
      free (data);
    }
    return vt_dict_destroy_common (dict, err);
  }
  return 0;
}

int
vt_dict_dyn_pcre_destroy (vt_dict_t *dict, vt_error_t *err)
{
  vt_dict_dyn_pcre_t *data;

  if (dict) {
    if (dict->data) {
      data = (vt_dict_dyn_pcre_t *)dict->data;
      if (data->pattern)
        free (data->pattern);
      free (data);
    }
    return vt_dict_destroy_common (dict, err);
  }
  return 0;
}

int
vt_slist_pcre_free (void *ptr)
{
  return vt_pcre_destroy ((vt_pcre_t *)ptr, NULL);
}

int
vt_dict_multi_pcre_destroy (vt_dict_t *dict, vt_error_t *err)
{
  int ret;
  vt_dict_multi_pcre_t *data;

  if (dict) {
    if (dict->data) {
      data = (vt_dict_multi_pcre_t *)dict->data;
      if (data->path)
        free (data->path);
      if ((ret = pthread_rwlock_destroy (&data->lock)) != 0)
        vt_panic ("%s: pthread_rwlock_destroy: %s", __func__, strerror (ret));
      if (data->regexes)
        vt_slist_free (data->regexes, &vt_slist_pcre_free, 0);
    }
    return vt_dict_destroy_common (dict, err);
  }
  return 0;
}

int
vt_pcre_check (float *weight, const char *str, vt_pcre_t *regex, vt_error_t *err)
{
  int match;
  int ret;

  assert (weight);
  assert (str);
  assert (regex);

  ret = pcre_exec (regex->re, regex->sd, str, strlen (str), 0, 0, NULL, 0);
  match = 0;
  *weight = 0;
  switch (ret) {
    case 0:
      if (regex->invert) {
        match = 0;
        *weight = 0.0;
      } else {
        match = 1;
        *weight = regex->weight;
      }
      return 0;
    case PCRE_ERROR_NOMATCH:
      if (regex->invert) {
        match = 1;
        *weight = regex->weight;
      } else {
        match = 0;
        *weight = 0.0;
      }
      break;
    case PCRE_ERROR_NOMEMORY:
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: pcre_exec: %s", __func__, strerror (ENOMEM));
    default:
      vt_panic ("%s: pcre_exec: unrecoverable error (%d)", __func__, ret);
  }

  return match ? 0 : -1;
}

int
vt_dict_stat_pcre_check (vt_dict_t *dict,
                         vt_request_t *req,
                         vt_result_t *res,
                         int pos,
                         vt_error_t *err)
{
  char *member;
  int ret;
  float weight;
  vt_dict_stat_pcre_t *data;
  vt_error_t tmperr;

  assert (dict);
  assert (req);
  assert (res);
  data = (vt_dict_stat_pcre_t *)dict->data;
  assert (data);

  if (! (member = vt_request_mbrbyid (req, data->member))) {
    if (data->regex->invert)
      weight = data->regex->weight;
    else
      weight = 0.0;
    goto update;
  }

  tmperr = 0;
  if (vt_pcre_check (&weight, member, data->regex, &tmperr) < 0 && tmperr != 0) {
    vt_set_error (err, tmperr);
    return -1;
  }

update:
  vt_result_update (res, pos, weight);
vt_error ("%s:%d: weight: %f", __func__, __LINE__, weight);
  return 0;
}

#define escape_character(c) ((c) == '.' || (c) == '-' || (c) == ':' || \
                             (c) == '[' || (c) == ']')

size_t
vt_dict_dyn_pcre_escaped_len (const char *str)
{
  char *ptr;
  size_t len;

  len = 0;

  if (str) {
    for (ptr = (char*)str; *ptr; ptr++)
      len += escape_character (*ptr) ? 2 : 1;
  }

  return len;
}

ssize_t
vt_dict_dyn_pcre_escape (char *str1, size_t len, const char *str2)
{
  char *ptr1, *ptr2;

  for (ptr1 = str1, ptr2 = (char*)str2; *ptr2 && (ptr1 - str1) < len; *ptr1++ = *ptr2++) {
    if (escape_character (*ptr2)) {
      *ptr1++ = '\\';

      if ((ptr1 - str1) >= len)
        break;
    }
  }

  *ptr1 = '\0'; /* always null terminate */

  return (*ptr2 == '\0') ? (size_t)(ptr1 - str1) : -1;
}

#undef escape_character

int
vt_dict_dyn_pcre_check (vt_dict_t *dict, 
                        vt_request_t *req,
                        vt_result_t *res,
                        int pos,
                        vt_error_t *err)
{
  char *member;
  char *buf, *str1, *str2, *ptr1, *ptr2, *ptr3;
  const char *errptr;
  float weight;
  int erroffset;
  int ret;
  pcre *re;
  size_t len1;
  ssize_t len2;
  vt_dict_dyn_pcre_t *data;

  assert (dict);
  assert (req);
  assert (res);
  data = (vt_dict_dyn_pcre_t *)dict->data;
  assert (data);

  if (! (member = vt_request_mbrbyid (req, data->member))) {
    if (data->invert)
      weight = data->weight;
    else
      weight = 0.0;
    goto update;
  }

  /* calculate buffer length needed to store pattern */
  for (len1 = 1, ptr1 = data->pattern; *ptr1; ptr1++) {
    if (*ptr1 == '%' && *(++ptr1) != '%') {
      if ((ptr2 = strchr (ptr1, '%')) == NULL)
        vt_panic ("%s: bad pattern %s", __func__, data->pattern);

      str2 = vt_request_mbrbynamen (req, ptr1, (size_t) (ptr2 - ptr1));
      ptr1 = ptr2;

      if (str2)
        len1 += vt_dict_dyn_pcre_escaped_len (str2);
    } else {
      len1++;
    }
  }

  if (! (buf = calloc (1, len1))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return -1;
  }

  for (ptr1 = data->pattern, ptr3 = buf; *ptr1; ptr1++) {
    if (*ptr1 == '%' && *(++ptr1) != '%') {
      if ((ptr2 = strchr (ptr1, '%')) == NULL) /* unlikely */
        vt_panic ("%s: bad pattern %s", __func__, data->pattern);

      if ((str2 = vt_request_mbrbynamen (req, ptr1, (size_t)(ptr2 - ptr1)))) {
        len2 = vt_dict_dyn_pcre_escape (ptr3, (size_t)(len1 - (ptr3 - buf)), str2);
        if (len2 < 0)
          vt_panic ("%s: not enough space in output buffer", __func__);
        ptr3 += len2;
      }

      ptr1 = ptr2;
    } else {
     *ptr3++ = *ptr1;
    }
  }

  *ptr3 = '\0'; /* always null terminate */
  vt_debug ("%s: pattern: %s", __func__, buf);

  /* compile and execute newly created pattern */
  re = pcre_compile (buf, (data->options | PCRE_UTF8), &errptr, &erroffset, NULL);
  free (buf);
  if (! re) {
    vt_panic ("%s: pcre_compile: compilation failed at offset %d: %s",
      __func__, erroffset, errptr);
  }

  ret = pcre_exec (re, NULL, member, strlen (member), 0, 0, NULL, 0);
  pcre_free (re);
  switch (ret) {
    case 0:
      if (data->invert)
        weight = 0.0;
      else
        weight = data->weight;
      break;
    case PCRE_ERROR_NOMATCH:
      if (data->invert)
        weight = data->weight;
      else
        weight = 0.0;
      break;
    case PCRE_ERROR_NOMEMORY:
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: pcre_exec: %s", __func__, strerror (ENOMEM));
      return -1;
    default:
      vt_panic ("%s: pcre_exec: unrecoverable error (%d)", __func__, ret);
  }

update:
  vt_result_update (res, pos, weight);
vt_error ("%s:%d: weight: %f", __func__, __LINE__, weight);
  return 0;
}

int
vt_dict_multi_pcre_open (vt_dict_multi_pcre_t *data, vt_error_t *err)
{
  char *fmt;
  int loaddb, wrlock;
  int ret;
  struct stat st_buf;
  time_t now;
  vt_error_t tmperr;

  assert (data);

  now = time (NULL);
  wrlock = 0;

again:
  if ((! data->regexes || ! data->ready) && ! vt_state_omit (&data->state))
    loaddb = 1;
  else
    loaddb = 0;

  if (loaddb || now < (data->atime + VT_DICT_PCRE_TIME_DIFF)) {
    fmt = "%s: stat %s: %s";
    if (stat (data->path, &st_buf) < 0) {
      switch (errno) {
        case EACCES:
        case ELOOP:
        case ENOENT:
        case ENOTDIR:
          break;
        default:
          vt_panic (fmt, __func__, data->path, strerror (errno));
      }

      vt_set_error (err, VT_ERR_CONNFAILED);
      vt_error (fmt, __func__, data->path, strerror (errno));
      return -1;
    }

    if (! loaddb && data->mtime != st_buf.st_mtime)
      loaddb = 1;
  }

  if (loaddb) {
    if (! wrlock) {
      if ((ret = pthread_rwlock_unlock (&data->lock)) != 0)
        vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
      if ((ret = pthread_rwlock_wrlock (&data->lock)) != 0)
        vt_panic ("%s: pthread_rwlock_wrlock: %s", __func__, strerror (ret));
      wrlock = 1;
      goto again;
    }

    if (data->regexes)
      vt_slist_free (data->regexes, &vt_slist_pcre_free, 0);

    tmperr = 0;
    if ((data->regexes = vt_dict_multi_pcre_parse (data->path, &tmperr)) || tmperr == 0) {
      /* FIXME: figure out how to handle situations where the file contains no
         patterns whatsoever. */
      data->ready = 1;
      data->atime = time (NULL);
      data->mtime = st_buf.st_mtime;
    } else {
      data->ready = 0;
      data->atime = 0;
      data->mtime = 0;
      vt_set_error (err, tmperr);
      vt_state_error (&data->state);
      return -1;
    }
  }

  return 0;
}

vt_slist_t *
vt_dict_multi_pcre_parse (const char *path, vt_error_t *err)
{
#define BUFLEN (1024)
  char *fmt;
  char *begin, *end, *pattern;
  char *ptr, buf[BUFLEN];
  FILE *fp;
  float weight;
  int escape, line;
  int invert, options;
  vt_error_t tmperr;
  vt_pcre_t *regex;
  vt_slist_t *cur, *root;

  if ((fp = fopen (path, "r")) < 0) {
    fmt = "%s: open %s: %s";
    switch (errno) {
      case EACCES:
      case ELOOP:
      case ENOENT:
      case ENOTDIR:
        vt_set_error (err, VT_ERR_CONNFAILED);
        vt_error (fmt, __func__, path, strerror (errno));
        return (NULL);
      case ENOMEM:
        vt_set_error (err, VT_ERR_NOMEM);
        vt_error (fmt, __func__, path, strerror (errno));
        return (NULL);
      default:
        vt_panic (fmt, __func__, path, strerror (errno));
    }
  }

  line = 0;
  root = NULL;
next:
  while (fgets (buf, BUFLEN, fp) != NULL) {
    line++;
    escape = 0;
    invert = 0;
    options = 0;

    for (ptr = buf; isspace (*ptr); ptr++)
      ; /* ignore leading white space */

    /* ignore empty and comment lines */
    if (! *ptr || *ptr == '#')
      goto next;
    if (*ptr == '!') {
      vt_debug ("%s: pattern on line %d in %s will be inverted",
        __func__, line, path);
      invert = 1;
      ptr++;
    }
    if (*ptr != '/') {
      vt_warning ("%s: missing pattern delimiter on line %d in %s",
        __func__ , line, path);
      goto next;
    }
    begin = ++ptr;
    for (; *ptr && *ptr != '/' && ! escape; ptr++) {
      if (*ptr == '\\')
        escape = escape ? 0 : 1;
    }
    
    if (! *ptr) {
      vt_warning ("%s: missing pattern delimiter on line %d in %s",
        __func__, line, path);
      goto next;
    }
    end = ptr - 1;

    if (begin == end) {
      vt_warning ("%s: empty pattern on line %d in %s", __func__, line, path);
      goto next;
    }

    for (++ptr; *ptr && ! isspace (*ptr); ptr++) {
      switch (*ptr) {
        case 'i':
          options |= PCRE_CASELESS;
          break;
        default:
          vt_warning ("%s: unsupported flag '%c' on line %d in %s",
            __func__, *ptr, line, path);
          goto next;
      }
    }

    for (; isspace (*ptr); ptr++)
      ;

    errno = 0;
    if ((weight = strtof (ptr, NULL)) == 0.0 && errno != 0) {
      vt_warning ("%s: unsupported weight on line %d in %s",
        __func__, line, path);
      goto next;
    }

    /* parse pattern and add it to the list */
    if (! (pattern = strndup (begin, (end - begin) + 1))) {
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: strndup: %s", __func__, strerror (ENOMEM));
      goto failure;
    }
vt_error ("%s: pattern: %s, weight: %f", __func__, pattern, weight);
    tmperr = 0;
    regex = vt_pcre_create (pattern, options, invert, weight, &tmperr);
    free (pattern);

    if (! regex) {
      if (tmperr == VT_ERR_BADCFG) {
        vt_warning ("%s: invalid pattern on line %d in %s",
          __func__, line, path);
        goto next;
      } else {
        vt_set_error (err, tmperr);
        goto failure;
      }
    }

    if (! (cur = vt_slist_append (root, (void *)regex))) {
      (void)vt_pcre_destroy (regex, NULL);
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: vt_slist_append: %s", __func__, strerror (ENOMEM));
      goto failure;
    }

    if (! root)
      root = cur;
  }

  (void)fclose (fp);
  return root;
failure:
  (void)fclose (fp);
  (void)vt_slist_free (root, &vt_slist_pcre_free, 0);
  return NULL;
#undef BUFLEN
}

int
vt_dict_multi_pcre_check (vt_dict_t *dict,
                          vt_request_t *req,
                          vt_result_t *res,
                          int pos,
                          vt_error_t *err)
{
  char *member;
  float weight;
  int ret;
  vt_dict_multi_pcre_t *data;
  vt_error_t tmperr;
  vt_pcre_t *regex;
  vt_slist_t *cur;

  assert (dict);
  assert (req);
  assert (res);
  data = (vt_dict_multi_pcre_t *)dict->data;
  assert (data);

  if ((ret = pthread_rwlock_rdlock (&data->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_rdlock: %s", __func__, strerror (ret));

  tmperr = 0;
  if (! (member = vt_request_mbrbyid (req, data->member))) {
    if (data->invert)
      weight = data->weight;
    else
      weight = 0.0;
    goto update;
  }

  if ((ret = vt_dict_multi_pcre_open (data, &tmperr)) != 0) {
    vt_set_error (err, tmperr);
    goto unlock;
  }

  for (cur = data->regexes; cur; cur = cur->next) {
    if ((regex = (vt_pcre_t *)cur->data)) {
      if (vt_pcre_check (&weight, member, regex, &tmperr) == 0) {
        goto update;
      } else if (tmperr != 0) {
        vt_set_error (err, tmperr);
        goto unlock;
      }
    }
  }

update:
vt_error ("%s:%d: pos: %d, weight: %f", __func__, __LINE__, pos, weight);
  vt_result_update (res, pos, weight);
unlock:
  if ((ret = pthread_rwlock_unlock (&data->lock)) != 0)
    vt_panic ("%s: pthread_rwlock_unlock: %s", __func__, strerror (ret));
  return tmperr ? -1 : 0;
}
