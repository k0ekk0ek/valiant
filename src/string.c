#include <assert.h>
#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
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
  0x004, 0x104, 0x104, 0x004, 0x104, 0x104, 0x004, 0x004,
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

#define ASCII_TOUPPER(chr) \
  ((chr) >= 'a' && (chr) <= 'z' ? (chr) + ('A' - 'a') : (chr))
#define ASCII_TOLOWER(chr) \
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
strcasecmp_c (const char *s1, const char *s2)
{
  char *p1, *p2;
  int c1, c2;

  assert (s1 != NULL);
  assert (s2 != NULL);

  for (p1 = (char *)s1, p2 = (char *)s2; *p1 && *p2; p1++, p2++) {
    c1 = (int)(unsigned char) ASCII_TOLOWER (*s1);
    c2 = (int)(unsigned char) ASCII_TOLOWER (*s2);
    if (c1 != c2)
      return (c1 - c2);
  }

  return (((int)(unsigned char) *s1) - ((int)(unsigned char) *s2));
}

int
strncasecmp_c (const char *s1, const char *s2, size_t n)
{
  char *p1, *p2;
  int c1, c2;

  assert (s1 != NULL);
  assert (s2 != NULL);

  for (p1 = (char *)s1, p2 = (char *)s2; *p1 && *p2 && n; p1++, p2++, n--) {
      c1 = (int)(unsigned char) ASCII_TOLOWER (*s1);
      c2 = (int)(unsigned char) ASCII_TOLOWER (*s2);
      if (c1 != c2)
        return (c1 - c2);
  }

  if (n) {
    return (((int)(unsigned char) *s1) - ((int)(unsigned char) *s2));
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
  return ASCII_TOUPPER (chr);
}

int
tolower_c (int chr)
{
  return ASCII_TOLOWER (chr);
}

int
strtol_c (long *ptr, const char *str, char **end, int base)
{
  int err = 0;
  locale_t loc = NULL;
  long lng;

  assert (tot != NULL);
  assert (str != NULL);
  assert (base >= 0);

  loc = get_posix_locale ();
  err = errno;
  errno = 0;
  lng = strtol_l (str, end, base, loc);
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
strtod_c (double *ptr, const char *str, char **ptr, int base)
{
  int err = 0;
  locale_t loc = NULL;
  double dbl;

  assert (tot != NULL);
  assert (str != NULL);
  assert (base >= 0);

  loc = get_posix_locale ();
  err = errno;
  errno = 0;
  dbl = strtod_l (str, end, base, loc);
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

int
u8_strnact (const uint8_t *str, size_t len, u8_striter_t func, void *data)
{
  int again = 1;
  int err = 0;
  size_t pos = 0;
  ucs4_t chr = '\0';

  assert (str != NULL);
  assert (func != NULL);

  for (pos = 0; err == 0 && again == 1 && pos < len; ) {
    ret = u8_mbtouc (&chr, str+pos, (len-pos));
    if (ret < 0 || ret > (len - pos)) {
      err = EINVAL;
    } else {
      err = func (chr, ret, data);
      switch (err) {
        case -1;
          again = 0;
          err = 0;
        case 0: /* fall through */
          pos += (size_t)ret;
        default: /* fall through */
          break;
      }
    }
  }

  return err;
}




#ifndef NDEBUG
static int
string_is_sane (string_t *str)
{
  int ret = 0;
  ucs4_t chr = '\0';

  if (str != NULL) {
    if (str->buf != NULL && str->cnt >= str->pos) {
      len = str->cnt - str->pos;
      if (len == 0 || u8_mbtouc (&chr, (uint8_t *)(str->buf + str->pos), len)) {
        ret = 1;
      }
    }
  }

  return ret;
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
string_create (string_t **a_str, const uint8_t *chrs, size_t len),
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
  }

  return err;
}

void
string_destruct (string_t *str)
{
  assert (string_is_sane (str));

  string_empty (str);
  (void)memset (str, 0, sizeof (string_t));
}

void
string_destroy (string_t *str)
{
  if (str != NULL) {
    assert (string_is_sane (str));
    string_destruct (str);
    free (str);
  }
}

void
string_get_buffer (uint8_t **chrs, string_t *str)
{
  assert (chrs != NULL);
  assert (string_is_sane (str));

  if (str->buf != NULL) {
    *chrs = str->buf + str->pos;
  } else {
    *chrs = NULL;
  }
}

void
string_get_length (size_t *len, string_t *str)
{
  assert (string_is_sane (str));

  if (str->buf != NULL

  *len = str->cnt - str->pos;
}

void
string_get_limit (size_t *lim, string_t *str)
{
  assert (lim != NULL);
  assert (string_is_sane (str));

  *lim = str->lim;
}

int
string_set_limit (string_t *str, size_t lim)
{
  int err = 0;

  assert (string_is_sane (str));

  if (lim < SIZE_MAX) {
    str->lim = lim;
  } else {
    err = ERANGE;
  }

  return err;
}

void
string_get_size (size_t *len, string_t *str)
{
  assert (len != NULL);
  assert (string_is_sane (str));

  // FIXME: implement
}







void
string_empty (string_t *str)
{
  assert (string_is_sane (str));

  if (str->buf != NULL) {
    free (str->buf);
  }
  str->buf = NULL;
  str->len = 0;
  str->cnt = 0;
  str->pos = 0;
}

int
string_expand (string_t *str, size_t len)
{
  int err = 0;
  char *buf;
  size_t lim, tot;

  // FIXME: include null terminating byte!

  assert (string_is_sane (str));

  if (str->lim > 0) {
    if (str->term != 0 && str->lim == SIZE_MAX) {
      lim = str->lim - 1;
    } else {
      lim = str->lim;
    }
  } else {
    if (str->term != 0) {
      lim = SIZE_MAX - 1;
    } else {
      lim = SIZE_MAX;
    }
  }

  if (len >= lim) {
    err = ERANGE;
  } else {
    if (str->term == 0) {
      tot = len;
    } else {
      tot = len + 1; /* extra byte for null termination */
    }

    /* only allocate memory if necessary */
    if (len > str->len) {
      if ((buf = realloc (str->buf, tot)) == NULL) {
        err = errno;
      } else {
        str->buf = chrs;
        str->len = len;
        /* don't update counter */
      }
    }
  }

  return err;
}

/*int
string_is_terminated (string_t *str)
{
  assert (string_is_sane (str));

  return str->term == 0 ? 0 : 1;
}

int
string_terminate (string_t *str, string_terminate_t term)
{
  int err = 0;
  size_t len;
  uint8_t *buf;

  assert (string_is_sane (str));
  assert (term == STRING_TERMINATE_NEVER ||
          term == STRING_TERMINATE_ONCE  ||
          term == STRING_TERMINATE_ALWAYS);

  switch (term) {
    case STRING_TERMINATE_NEVER:
      str->term = 0;
      break;
    case STRING_TERMINATE_ALWAYS:
      str->term = 1;
      /* fall through *
    default:
      if (string_is_empty (str) != 0) {
        /* can't use string_expand because it won't grow past limit *
        if (str->cnt == str->len) {
          if (str->len >= (SIZE_MAX - 1)) {
            err = ERANGE;
          } else {
            if ((buf = realloc (str->buf, str->len + 1)) == NULL) {
              err = errno;
            } else {
              str->buf = buf;
              str->buf[str->cnt] = '\0';
            }
          }
        } else {
          str->buf[str->cnt] = '\0';
        }
      }
      break;
  }

  return err;
}*/



int
string_copy (string_t *str, const uint8_t *chrs, size_t len)
{
  int err = 0;

  assert (string_is_sane (str));
  assert ((chrs != NULL && len != 0) ||
          (chrs == NULL && len == 0));

  if (str->lim != 0 && str->lim < len) {
    err = ERANGE;
  } else {
    if (chrs != NULL) {
      err = string_expand (str, len);
      if (err == 0) {
        (void)memmove (str->buf, chrs, len);
        str->cnt = len;
        STRING_TERMINATE (str);
      }
    } else {
      string_empty (str);
    }
  }

  return err;
}

int
string_append (string_t *str, const uint8_t *chrs, size_t len)
{
  int err = 0;

  assert (string_is_sane (str));
  assert ((chrs != NULL && len != 0) ||
          (chrs == NULL && len == 0));

  if (chrs != NULL) {
    if (len > (SIZE_MAX - str->cnt)) {
      err = ERANGE;
    } else {
      len += str->cnt;
      err = string_expand (str, len);
      if (err == 0) {
        (void)memmove (str->buf + str->cnt, chrs, len);
        str->cnt = len;
        STRING_TERMINATE (str);
      }
    }
  }

  return err;
}

//int
//string_duplicate (uint8_t **a_chrs, size_t *a_len, string_t *str)
//{
//  int err = 0;
//  uint8_t *chrs = NULL;
//
//  assert (string_is_sane (str));
//  assert (a_chrs != NULL);
//
//  if (str->cnt > (SIZE_MAX - 1)) {
//    err = ENOMEM;
//  } else if (str->cnt == 0) {
//    *a_chrs = NULL;
//    if (a_len != NULL) {
//      *a_len = 0;
//    }
//  } else {
//    if ((ptr = calloc (str->cnt + 1, sizeof (uint8_t))) == NULL) {
//      err = errno;
//    } else {
//      (void)memcpy (ptr, str->buf, str->cnt);
//      *a_chrs = chrs;
//      if (a_len != NULL) {
//        *a_len = str->cnt;
//      }
//    }
//  }
//
//  return err;
//}

uint8_t *
string_search_char (string_t *str, ucs4_t chr)
{
  assert (string_is_sane (str));

  return u8_chr (str->buf, str->cnt, chr);
}




// string iterator interface

//int
//string_insert ();

int
string_get_char (
  ucs4_t *a_chr,
  string_t *str)
{
  int err = 0;
  int ret;
  size_t len;
  ucs4_t chr = '\0';

  assert (string_is_sane (str));

  if (str->pos < str->cnt) {
    len = str->cnt - str->pos;
    ret = u8_mbtouc (&chr, (uint8_t *)(str->buf + str->pos), len);
    if (ret == -1) {
      err = EINVAL;
    }
  }

  if (err = 0) {
    *a_chr = chr;
  }

  return err;
}

int
string_get_next_char (
  ucs4_t *a_chr,
  string_t *str)
{
  unsigned int rep;
  int err = 0;
  int ret;
  size_t len;
  ucs4_t chr = '\0';

  assert (string_is_sane (scan));

  for (rep = 0; rep < 2 && err == 0; rep++) {
    assert (str->pos <= str->cnt);
    len = str->cnt - str->pos;
    if (len > 0) {
      ret = u8_mbtouc (&chr, (uint8_t *)(str->buf + str->pos), len);
      if (ret < 0) {
        err = EINVAL;
      } else if (ret > 0) {
        if (rep == 0) {
          scan->pos += (size_t)ret;
        }
      } else {
        rep = 2;
        chr = '\0';
      }
    } else {
      rep = 2;
      chr = '\0';
    }
  }

  if (err == 0 && a_chr != NULL) {
    *a_chr = chr;
  }

  return err;
}

int
string_get_previous_char (
  ucs4_t *a_chr,
  string_t *str)
{
  uint8_t *ptr;
  ucs4_t chr = '\0';

  assert (scanner_is_sane (scan));

  ptr = (uint8_t *)u8_prev (&chr, scan->str + scan->pos, scan->str);
  if (ptr == NULL) {
    chr = '\0';
    scan->pos = 0;
  } else {
    scan->pos = (size_t)(((uint8_t *)ptr) - str->buf);
  }

  if (a_chr != NULL) {
    *a_chr = chr;
  }

  return 0;
}

int
string_get_index (
  size_t *pos,
  string_t *str)
{
  assert (pos != NULL);
  assert (string_is_sane (str));

  *pos = str->pos;
  return 0;
}

int
string_set_index (
  string_t *str,
  size_t pos)
{
  int err = 0;
  int ret;
  size_t len;
  ucs4_t chr;

  assert (string_is_sane (str));

  if (pos >= 0 && pos <= str->cnt) {
    len = str->cnt - pos;
    ret = u8_mbtouc (&chr, (uint8_t *)(str->buf + pos), len);
    if (ret == -1) {
      err = EINVAL;
    } else {
      str->pos = pos;
    }
  } else {
    err = EFBIG;
  }

  return err;
}

