#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <stdint.h>
#define __USE_GNU
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <xlocale.h>
#include <unictype.h>
#include <unistr.h>

#include <valiant/string.h>

static locale_t posix_locale;
static pthread_once_t posix_locale_once = PTHREAD_ONCE_INIT;


/* ASCII table for functions like the ones in <ctype.h> that are not affected
   by locale. copied almost directly from GLib under the LGPL license. */
static const unsigned int ascii_table_data[256] = {
  0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
  0x004, 0x104, 0x104, 0x104, 0x104, 0x104, 0x004, 0x004,
  0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
  0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
  0x140, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x459, 0x459, 0x459, 0x459, 0x459, 0x459, 0x459, 0x459,
  0x459, 0x459, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x0d0, 0x653, 0x653, 0x653, 0x653, 0x653, 0x653, 0x253,
  0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253,
  0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253,
  0x253, 0x253, 0x253, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x0d0, 0x473, 0x473, 0x473, 0x473, 0x473, 0x473, 0x073,
  0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073,
  0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073,
  0x073, 0x073, 0x073, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x004
  /* the upper 128 are all zeroes */
};

const unsigned int * const ascii_table = ascii_table_data;

#define TOUPPER(chr) \
  ((chr) >= 'a' && (chr) <= 'z' ? (chr) + ('A' - 'a') : (chr))
#define TOLOWER(chr) \
  ((chr) >= 'A' && (chr) <= 'Z' ? (chr) - ('A' - 'a') : (chr))

static void set_posix_locale (void);
static locale_t get_posix_locale (void);

#ifndef NDEBUG
static int string_is_sane (string_t *)
  __attribute__ ((nonnull));
#endif


static void
set_posix_locale (void)
{
  posix_locale = newlocale (LC_ALL, "POSIX", NULL);
}

static locale_t
get_posix_locale (void)
{
  (void)pthread_once (&posix_locale_once, &set_posix_locale);

  return posix_locale;
}

int
strcasecmp_c (const uint8_t *s1, const uint8_t *s2)
{
  char *p1, *p2;
  int c1, c2;

  assert (s1 != NULL);
  assert (s2 != NULL);

  for (p1 = (char *)s1, p2 = (char *)s2; *p1 && *p2; p1++, p2++) {
    c1 = (int)(uint8_t) TOLOWER (*s1);
    c2 = (int)(uint8_t) TOLOWER (*s2);
    if (c1 != c2)
      return (c1 - c2);
  }

  return (((int)(uint8_t) *p1) - ((int)(uint8_t) *p2));
}

int
strncasecmp_c (const uint8_t *s1, const uint8_t *s2, size_t n)
{
  char *p1, *p2;
  int c1, c2;

  assert (s1 != NULL);
  assert (s2 != NULL);

  for (p1 = (char *)s1, p2 = (char *)s2; *p1 && *p2 && n; p1++, p2++, n--) {
      c1 = (int)(uint8_t) TOLOWER (*s1);
      c2 = (int)(uint8_t) TOLOWER (*s2);
      if (c1 != c2)
        return (c1 - c2);
  }

  if (n) {
    return (((int)(uint8_t) *p1) - ((int)(uint8_t) *p2));
  } else {
    return 0;
  }
}

int
todigit_c (int chr)
{
  int num;

  if (chr >= '0' && chr <= '9') {
    num = chr - '0';
  } else if (chr >= 'a' && chr <= 'f') {
    num = chr - 'a';
  } else if (chr >= 'A' && chr <= 'F') {
    num = chr - 'A';
  } else {
    num = -1;
  }

  return num;
}

int
toupper_c (int chr)
{
  return TOUPPER (chr);
}

int
tolower_c (int chr)
{
  return TOLOWER (chr);
}

int
strtol_c (long *ptr, const uint8_t *str, uint8_t **end, int base)
{
  int err = 0;
  locale_t loc = NULL;
  long lng;

  assert (ptr != NULL);
  assert (str != NULL);
  assert (base >= 0);

  loc = get_posix_locale ();
  err = errno;
  errno = 0;
  lng = strtol_l ((const char *)str, (char **)end, base, loc);
  if (errno != 0) {
    if (lng != 0) {
      *ptr = lng;
    }
    err = errno;
  } else {
    errno = err;
    *ptr = lng;
  }

  return err;
}

int
strtod_c (double *ptr, const uint8_t *str, uint8_t **end)
{
  int err = 0;
  locale_t loc = NULL;
  double dbl;

  assert (ptr != NULL);
  assert (str != NULL);

  loc = get_posix_locale ();
  err = errno;
  errno = 0;
  dbl = strtod_l ((const char *)str, (char **)end, loc);
  if (errno != 0) {
    if (dbl != 0) {
      *ptr = dbl;
    }
    err = errno;
  } else {
    errno = err;
    *ptr = dbl;
  }

  return err;
}

#ifndef NDEBUG
static int
string_is_sane (string_t *str)
{
  int err = EINVAL;

  if (str != NULL) {
    if (str->buf == NULL &&
        str->len == 0 &&
        str->cnt == 0)
    {
      err = 0;
    } else if ((str->cnt >= str->pos) &&
               (str->cnt <= str->len) &&
               (str->len <= str->lim || str->lim == 0) &&
               (u8_check (str->buf, str->cnt) == NULL))
    {
      err = 0;
    }
  }

  return err;
}
#endif

int
string_construct (string_t *str, const uint8_t *chrs, size_t len)
{
  int err = 0;

  assert (str != NULL);
  /* user may specify NULL string with length of zero to initialize empty
     string, but he must be consistent */
  assert ((chrs != NULL && len != 0) ||
          (chrs == NULL && len == 0));

  (void)memset (str, 0, sizeof (string_t));
  if (chrs != NULL) {
    err = string_copy (str, chrs, len);
  }

  return err;
}

int
string_create (string_t **a_str, const uint8_t *chrs, size_t len)
{
  int err = 0;
  string_t *str;

  assert (a_str != NULL);
  assert ((chrs != NULL && len != 0) ||
          (chrs == NULL && len == 0));

  if ((str = calloc (1, sizeof (string_t))) == NULL) {
    err = errno;
  } else if ((err = string_construct (str, chrs, len)) != 0) {
    string_destroy (str);
  } else {
    *a_str = str;
  }

  return err;
}

void
string_destruct (string_t *str)
{
  assert (string_is_sane (str) == 0);
  string_clear (str);
}

void
string_destroy (string_t *str)
{
  if (str != NULL) {
    assert (string_is_sane (str) == 0);
    string_destruct (str);
    free (str);
  }
}

void
string_clear (string_t *str)
{
  assert (string_is_sane (str) == 0);

  if (str->buf != NULL) {
    free (str->buf);
  }

  memset (str, 0, sizeof (string_t));
}

uint8_t *
string_get_buffer (string_t *str)
{
  uint8_t *ptr;

  assert (string_is_sane (str) == 0);

  if (str->buf != NULL) {
    ptr = str->buf + str->pos;
  } else {
    ptr = NULL;
  }

  return ptr;
}

size_t
string_get_length (string_t *str)
{
  assert (string_is_sane (str) == 0);
  return str->cnt - str->pos;
}

size_t
string_get_limit (string_t *str)
{
  assert (string_is_sane (str) == 0);
  return str->lim;
}

int
string_set_limit (string_t *str, size_t lim)
{
  int err = 0;

  assert (string_is_sane (str) == 0);

  if (lim < SIZE_MAX) {
    str->lim = lim;
  } else {
    err = ERANGE;
  }

  return err;
}

size_t
string_get_size (string_t *str)
{
  assert (string_is_sane (str) == 0);
  return str->len;
}

int
string_set_size (string_t *str, size_t len)
{
  int err = 0;
  uint8_t *buf;
  size_t lim;

  assert (string_is_sane (str) == 0);

  if (str->lim > 0 && str->lim < SIZE_MAX) {
    lim = str->lim;
  } else {
    /* extra byte for null termination */
    lim = SIZE_MAX - 1;
  }

  if (len > lim) {
    err = ERANGE;
  } else {
    /* allocate memory only if necessary */
    if (len > str->len) {
      if ((buf = realloc (str->buf, len + 1)) == NULL) {
        err = errno;
      } else {
        str->buf = buf;
        str->len = len;
        /* do not update counter */
      }
    }
  }

  return err;
}

size_t
string_get_state (string_t *str)
{
  assert (string_is_sane (str) == 0);
  return str->pos;
}

int
string_set_state (string_t *str, size_t off)
{
  int err = 0;
  int ret;
  ucs4_t chr;

  assert (string_is_sane (str) == 0);

  if (off <= str->cnt) {
    ret = u8_mbtouc (&chr, str->buf + off, (str->cnt - off));
    if (ret == -1) {
      err = EINVAL;
    } else {
      str->pos = off;
    }
  } else {
    err = ERANGE;
  }

  return err;
}

int
string_copy (string_t *str, const uint8_t *chrs, size_t len)
{
  int err = 0;

  assert (string_is_sane (str) == 0);
  assert (chrs != NULL && len != 0 && u8_check (chrs, len) == NULL);

  if (str->lim == 0 || str->lim <= len) {
    if ((err = string_set_size (str, len)) == 0) {
      (void)memmove (str->buf, chrs, len);
      str->cnt = len;
    }
  } else {
    err = ERANGE;
  }

  return err;
}

int
string_append (string_t *str, const uint8_t *chrs, size_t len)
{
  int err = 0;
  size_t tot;

  assert (string_is_sane (str) == 0);
  assert (chrs != NULL && len != 0 && u8_check (chrs, len) == NULL);

  if (len > (SIZE_MAX - str->cnt)) {
    err = ERANGE;
  } else {
    tot = str->cnt + len;
    if (str->lim == 0 || str->lim >= tot) {
      if ((err = string_set_size (str, tot)) == 0) {
        (void)memmove (str->buf + str->cnt, chrs, len);
        str->cnt = tot;
      }
    } else {
      err = ERANGE;
    }
  }

  return err;
}

ssize_t
string_find_char (string_t *str, ucs4_t chr)
{
  ssize_t pos = -1;
  uint8_t *ptr;

  assert (string_is_sane (str) == 0);

  if ((ptr = u8_chr (str->buf + str->pos, str->cnt - str->pos, chr)) != NULL) {
    pos = (ssize_t)(((uint8_t *)ptr) - str->buf);
  }

  return pos;
}

int
string_get_char (ucs4_t *a_chr, string_t *str)
{
  int err = 0;
  int ret;
  ucs4_t chr = '\0';

  assert (string_is_sane (str) == 0);
  assert (a_chr != NULL);

  if (str->pos < str->cnt) {
    ret = u8_mbtouc (
      &chr, (uint8_t *)(str->buf + str->pos), str->cnt - str->pos);
    if (ret == -1) {
      err = EINVAL;
    }
  }

  if (err == 0) {
    *a_chr = chr;
  }

  return err;
}

/* FIXME: implement string_has_next_char */

int
string_get_next_char (ucs4_t *a_chr, string_t *str)
{
  int err = 0;
  int rep, ret;
  size_t pos;
  ucs4_t chr = '\0';

  assert (string_is_sane (str) == 0);

  for (rep = 2; rep > 0 && err == 0; rep--) {
    if ((str->cnt - pos) >= 0) {
      ret = u8_mbtouc (&chr, str->buf + str->pos, str->cnt - str->pos);
      if (ret < 0) {
        err = EINVAL;
      } else {
        assert (ret != 0);
        if (rep == 2) {
          str->pos += (size_t)ret;
        }
      }
    } else {
      chr = '\0';
    }
  }

  if (err == 0 && a_chr != NULL) {
    *a_chr = chr;
  }

  return err;
}

/* FIXME: implement string_has_previous_char */

int
string_get_previous_char (ucs4_t *a_chr, string_t *str)
{
  uint8_t *ptr;
  ucs4_t chr = 0;

  assert (string_is_sane (str) == 0);

  ptr = (uint8_t *)u8_prev (&chr, str->buf + str->pos, str->buf);
  if (ptr == NULL) {
    chr = 0;
    str->pos = 0;
  } else {
    str->pos = (size_t)(((uint8_t *)ptr) - str->buf);
  }

  if (a_chr != NULL) {
    *a_chr = chr;
  }

  return 0;
}
