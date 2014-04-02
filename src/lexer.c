/* system includes */
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistr.h>

/* valiant includes */
#define LEXER_EXPORT_MACROS 1
#include <valiant/lexer.h>
#include <valiant/string.h>
#include <valiant/value.h>

// http://en.wikipedia.org/wiki/Unicode_character_property#General_Category

/* generate struct members */
#define X_STR(name) uint8_t *name;
#define X_CHAR(name) ucs4_t name;
#define X_BOOL(name) bool name;
#define X(type,name,value) X_ ## type (name)
typedef struct {
  LEXER_CONTEXT_APPLY(X)
} lexer_context_t;
#undef X
#undef X_BOOL
#undef X_CHAR
#undef X_STR

typedef struct {
  size_t pos; /* scanner state */
  size_t lns; /* line number */
  size_t cols; /* column */
  token_t tok;
  value_t val;
} lexer_term_t;

struct lexer {
  string_t scan; /* scanner */
  size_t pos; /* scanner state */
  size_t lns; /* line number */
  size_t cols; /* column */
  int eof; /* end of file */
  ucs4_t dquot; /* double quote character */
  ucs4_t squot; /* single quote character */
  lexer_context_t ctx; /* options */
  lexer_term_t term; /* current token/value in sequence */
};

static int lexer_get_char (ucs4_t *, lexer_t *)
  __attribute__ ((nonnull(2)));
static int lexer_get_next_char (ucs4_t *, lexer_t *)
  __attribute__ ((nonnull(2)));
static int lexer_get_previous_char (ucs4_t *, lexer_t *)
  __attribute__ ((nonnull(2)));
static size_t lexer_get_state (lexer_t *)
  __attribute__ ((nonnull));
static int lexer_set_state (lexer_t *, size_t)
  __attribute__ ((nonnull(1)));
static int lexer_unesc_str_dquot (uint8_t **, const uint8_t *, size_t, ucs4_t)
  __attribute__ ((nonnull(1,2)));
static int lexer_unesc_str_squot (uint8_t **, const uint8_t *, size_t, ucs4_t)
  __attribute__ ((nonnull(1,2)));
static int lexer_is_skip_char (lexer_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
static int lexer_is_ident_first (lexer_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
static int lexer_is_ident (lexer_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
static ucs4_t *lexer_is_str_quot (lexer_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
static bool lexer_scan_str_quot (lexer_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
static bool lexer_skip_str_quot (lexer_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
static int lexer_is_num_first (lexer_t *, ucs4_t)
  __attribute__ ((nonnull(1)));

int
lexer_create (lexer_t **a_lex, const uint8_t *str, size_t len)
{
  int err;
  lexer_t *lex;

  assert (a_lex != NULL);
  assert (str != NULL);

  if ((lex = calloc (1, sizeof (lexer_t))) == NULL) {
    err = errno;
  } else {
    err = 0;
    string_construct (&lex->scan, str, len);
    *a_lex = lex;

    /* set defaults */
#define X(type,name,value) lex->ctx.name = value;
LEXER_CONTEXT_APPLY(X)
#undef X
  }

  return err;
}

void
lexer_destroy (lexer_t *lex)
{
  assert (lex != NULL);

  string_destruct (&lex->scan);
  value_clear(&lex->term.val);

#define X_STR(name)            \
  if (lex->ctx.name != NULL) { \
    free (lex->ctx.name);      \
  }
#define X_CHAR(name)
#define X_BOOL(name)
#define X(type,name,value) X_ ## type (name)
LEXER_CONTEXT_APPLY(X)
#undef X
#undef X_BOOL
#undef X_CHAR
#undef X_STR

  memset (lex, 0, sizeof (lexer_t));
  free (lex);
}

/* generate context accessor functions */
#define X_STR(name)                                   \
int                                                   \
lexer_get_ ## name (uint8_t **a_str, lexer_t *lex)    \
{                                                     \
  int err = 0;                                        \
  uint8_t *str = NULL;                                \
                                                      \
  assert (a_str != NULL);                             \
  assert (lex != NULL);                               \
                                                      \
  str = (uint8_t *)strdup ((char *)lex->ctx.name);    \
  if (str != NULL) {                                  \
    *a_str = str;                                     \
  } else {                                            \
    err = errno;                                      \
  }                                                   \
                                                      \
  return err;                                         \
}                                                     \
int                                                   \
lexer_set_ ## name (lexer_t *lex, const uint8_t *str) \
{                                                     \
  int err = 0;                                        \
  uint8_t *a_str;                                     \
                                                      \
  assert (lex != NULL);                               \
                                                      \
  a_str = (uint8_t *)strdup ((char *)str);            \
  if (a_str != NULL) {                                \
    if (lex->ctx.name != NULL) {                      \
      free (lex->ctx.name);                           \
    }                                                 \
    lex->ctx.name = a_str;                            \
  } else {                                            \
    err = errno;                                      \
  }                                                   \
                                                      \
  return err;                                         \
}

#define X_CHAR(name)                                  \
ucs4_t                                                \
lexer_get_ ## name (lexer_t *lex)                     \
{                                                     \
  assert (lex != NULL);                               \
  return lex->ctx.name;                               \
}                                                     \
void                                                  \
lexer_set_ ## name (lexer_t *lex, ucs4_t chr)         \
{                                                     \
  assert (lex != NULL);                               \
  lex->ctx.name = chr;                                \
}

#define X_BOOL(name)                                  \
bool                                                  \
lexer_get_ ## name (lexer_t *lex)                     \
{                                                     \
  assert (lex != NULL);                               \
  return lex->ctx.name;                               \
}                                                     \
void                                                  \
lexer_set_ ## name (lexer_t *lex, bool bln)           \
{                                                     \
  assert (lex != NULL);                               \
  lex->ctx.name = bln;                                \
}

#define X(type,name,value) X_ ## type (name)

LEXER_CONTEXT_APPLY(X)

#undef X
#undef X_BOOL
#undef X_CHAR
#undef X_STR

/* functions that proxy for their character iterator interface equivalents and
   keep track of line number and column */

static int
lexer_get_char (ucs4_t *a_chr, lexer_t *lex)
{
  assert (a_chr != NULL);
  assert (lex != NULL);

	return string_get_char (a_chr, &lex->scan);
}

static int
lexer_get_next_char (ucs4_t *a_chr, lexer_t *lex)
{
  int err = 0;
  ucs4_t chr = '\0';

  assert (lex != NULL);

  err = string_get_char (&chr, &lex->scan);
  if (err == 0 && chr != '\0') {
    err = string_get_next_char (&chr, &lex->scan);
    if (err == 0) {
      if (chr == '\n') {
        lex->lns++;
        lex->cols = 1;
      } else {
        lex->cols++;
      }
    }
	}

  if (err == 0) {
    *a_chr = chr;
  }

	return err;
}

static int
lexer_get_previous_char (ucs4_t *a_chr, lexer_t *lex)
{
  int err = 0;
  size_t cols = 1;
  size_t pos, rev;
  ucs4_t chr;

  assert (lex != NULL);

  rev = string_get_state (&lex->scan);
  err = string_get_previous_char (a_chr, &lex->scan);
  if (err == 0) {
    if (*a_chr == '\n') {
      assert (lex->lns > 1);
      cols = 1;

      /* save position, count columns to previous newline */
      pos = string_get_state (&lex->scan);
      for (err = string_get_previous_char (&chr, &lex->scan);
           err == 0 && chr != '\n' && chr != '\0';
           err = string_get_previous_char (&chr, &lex->scan), cols++)
      {
        /* do nothing */
      }

      if (err == 0) {
        string_set_state (&lex->scan, pos);
        lex->lns--;
        lex->cols = cols;
      } else {
        string_set_state (&lex->scan, rev);
      }
    } else if (*a_chr != '\0') {
      assert (lex->cols > 1);
      lex->cols--;
    }
  }

  return err;
}

static size_t
lexer_get_state (lexer_t *lex)
{
  assert (lex != NULL);

  return string_get_state (&lex->scan);
}

static int
lexer_set_state (lexer_t *lex, size_t off)
{
  int err = 0;
  int(*func)(ucs4_t *, lexer_t *);
  size_t pos;

  assert (lex != NULL);

  pos = lexer_get_state (lex);
  if (pos < off) {
    func = &lexer_get_next_char;
  } else {
    func = &lexer_get_previous_char;
  }

  for (; err == 0 && pos != off; ) {
    if ((err = func (NULL, lex)) == 0) {
      pos = lexer_get_state (lex);
    }
  }

  if (err == 0) {
    assert (pos == off);
  }

  return err;
}

static int
lexer_unesc_str_dquot (
  uint8_t **a_str,
  const uint8_t *str,
  size_t len,
  ucs4_t quot)
{
  int again;
  int err = 0;
  int esc;
  int ret;
  size_t cnt = 0;
  size_t pos, tot;
  ucs4_t chr;
  uint8_t *ptr;

  assert (a_str != NULL);
  assert (str != NULL);

  cnt = 0;

  for (again = 1, esc = 0, pos = 0; err == 0 && again == 1 && pos < len; ) {
    ret = u8_mbtouc (&chr, str+pos, len-pos);
    if (ret < 0 || (size_t)ret > len - pos) {
      err = EINVAL;
    } else {
      pos += (size_t)ret;
      if (esc == 1) {
        esc = 0;
        cnt += (size_t)ret;
      } else if (chr == '\0' || chr == quot) {
        again = 0;
        cnt += 1;
      } else if (chr == '\\') {
        esc = 1;
      } else {
        cnt += (size_t)ret;
      }
    }
  }

  if (err == 0) {
    tot = cnt;
    cnt = 0;
    ptr = calloc (tot, sizeof (uint8_t));
    if (ptr != NULL) {
      for (again = 1, esc = 0, pos = 0; err == 0 && again == 1 && pos < len; ) {
        ret = u8_mbtouc (&chr, str + pos, len - pos);
        assert (ret > 0 && (size_t)ret <= len - pos);
        pos += (size_t)ret;

        if (esc == 1) {
          if (chr == 'a') {
            chr = '\a';
          } else if (chr == 'b') {
            chr = '\b';
          } else if (chr == 'f') {
            chr = '\f';
          } else if (chr == 'n') {
            chr = '\n';
          } else if (chr == 'r') {
            chr = '\r';
          } else if (chr == 't') {
            chr = '\t';
          } else if (chr == 'v') {
            chr = '\v';
          }

          ret = u8_uctomb (ptr + cnt, chr, tot - cnt);
          assert (ret > 0 && (size_t)ret <= tot - cnt);
          cnt += (size_t)ret;
        } else {
          if (chr == '\0' || chr == quot) {
            again = 0;
          } else if (chr == '\\') {
            esc = 1;
          } else {
            ret = u8_uctomb (ptr + cnt, chr, tot - cnt);
            assert (ret > 0 && (size_t)ret <= tot - cnt);
            cnt += (size_t)ret;
          }
        }
      }

      /* null terminate */
      ptr[cnt++] = '\0';
      assert (tot == cnt);
    } else {
      err = errno;
    }
  }

  return err;
}

static int
lexer_unesc_str_squot (
  uint8_t **a_str,
  const uint8_t *str,
  size_t len,
  ucs4_t quot)
{
  int again;
  int err = 0;
  int esc;
  int ret;
  size_t cnt = 0;
  size_t pos, tot;
  ucs4_t chr;
  uint8_t *ptr;

  assert (a_str != NULL);
  assert (str != NULL);

  cnt = 0;

  for (again = 1, esc = 0, pos = 0; err == 0 && again == 1 && pos < len; ) {
    ret = u8_mbtouc (&chr, str+pos, len-pos);
    if (ret < 0 || (size_t)ret > len - pos) {
      err = EINVAL;
    } else {
      pos += (size_t)ret;
      if (esc == 1) {
        esc = 0;
        cnt += (size_t)ret;
        if (chr != quot) {
          cnt += 1;
        }
      } else if (chr == '\0' || chr == quot) {
        again = 0;
        cnt += 1;
      } else if (chr == '\\') {
        esc = 1;
      } else {
        cnt += (size_t)ret;
      }
    }
  }

  if (err == 0) {
    tot = cnt;
    cnt = 0;
    ptr = calloc (tot, sizeof (uint8_t));
    if (ptr != NULL) {
      for (again = 1, esc = 0, pos = 0; err == 0 && again == 1 && pos < len; ) {
        ret = u8_mbtouc (&chr, str + pos, len - pos);
        assert (ret > 0 && (size_t)ret <= len - pos);
        pos += (size_t)ret;

        if (esc == 1) {
          if (chr != quot) {
            ptr[cnt++] = '\\';
          }

          ret = u8_uctomb (ptr + cnt, chr, tot - cnt);
          assert (ret > 0 && (size_t)ret <= tot - cnt);
          cnt += (size_t)ret;
        } else {
          if (chr == '\0' || chr == quot) {
            again = 0;
          } else if (chr == '\\') {
            esc = 1;
          } else {
            ret = u8_uctomb (ptr + cnt, chr, tot - cnt);
            assert (ret > 0 && (size_t)ret <= tot - cnt);
            cnt += (size_t)ret;
          }
        }
      }

      /* null terminate */
      ptr[cnt++] = '\0';
      assert (tot == cnt);
    } else {
      err = errno;
    }
  }

  return err;
}

#define lexer_scan_comment_single(p) \
  ((p)->ctx.scan_comment_single == true)
#define lexer_scan_comment_multi(p) \
  ((p)->ctx.scan_comment_multi == true)
#define lexer_skip_comment_single(p) \
  ((p)->ctx.skip_comment_single == true)
#define lexer_skip_comment_multi(p) \
  ((p)->ctx.skip_comment_multi == true)
#define lexer_scan_bool(p) \
  ((p)->ctx.scan_bool == true)
#define lexer_scan_oct(p) \
  ((p)->ctx.scan_oct == true)
#define lexer_scan_int(p) \
  ((p)->ctx.scan_int == true)
#define lexer_scan_hex(p) \
  ((p)->ctx.scan_hex == true)
#define lexer_scan_float(p) \
  ((p)->ctx.scan_float == true)
#define lexer_scan_numeric(p) \
  (lexer_scan_int (p) || lexer_scan_float (p))
#define lexer_int_as_float(p) \
  ((p)->ctx.int_as_float == true)
#define lexer_ident_as_str(p) \
  ((p)->ctx.ident_as_str == true)

static int
lexer_is_skip_char (lexer_t *lex, ucs4_t chr)
{
  int res = 0;

  assert (lex != NULL);

  if (!lex->ctx.skip_chars) {
    if (uc_is_property (chr, UC_PROPERTY_WHITE_SPACE)) {
      res = 1;
    }
  } else if (u8_strchr (lex->ctx.skip_chars, chr)) {
    res = 1;
  }

  return res;
}

static int
lexer_is_ident_first (lexer_t *lex, ucs4_t chr)
{
  int res = 0;

  assert (lex != NULL);

  if (!lex->ctx.ident_first) {
    /* UC_PROPERTY_ID_START is used to distinguish initial letters from
       continuation characters */
    /* Source: http://icu-project.org/apiref/icu4c/uchar_8h.html */
    if (uc_is_property (chr, UC_PROPERTY_ID_START)) {
      if (!lex->ctx.not_ident_first || u8_strchr (lex->ctx.not_ident_first, chr)) {
        res = 1;
      }
    }
  } else if (u8_strchr (lex->ctx.ident_first, chr)) {
    res = 1;
  }

  return res;
}

static int
lexer_is_ident (lexer_t *lex, ucs4_t chr)
{
  int res = 0;

  assert (lex != NULL);

  if (!lex->ctx.ident) {
    if (uc_is_property (chr, UC_PROPERTY_ID_CONTINUE)) {
      if (!lex->ctx.not_ident || u8_strchr (lex->ctx.not_ident, chr)) {
        res = 1;
      }
    }
  } else if (u8_strchr (lex->ctx.ident, chr)) {
    res = 1;
  }

  return res;
}

static ucs4_t *
lexer_is_str_quot (lexer_t *lex, ucs4_t chr)
{
  ucs4_t *ptr = NULL;

  assert (lex != NULL);
  assert (lex->ctx.str_squot != lex->ctx.str_dquot);
  assert (lex->squot == '\0' || lex->dquot == '\0');

  if (lex->squot == '\0' ? chr == lex->ctx.str_squot
                         : chr == lex->squot)
  {
    ptr = &lex->squot;
  } else if (lex->dquot == '\0' ? chr == lex->ctx.str_dquot
                                : chr == lex->dquot)
  {
    ptr = &lex->dquot;
  }

  return ptr;
}

static bool
lexer_scan_str_quot (lexer_t *lex, ucs4_t chr)
{
  bool scan;
  ucs4_t *quot;

  assert (lex != NULL);

  quot = lexer_is_str_quot (lex, chr);
  if (quot == &lex->squot) {
    scan = lex->ctx.scan_str_squot;
  } else {
    assert (quot == &lex->dquot);
    scan = lex->ctx.scan_str_dquot;
  }

  return scan;
}

static bool
lexer_skip_str_quot (lexer_t *lex, ucs4_t chr)
{
  bool skip;
  ucs4_t *quot;

  assert (lex != NULL);

  quot = lexer_is_str_quot (lex, chr);
  if (quot == &lex->squot) {
    skip = lex->ctx.skip_str_squot;
  } else {
    assert (quot == &lex->dquot);
    skip = lex->ctx.skip_str_dquot;
  }

  return skip;
}

static int
lexer_is_num_first (lexer_t *lex, ucs4_t chr)
{
  int res = 0;

  assert (lex != NULL);

  if ((chr == '.' && lexer_scan_float (lex)) ||
      (chr == '+' && lexer_scan_numeric (lex)) ||
      (chr == '-' && lexer_scan_numeric (lex)) ||
      (isdigit_c (chr) && lexer_scan_numeric (lex)))
  {
    res = 1;
  }

  return res;
}

int
lexer_get_token (token_t *a_tok, lexer_t *lex)
{
  int err = 0;

  assert (a_tok != NULL);
  assert (lexer_is_sane (lex) == 0);

  if (lex->term.tok == TOKEN_NONE) {
    err = lexer_get_next_token (a_tok, lex);
  } else {
    *a_tok = lex->term.tok;
  }

  return err;
}

int
lexer_get_next_token (token_t *a_tok, lexer_t *lex)
{
  double dbl;
  int err_dbl, err_lng, err = 0;
  int esc;
  long lng;
  ssize_t cnt = 1;
  size_t lim = 0;
  size_t off = 0;
  size_t lns, cols;
  token_t tok = TOKEN_NONE;
  size_t len;
  ucs4_t chr, *quot;
  uint8_t *end_dbl, *end;
  uint8_t *ptr, *str;

  assert (a_tok != NULL);
  assert (lex != NULL);

  /* operation is stateless, except for opening and closing quotes, but that
     is of no concern to the user */
  if (lex->term.tok == TOKEN_STR_DQUOT ||
      lex->term.tok == TOKEN_STR_SQUOT)
  {
    assert (value_get_type (&lex->term.val) == VALUE_CHAR);
    if (lex->dquot != '\0' || lex->squot != '\0') {
      tok = TOKEN_STR;
    }
  }

  for (cnt = 1; err == 0 && cnt != 0; ) {
    if (cnt == 1) {
      err = lexer_get_char (&chr, lex);

      if (err == 0) {
        switch (tok) {
          case TOKEN_COMMENT_SINGLE:
            if (chr == '\n') {
              if (lexer_skip_comment_single (lex)) {
                tok = TOKEN_NONE;
              } else {
                cnt = 0;
              }
            }
            break;
          case TOKEN_COMMENT_MULTI:
            if (chr == '*') {
              err = lexer_get_char (&chr, lex);
              if (err == 0 && chr == '/') {
                if (lexer_skip_comment_multi (lex)) {
                  tok = TOKEN_NONE;
                } else {
                  cnt = 0;
                }
              }
            }
            break;
          case TOKEN_IDENT:
          case TOKEN_IDENT_NULL:
            if (!lexer_is_ident (lex,chr)) {
              cnt = -1;
            }
            break;
          case TOKEN_STR:
            assert (lex->dquot == '\0' || lex->squot == '\0');

            if (esc || chr == '\\') {
              esc = (esc == 0);
            } else if (lexer_is_str_quot (lex, chr) != NULL) {
              if (lexer_skip_str_quot (lex, chr)) {
                cnt = 0;
              } else {
                cnt = -1;
              }
            }
            break;

          default: /* TOKEN_NONE */
            assert (tok == TOKEN_NONE);
            esc = 0;
            off = lexer_get_state (lex);

            /* double or single quoted string */
            if ((quot = lexer_is_str_quot (lex, chr)) != NULL) {
              if (lexer_scan_str_quot (lex, chr)) {
                if (*quot != '\0') {
                  if (lexer_skip_str_quot (lex, chr)) {
                    tok = TOKEN_NONE;
                    cnt = 2; /* skip current character */
                  } else {
                    if (quot == &lex->squot) {
                      tok = TOKEN_STR_SQUOT;
                    } else {
                      tok = TOKEN_STR_DQUOT;
                    }
                    cnt = 0;
                  }
                  *quot = '\0';
                } else {
                  if (lexer_skip_str_quot (lex, chr)) {
                    tok = TOKEN_STR;
                    cnt = 2; /* skip current character */
                  } else {
                    cnt = 0;
                  }
                  *quot = chr;
                }
              } else {
                tok = TOKEN_NONE;
              }

            /* integer or floating-point number */
            } else if (lexer_is_num_first (lex, chr)) {
              /* optional plus or minus sign */
              if (chr == '+' || chr == '-') {
                err = lexer_get_next_char (&chr, lex);
              }

              if (!err) {
                /* scan for octal and hexadecimal values, short circuit if
                   lexical analyzer should not scan for them */
                if (chr == '0' && !(err = lexer_get_next_char (&chr, lex))) {
                  if ((!lexer_scan_hex (lex) && (chr == 'x' || chr == 'X')) ||
                      (!lexer_scan_oct (lex)))
                  {
                    cnt = 0;
                    lng = 0;
                    tok = TOKEN_INT;
                  }
                }

                if (!err && tok == TOKEN_NONE) {
                  str = lex->scan.buf + off;

                  if (lexer_scan_int (lex)) {
                    err_lng = strtol_c (&lng, str, &end, 0);
                  }
                  if (lexer_scan_float (lex)) {
                    err_dbl = strtod_c (&dbl, str, &end_dbl);
                  }

                  if (end != end_dbl) {
                    end = end_dbl;
                    err = err_dbl;
                    tok = TOKEN_FLOAT;
                  } else if (end != str) {
                    /* in case lexical analyzer must not scan for floating
                       point numbers or no conversion could be performed. the
                       latter, however, is not an error. it simply means that
                       it's not a numeric value */
                    err = err_lng;
                    tok = TOKEN_INT;
                  }

                  if (!err) {
                    assert ((size_t)(end - str) < (SIZE_MAX - off));
                    lexer_set_state (lex, off + (end - str));
                  }
                }
              }

            /* comment line */
            } else if (chr == '#' && lexer_scan_comment_single (lex)) {
              tok = TOKEN_COMMENT_SINGLE;

            /* comment or comment line */
            } else if (chr == '/') {
              err = lexer_get_next_char (&chr, lex);
              if (err == 0) {
                /* comment line */
                if (chr == '/' && lexer_scan_comment_single (lex)) {
                  tok = TOKEN_COMMENT_SINGLE;
                  cnt = 2;
                /* comment */
                } else if (chr == '*' && lexer_scan_comment_multi (lex)) {
                  tok = TOKEN_COMMENT_MULTI;
                  cnt = 2;
                }

                err = lexer_get_previous_char (&chr, lex);
              }
            }

            /* char */
            if (err == 0 && cnt == 1 && tok == TOKEN_NONE) {
              if (lexer_is_ident_first (lex,chr)) {
                tok = TOKEN_IDENT;
              } else if (!lexer_is_skip_char (lex,chr)) {
                tok = TOKEN_CHAR;
                cnt = 0;
              }
            }

            /* register offset */
            lns = lex->lns;
            cols = lex->cols;
            break;
        }
      }
    } else if (cnt > 1) {
      cnt--;
      err = lexer_get_next_char (NULL, lex);
    } else if (cnt < 1) {
      cnt++;
      err = lexer_get_previous_char (NULL, lex);
    }
  }

  if (!err) {
    lim = lexer_get_state (lex);
    str = lex->scan.buf;
    len = lim - off;

    switch (tok) {
      case TOKEN_IDENT:
        if ((len == 5 && strncasecmp_c (str+off, (uint8_t *)"false", 5) == 0) ||
            (len == 2 && strncasecmp_c (str+off, (uint8_t *)"no",    2) == 0) ||
            (len == 3 && strncasecmp_c (str+off, (uint8_t *)"off",   3) == 0))
        {
          lex->term.tok = TOKEN_BOOL;
          value_set_bool (&lex->term.val, false);
        } else if ((len == 4 && strncasecmp_c (str+off, (uint8_t *)"true", 4) == 0) ||
                   (len == 3 && strncasecmp_c (str+off, (uint8_t *)"yes",  3) == 0) ||
                   (len == 2 && strncasecmp_c (str+off, (uint8_t *)"on",   2) == 0))
        {
          lex->term.tok = TOKEN_BOOL;
          value_set_bool (&lex->term.val, true);
        } else {
          err = value_set_str (&lex->term.val, str, len);
          if (err == 0) {
            if (lexer_ident_as_str (lex)) {
              lex->term.tok = TOKEN_STR;
            } else {
              lex->term.tok = TOKEN_IDENT;
            }
          }
        }
        break;
      case TOKEN_CHAR:
        value_set_char (&lex->term.val, chr);
        break;
      case TOKEN_STR:
        if (lex->squot != '\0') {
          err = lexer_unesc_str_squot (&ptr, str, len, lex->squot);
        } else {
          assert (lex->dquot != '\0');
          err = lexer_unesc_str_dquot (&ptr, str, len, lex->dquot);
        }

        if (err == 0) {
          assert (ptr != NULL);
          err = value_set_str (&lex->term.val, str, 0);
          free (ptr);
          ptr = NULL;
        }
        break;
      case TOKEN_INT:
        if (lexer_int_as_float (lex)) {
          tok = TOKEN_FLOAT;
          value_set_double (&lex->term.val, (double)lng);
        } else {
          value_set_long (&lex->term.val, lng);
        }
        break;
      case TOKEN_FLOAT:
        value_set_double (&lex->term.val, dbl);
        break;
      default:
        break;
    }

    if (err == 0) {
      *a_tok = tok;
      lex->term.pos = off;
      lex->term.lns = lns;
      lex->term.cols = cols;
      lex->term.tok = tok;
    }
  } else {
    (void)lexer_set_state (lex, off);
  }

  return err;
}

void
lexer_unget_token (lexer_t *lex)
{
  assert (lex && lex->term.tok != TOKEN_NONE);
  /* reset position, not state */
  lex->pos = lex->term.pos;
  lex->lns = lex->term.lns;
  lex->cols = lex->term.cols;
}

int
lexer_get_line (size_t *lns, lexer_t *lex)
{
  assert (lns != NULL);
  assert (lexer_is_sane (lex) == 0);

  *lns = lex->term.lns;
  return 0;
}

int
lexer_get_column (size_t *cols, lexer_t *lex)
{
  assert (cols != NULL);
  assert (lexer_is_sane (lex) == 0);

  *cols = lex->term.cols;
  return 0;
}

int
lexer_get_value (value_t *val, lexer_t *lex)
{
  assert (val != NULL);
  assert (lexer_is_sane (lex) == 0);

  return value_copy (val, &lex->term.val);
}

#undef LEXER_CONTEXT_APPLY
#undef LEXER_EXPORT_MACROS
