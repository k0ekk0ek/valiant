/* LICENSE */

/* system includes */
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/* valiant includes */
#include <valiant/lexer.h>
#include <valiant/string.h>
#include <valiant/value.h>

// http://en.wikipedia.org/wiki/Unicode_character_property#General_Category

/* generate struct members */
#define X_STR(name)  string_t name;
#define X_CHAR(name) ucs4_t name;
#define X_BOOL(name) bool name;
#define X(type,name) LEXER_X_ ## type (name)
typedef struct {
  LEXER_CONTEXT /* expand */
} lexer_context_t;
#undef X
#undef X_BOOL
#undef X_CHAR
#undef X_STR

typedef struct lexer_term {
  uint32_t pos; /* scanner state */
  size_t lns; /* line number */
  size_t cols; /* column */
  token_t token; // tok
  value_t value; // val
} lexer_term_t;

struct lexer {
  string_t scan; /* scanner */
  uint32_t pos; /* scanner state */
  size_t lns; /* line number */
  size_t cols; /* column */
  int eof; /* end of file */
  ucs4_t dquot; /* double quote character */
  ucs4_t squot; /* single quote character */
  lexer_context_t ctx; /* options */
  lexer_term_t term; /* current token/value in sequence */


  //token_metadata_t user_tok;
  // these are combined into a single variable that we can simply set by
  // by invoking set_token
  // providing a value is undesirable because we must also support settings
  // strings or partial stuff and we can't provide getting line numbers etc
  // so... implement that!
};

static int lexer_get_char (ucs4_t *, lexer_t *)
  __attribute__ ((nonnull(2)));
static int lexer_get_next_char (ucs4_t *, lexer_t *)
  __attribute__ ((nonnull(2)));
static int lexer_get_previous_char (ucs4_t *, lexer_t *)
  __attribute__ ((nonnull(2)));
static int lexer_tell (size_t *, lexer_t *)
  __attribute__ ((nonnull));
static int lexer_move (lexer_t *, size_t)
  __attribute__ ((nonnull(1)));
static int lexer_unesc_str_dquot (uint8_t **, const uint8_t *, size_t, int)
  __attribute__ ((nonnull (1,2)));
static int lexer_unesc_str_squot (uint8_t **, const uint8_t *, size_t, int)
  __attribute__ ((nonnull (1,2)));
static bool lexer_is_skip_char (lexer_t *, ucs4_t)
  __attribute__ ((nonnull (1)));
static bool lexer_is_ident_first (lexer_t *, ucs4_t)
  __attribute__ ((nonnull (1)));
static bool lexer_is_ident (lexer_t *, ucs4_t)
  __attribute__ ((nonnull (1)));
static bool lexer_is_num_first (lexer_t *, ucs4_t)
  __attribute__ ((nonnull (1)));
static int lexer_eval_token (
  lexer_t *lex, int32_t off, int32_t lim, size_t lns, size_t cols, token_t tok)
  __attribute__ ((nonnull(1)));
static int lexer_get_next_token_numeric (token_t *, lexer_t *)
  __attribute__ ((nonnull));

int
lexer_create (lexer_t **a_lex, uint8_t *str, size_t len)
{
  int err;
  lexer_t *lex;

  assert (a_lex != NULL);
  assert (str != NULL);

  if ((lex = calloc (1, sizeof (lexer_t))) == NULL) {
    err = errno;
  } else {
    err = 0;
    scanner_prepare (&lex->iter, str, len);
    *a_lex = lex;

    /* defaults */
#define X(type,name,default) lex->ctx.name = default;
LEXER_CONTEXT_EXPAND();
#undef X
  }

  return err;
}

void
lexer_destroy (lexer_t *lex)
{
  assert (lex != NULL);

  memset (lex, 0, sizeof (lexer_t));
  free (lex);
}

/* generate context accessor functions */
#define X_STR(name)                                                 \
int                                                                 \
lexer_get_ ## name (uint8_t **a_str, lexer_t *lex)                  \
{                                                                   \
  int err = 0;                                                      \
  uint8_t *str = NULL;                                              \
                                                                    \
  assert (a_str != NULL);                                           \
  assert (lex != NULL);                                             \
                                                                    \
  if ((err = string_duplicate (&str, NULL, &lex->ctx.name)) == 0) { \
    *a_str = str;                                                   \
  }                                                                 \
                                                                    \
  return err;                                                       \
}                                                                   \
int                                                                 \
lexer_set_ ## name (lexer_t *lex, const uint8_t *str, size_t len)   \
{                                                                   \
  int err = 0;                                                      \
                                                                    \
  assert (lex != NULL);                                             \
                                                                    \
  err = string_assign (&lex->ctx.name, str, len);                   \
                                                                    \
  return err;                                                       \
}

#define X_CHAR(name)                                                \
ucs4_t                                                              \
lexer_get_ ## name (lexer_t *lex)                                   \
{                                                                   \
  assert (lex != NULL);                                             \
  return lex->ctx.name;                                             \
}                                                                   \
void                                                                \
lexer_set_ ## name (lexer_t *lex, ucs4_t chr)                       \
{                                                                   \
  assert (lex != NULL);                                             \
  lex->ctx.name = chr;                                              \
}

#define X_BOOL(name)                                                \
bool                                                                \
lexer_get_ ## name (lexer_t *lex)                                   \
{                                                                   \
  assert (lex != NULL);                                             \
  *a_bln = lex->ctx.name;                                           \
  return 0;                                                         \
}                                                                   \
void                                                                \
lexer_set_ ## name (lexer_t *lex, bool bln)                         \
{                                                                   \
  assert (lex != NULL);                                             \
  lex->ctx.name = bln;                                              \
  return 0;                                                         \
}

#define X(type,name,default) X_ ## type (name)

LEXER_CONTEXT_EXPAND();

#undef X
#undef X_BOOL
#undef X_CHAR
#undef X_STR


static int
lexer_get_char (ucs4_t *a_chr, lexer_t *lex)
{
  assert (chr != NULL);
  assert (lex != NULL);

	return scanner_get_char (a_chr, lex);
}

static int
lexer_get_next_char (ucs4_t *a_chr, lexer_t *lex)
{
  int err = 0;
  ucs4_t chr = '\0';

  assert (lex != NULL);

  err = scanner_get_char (&chr, lex);
  if (err == 0) {
    err = scanner_get_next_char (a_chr, lex);
    if (err == 0) {
      if (a_chr != NULL) {
        *a_chr = chr;
      }

      if (chr == '\n') {
        lex->lns++;
        lex->cols = 1;
      } else {
        lex->cols++;
      }
	}

	return err;
}

static int
lexer_get_previous_char (ucs4_t *a_chr, lexer_t *lex)
{
  int err = 0;
  size_t pos = 0;
  ucs4_t chr = '\0';

  assert (lex != NULL);

  err = scanner_get_previous_char (chr, &lex->scan);
  if (err == 0) {
    if (a_chr != NULL) {
      *a_chr = chr;
    }

    if (chr == '\n') {
      assert (lex->lns > 1);
      lex->lns--;
      lex->cols = 1;

      /* save position, count columns to previous newline */
      err = scanner_tell (&pos, &lex->scan);
      if (err == 0) {
        for (err = scanner_get_previous_char (&chr, &lex->scan);
             err == 0 && chr != '\n' && chr != '\0';
             err = scanner_get_previous_char (&chr, &lex->scan))
        {
          /* do nothing */
        }

        if (err == 0) {
          scanner_move (&lex->scan, pos);
        }
      }
    } else if (chr != '\0') {
      assert (lex->cols > 1);
      lex->cols--;
    }
  }

  return err;
}

static int
lexer_tell (size_t *off, lexer_t *lex)
{
  assert (off != NULL);
  assert (lex != NULL);

  return scanner_tell (off, &lex->scan);
}

static int
lexer_move (lexer_t *lex, size_t off)
{
  size_t pos;
  int(*func)(ucs4_t *, scanner_t *);

  assert (lex != NULL);
  assert (off != NULL);

  err = scanner_tell (&pos, &lex->scan);
  if (err == 0) {
    if (pos < off) {
      func = &lexer_get_next_char;
    } else {
      func = &lexer_get_previous_char;
    }
    for (; err == 0 && pos != off; ) {
      err = func (NULL, lex);
      if (!err) {
        err = scanner_tell (&pos, &lex->scan);
      }
    }
    assert (pos == off);
  }

  return err;
}

static bool
lexer_is_skip_char (lexer_t *lex, ucs4_t chr)
{
  bool ret = false;

  assert (lex != NULL);

  if (string_is_empty (&lex->ctx.skip_chars) == false) {
    if (uc_is_property (chr, UC_PROPERTY_WHITE_SPACE)) {
      ret = true;
    }
  } else {
    if (string_search_char (&lex->ctx.skip_chars, chr) != NULL) {
      ret = true;
    }
  }

  return ret;
}

static bool
lexer_is_ident_first (lexer_t *lex, ucs4_t chr)
{
  int ret = false;

  assert (lex != NULL);

  if (string_is_empty (&lex->ctx.ident_first) == false) {
    /* UC_PROPERTY_ID_START is used to distinguish initial letters from
       continuation characters */
    /* Source: http://icu-project.org/apiref/icu4c/uchar_8h.html */
    if (uc_is_property (chr, UC_PROPERTY_ID_START)) {
      if (string_is_empty (&lex->ctx.ident_first) == true ||
          string_search_char (&lex->ctx.ident_first, chr) != NULL)
      {
        ret = true;
      }
    }
  } else {
    if (string_search_char (&lex->ctx.not_ident_first, chr) != NULL) {
      ret = true;
    }
  }

  return ret;
}

static bool
lexer_is_ident (lexer_t *lex, ucs4_t chr)
{
  bool ret = false;

  assert (lex != NULL);

  if (string_is_empty (&lex->ctx.ident) == false) {
    if (uc_is_property (chr, UC_PROPERTY_ID_CONTINUE)) {
      ret = true;
    }
  } else {
    if (string_search_char (&lex->ctx.ident, len, chr) != NULL) {
      ret = true;
    }
  }

  return ret;
}

static bool
lexer_is_num_first (lexer_t *lex, ucs4_t chr)
{
  bool ret = false;

  assert (lex != NULL);

  if (chr == '.' && lexer_scan_float (lex)) {
    ret = true;
  } else if (chr == '-' || chr == '+') {
    ret = true;
  } else if (ascii_isdigit (chr)) {
    ret = true;
  }

  return ret;
}

#define lexer_is_str_squot(p,c)       ((p)->ctx.str_squot == (c))
#define lexer_is_str_dquot(p,c)       ((p)->ctx.str_dquot == (c))
#define lexer_scan_comment_single(p)  ((p)->ctx.scan_comment_single == true)
#define lexer_scan_comment_multi(p)   ((p)->ctx.scan_comment_multi == true)
#define lexer_skip_comment_single(p)  ((p)->ctx.skip_comment_single == true)
#define lexer_skip_comment_multi(p)   ((p)->ctx.skip_comment_multi == true)
#define lexer_scan_str_squot(p)       ((p)->ctx.scan_str_squot == true)
#define lexer_scan_str_dquot(p)       ((p)->ctx.scan_str_dquot == true)
#define lexer_skip_str_squot(p)       ((p)->ctx.skip_str_squot == true)
#define lexer_skip_str_dquot(p)       ((p)->ctx.skip_str_dquot == true)
#define lexer_scan_bool(p)            ((p)->ctx.scan_bool == true)
#define lexer_scan_oct(p)             ((p)->ctx.scan_oct == true)
#define lexer_scan_int(p)             ((p)->ctx.scan_int == true)
#define lexer_scan_hex(p)             ((p)->ctx.scan_hex == true)
#define lexer_scan_float(p)           ((p)->ctx.scan_float == true)
#define lexer_int_as_float(p)         ((p)->ctx.int_as_float == true)
#define lexer_ident_as_str(p)         ((p)->ctx.ident_as_str == true)

typedef struct {
  string_t str;
  ucs4_t quot;
  int esc;
} lexer_unesc_str_quot_context_t;

static int
lexer_unesc_str_dquot_length (ucs4_t chr, int len, void *data)
{
  int err = 0;
  lexer_unesc_str_quot_context_t *ctx;

  assert (data != NULL);
  ctx = (lexer_unesc_str_quot_context_t *)data;

  if (ctx->esc == 1) {
    ctx->esc = 0;
    err = string_raise_limit (&ctx->str, (size_t)len);
  } else {
    if (chr == '\0' || chr == ctx->quot) {
      err = stract_stop;
    } else if (chr == '\\') {
      ctx->esc = 1;
    } else {
      err = string_raise_limit (&ctx->str, (size_t)len);
    }
  }

  return err;
}

static int
lexer_unesc_str_dquot_copy (ucs4_t chr, int len, void *data)
{
  int err = 0;
  lexer_unesc_str_quot_context_t *ctx;

  assert (data != NULL);
  ctx = (lexer_unesc_str_quot_context_t *)data;

  if (ptr->esc == 1) {
    switch (str[pos]) {
      case 'a':
        err = string_suffix_char (&ctx->str, '\a');
        break;
      case 'b':
        err = string_suffix_char (&ctx->str, '\b');
        break;
      case 'f':
        err = string_suffix_char (&ctx->str, '\f');
        break;
      case 'n':
        err = string_suffix_char (&ctx->str, '\n');
        break;
      case 'r':
        err = string_suffix_char (&ctx->str, '\r');
        break;
      case 't':
        err = string_suffix_char (&ctx->str, '\t');
        break;
      case 'v':
        err = string_suffix_char (&ctx->str, '\v');
        break;
      default:
        err = string_suffix_char (&ctx->str, chr);
        break;
    }
  } else {
    if (chr == '\0' || chr == ctx->quot) {
      err = -1;
    } else if (chr == '\\') {
      ctx->esc = 1;
    } else {
      err = string_append_char (&ctx->str, chr);
    }
  }

  return err;
}

static int
lexer_unesc_str_squot_length (ucs4_t chr, int len, void *data)
{
  int err = 0;
  lexer_unesc_str_quot_context_t *ctx;

  assert (data != NULL);
  ctx = (lexer_unesc_str_quot_context_t *)data;

  if (ctx->esc == 1) {
    err = string_raise_limit (&ctx->str, (size_t)len);
    if (err == 0 && chr != ctx->quot) {
      err = string_raise_limit (&ctx->str, 1);
    }
  } else {
    if (chr == '\0' || chr == ctx->quot) {
      err = -1;
    } else if (chr == '\\') {
      ctx->esc = 1;
    } else {
      err = string_raise_limit (&ctx->str, (size_t)len);
    }
  }

  return err;
}

static int
lexer_unesc_str_squot_copy (ucs4_t chr, int len, void *data)
{
  int err = 0;
  lexer_unesc_str_context_t *ctx;

  assert (data != NULL);
  ctx = (lexer_unesc_str_context_t *)data;

  if (ctx->esc == 1) {
    if (chr != ctx->quot) {
      err = string_suffix_char (&ctx->str, '\\');
    }
    if (err == 0) {
      err = string_suffix_char (&ctx->str, chr);
    }
  } else {
    if (chr == '\0' && chr == ctx->quot) {
      err = -1;
    } else if (chr == '\\') {
      ctx->esc = 1;
    } else {
      err = string_suffix_char (&ctx->str, chr);
    }
  }

  return err;
}

static int
lexer_unesc_str_quot (
  uint8_t **a_str,
  const uint8_t *str,
  size_t len,
  ucs4_t quot,
  stract_t unesc_str_quot_length,
  stract_t unesc_str_quot_copy)
{
  int err = 0;
  size_t lim, off;
  ucs4_t chr;
  lexer_string_t ctx;

  assert (a_str != NULL);
  assert (str != NULL);
  assert (len != 0);

  memset (ctx, 0, sizeof (ctx));

  ret = u8_mbtouc (&chr, str, len);
  if (ret < 0) {
    err = EINVAL;
  } else {
    if (chr != quot) {
      off = 0;
      ctx.quot = '\0';
    } else {
      off = (size_t)ret;
      ctx.quot = chr;
    }

    (void)string_terminate (&ctx.str, STRING_TERMINATE_ALWAYS);
    (void)string_limit (&ctx.str, 1);

    err = u8_strnact (str+off, len-off, unesc_str_quot_length, &ctx);
    if (err == 0) {
      (void)string_is_limited (&lim, &ctx.str);
      err = string_expand (&ctx->str, lim);
      if (err == 0) {
        err = u8_strnact (str+off, len-off, unesc_str_quot_copy, &ctx);
      }
    }
  }

  if (err == 0) {
    *a_str = string_pointer (&ctx.str);
  } else {
    string_clear (&ctx.str);
  }

  return err;
}

/**
 * \brief Examine token and content, convert values and assign to lexer
 *        structure.
 */
static int
lexer_eval_token (
  lexer_t *lex, int32_t off, int32_t lim, size_t lns, size_t cols, token_t tok)
{
  int err = 0;
//  char *str = NULL;
//  double dbl;
//  long lng;
  int32_t len = lim - off;
  void *str = lex->iter.context; // + offset
  uint8_t *dup;
  char *ptr = NULL;
  u8_stract_t copy, length;
  ucs4_t quot;


  assert (lex != NULL);
  assert (off < lim);
  /* integer and floating-point values are handled separately */
  assert (tok != TOKEN_INT && tok != TOKEN_FLOAT);

  switch (tok) {
    case TOKEN_BOOL:
      /* FIXME: comment on why this is allowed */
      str = (void *)lex->iter.context;

      if ((len == 5 && ascii_strncasecmp (str+off, "false", 5) == 0) ||
          (len == 2 && ascii_strncasecmp (str+off, "no",    2) == 0) ||
          (len == 3 && ascii_strncasecmp (str+off, "off",   3) == 0))
      {
        lex->term.tok = TOKEN_BOOL;
        value_set_bool (&lex->term.val, false);
      } else if ((len == 4 && ascii_strncasecmp (str+off, "true", 4) == 0) ||
                 (len == 3 && ascii_strncasecmp (str+off, "yes",  3) == 0) ||
                 (len == 2 && ascii_strncasecmp (str+off, "on",   2) == 0))
      {
        lex->term.tok = TOKEN_BOOL;
        value_set_bool (&lex->term.val, true);
      }
      break;
    case TOKEN_CHAR:
      assert (len >= 1 && len <= 4);
      value_set_char (&lex->term.val, str+off);
      break;
    case TOKEN_IDENT:
      err = value_set_strlen (&lex->term.val, str, len);
      if (err == 0) {
        if (lexer_ident_as_str (lex)) {
          lex->term.tok = TOKEN_STR;
        } else {
          lex->term.tok = TOKEN_IDENT;
        }
      }
      break;
    case TOKEN_STR:
      assert (lex->dquot != '\0' || lex->squot != '\0');
      if (lex->dquot != '\0') {
        quot = lex->dquot;
        copy = &lexer_unesc_str_dquot_copy;
        length = &lexer_unesc_str_dquot_length;
      } else {
        quot = lex->squot;
        copy = &lexer_unesc_str_squot_copy;
        length = &lexer_unesc_str_squot_length;
      }

      err = lexer_unesc_str_quot (&ptr, str, len, quot, copy, length);
      if (err == 0) {
        err = value_set_str (&lex->term.val, str);
        if (err == 0) {
          lex->term.tok = TOKEN_STR;
        }
      }
      break;
  }

  if (!err) {
    lex->term.pos = pos;
    lex->term.lns = lns;
    lex->term.cols = cols;
  }

  return err;
}

#define LEXER_FLAG_NONE       (0)
#define LEXER_FLAG_ESCAPE     (1<<0)

int
lexer_get_next_token_numeric (token_t *a_tok, lexer_t *lex)
{
  int err = 0;
  int err_d = 0;
  int err_l = 0;
  int ret;
  size_t cols, lim, lns, off, pos;
  ucs4_t chr;
  uint8_t *str;
  uint8_t *end = NULL;
  uint8_t *end_d = NULL;
  uint8_t *end_l = NULL;

  assert (a_tok != NULL);
  assert (lex != NULL);

  pos = lex->scan.pos;
  lns = lex->lns;
  cols = lex->cols;

  /* register current position in case algorithm needs to revert */
  err = lexer_get_current_char (&chr, lex->scan);

  /* optional plus or minus sign */
  if (!err && (chr == '-' || chr == '+')) {
    err = lexer_get_next_char (&chr, lex->scan);
  }

  /* scan for octal and hexadecimal values and short circuit if the lexical
     analyzer should not scan for them */
  if (!err && (chr == '0')) {
    err = lexer_get_next_char (&chr, lex);
    if (!err) {
      /* hexadecimal value */
      if (chr == 'x' || chr == 'X') {
        if (!lexer_scan_hex (lex)) {
          (void)lexer_get_previous_char (&chr, lex->scan);
          tok = TOKEN_INT; 
        }
      /* octal value */
      } else if (!lexer_scan_oct (lex)) {
        tok = TOKEN_INT;
      /* end of file */
      } else if (chr == '\0') {
        tok = TOKEN_INT;
      }

      if (tok != TOKEN_NONE) {
        if (lexer_scan_int (lex)) {
          if (lexer_num_as_float (lex)) {
            tok = TOKEN_FLOAT;
          } else {
            tok = TOKEN_INT;
          }
        } else if (lexer_scan_float (lex)) {
          tok = TOKEN_FLOAT;
        } else {
          tok = TOKEN_NONE;
        }

        if (tok != TOKEN_NONE) {
          *a_tok = tok;
          /* fill term */
          lex->term.tok = tok;
          lex->term.pos = pos;
          lex->term.lns = lns;
          lex->term.cols = cols;
          value_set_int (&lex->term.value, 0);
        } else {
          (void)lexer_move (lex, pos);
        }

        return err;
      }
    }
  }

  if (!err) {
    str = lex->scan.str.buf + pos;

    if (lexer_scan_int (lex)) {
      err_l = ascii_strtol (&lng, str, &end_l, 0 /* auto detect */);
    }
    if (lexer_scan_float (lex)) {
      err_d = ascii_strtod (&dbl, str, &end_d);
    }

    if (end_l < end_d) {
      end = end_d;
      err = err_d;
      tok = TOKEN_FLOAT;
    } else {
      /* user might not want to scan for floating point numbers or no
         conversion could be performed. the latter, however strange it may
         seem, is not an error in our situation. it simply means that what
         seemed to be a numeric value, is actually not */
      if (end_l == str) {
        tok = TOKEN_NONE;
      } else {
        end = end_l;
        err = err_l;
        if (lexer_int_as_float (lex)) {
          dbl = (double)lng;
          tok = TOKEN_FLOAT;
        } else {
          tok = TOKEN_INT;
        }
      }
    }
  }

  if (!err) {
    assert (end != NULL && end > str);

    lex->term.tok = tok;
    lex->term.pos = pos;
    lex->term.lns = lns;
    lex->term.cols = cols;
    if (tok == TOKEN_INT) {
      (void)value_set_long (&lex->term.val, lng);
    } else {
      (void)value_set_double (&lex->term.val, dbl);
    }

    err = lexer_tell (&pos, lex);
    if (err == 0) {
      assert ((size_t)(end - str) < (SIZE_MAX - pos));
      pos += (size_t)(end - str);
      err = lexer_move (lex, pos);
    }
  } else {
    err = lexer_move (lex, pos);
  }

  return err;
}

int
lexer_get_next_token (token_t *ptr, lexer_t *lex)
{
  int err = 0;
  int num;
  ucs4_t chr;
  unsigned int flags = LEXER_FLAG_NONE;
  ssize_t cnt = 1;
  size_t lim = 0;
  size_t off = 0;
  token_t tok = TOKEN_NONE;

  assert (ptr != NULL);
  assert (lex != NULL);

  /* operation is stateless, except for opening and closing quotes, but that
     is of no concern to the user */
  if (lex->term.tok == TOKEN_STR_DQOUT ||
      lex->term.tok == TOKEN_STR_SQUOT)
  {
    assert (value_get_type (&lex->term.value) == VALUE_CHAR);
    if (lex->dquot || lex->squot) {
      tok = TOKEN_STR;
    }
  }

  for (cnt = 1; err == 0 && cnt != 0; ) {
    if (cnt == 1) {
      // FIXME: instead of directly using the scanner we must use lexer_get_current_char
      //        etc... it will keep score of newlines and columns for us... so we don't
      //        have to!
      err = lexer_get_char (&chr, &lex->scan);

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
              err = lexer_get_char (&chr, &lex->scan);
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

            if (flags & LEXER_FLAG_ESCAPE) {
              flags &= ^LEXER_FLAG_ESCAPE;
            } else if (chr == '\\') {
              flags |=  LEXER_FLAG_ESCAPE;
            } else if (chr == lex->dquot) {
              if (lexer_skip_dquot (lex)) {
                cnt = 0;
              } else {
                cnt = -1;
              }
            } else if (chr == lex->squot) {
              if (lexer_skip_squot (lex)) {
                cnt = 0;
              } else {
                cnt = -1;
              }
            }
            break;
          case TOKEN_STR_DQUOT:
            assert (lex->squot == '\0');

            if (lexer_is_str_dquot (lex,chr)) {
              if (lexer_skip_str_dquot (lex)) {
                tok = TOKEN_NONE;
              } else {
                cnt = 0;
              }
            }
            break;
          case TOKEN_STR_SQUOT:
            assert (lex->dquot == '\0');

            if (lexer_is_str_squot (lex,chr)) {
              if (lexer_skip_str_squot (lex)) {
                tok = TOKEN_NONE;
              } else {
                cnt = 0;
              }
            }
            break;

          default: /* TOKEN_NONE */
            flags = LEXER_FLAG_NONE;

            /* double quoted string */
            if (lexer_is_str_dquot (lex,chr)) {
              assert (lex->squot == '\0');

              if (lex->dquot) {
                if (lexer_skip_str_dquot (lex)) {
                  tok = TOKEN_NONE;
                } else {
                  tok = TOKEN_STR_DQUOT;
                  cnt = 0;
                }
                lex->dquot = '\0';
              } else {
                if (lexer_skip_str_dquot (lex)) {
                  tok = TOKEN_STR;
                } else {
                  tok = TOKEN_STR_DQUOT;
                  cnt = 0;
                }
                lex->dquot = chr;
              }

            /* single quoted string */
            } else if (lexer_is_str_squot (lex,chr)) {
              assert (lex->dquot == '\0');

              if (lex->squot) {
                if (lexer_skip_str_squot (lex)) {
                  tok = TOKEN_NONE;
                } else {
                  tok = TOKEN_STR_SQUOT;
                  cnt = 0;
                }
                lex->squot = '\0';
              } else {
                if (lexer_skip_str_squot (lex)) {
                  tok = TOKEN_STR;
                } else {
                  tok = TOKEN_STR_SQUOT;
                  cnt = 0;
                }
                lex->squot = chr;
              }

            /* integer or floating-point number */
            } else if (lexer_is_num_first (lex, chr)) {
              err = lexer_get_next_token_numeric (&tok, lex);
              if (!err && tok != TOKEN_NONE) {
                cnt = 0;
              }

            /* comment line */
            } else if (chr == '#' && lexer_scan_comment_single (lex)) {
              tok = TOKEN_COMMENT_SINGLE;

            /* comment or comment line */
            } else if (chr == '/') {
              chr = uiter_next32 (&lex->iter);
              /* comment line */
              if (chr == '/' && lexer_scan_comment_single (lex)) {
                tok = TOKEN_COMMENT_SINGLE;
                cnt = 2;
              /* comment */
              } else if (chr == '*' && lexer_scan_comment_multi (lex)) {
                tok = TOKEN_COMMENT_MULTI;
                cnt = 2;
              }
              err = scanner_previous_char (&chr, &lex->scan);
            }

            /* char */
            if (!err && tok == TOKEN_NONE) {
              if (lexer_is_ident_first (lex,chr)) {
                tok = TOKEN_IDENT;
              } else if (!lexer_is_skip_char (lex,chr)) {
                tok = TOKEN_CHAR;
                cnt = 0;
              }
            }

            /* register offset */
            err = scanner_tell (&off, &lex->scan);
            break;
        }
      }
    } else if (cnt > 1) {
      cnt--;
      err = scanner_next_char (NULL, &lex->scan);
    } else if (cnt < 1) {
      cnt++;
      err = scanner_previous_char (NULL, &lex->scan);
    }
  }

  /* register limit */
  err = scanner_tell (&lim, &lex->scan);

  // now evaluate the token and everything that's important in
  // lexer_eval_token
}

#undef LEXER_FLAG_NONE
#undef LEXER_FLAG_ESCAPE

// lexer_unget_token
void
lexer_unshift_token (lexer_t *lex)
{
  assert (lex && lex->term.token != TOKEN_NONE);
  /* reset position, not state */
  lex->offset = lex->term.offset;
  lex->line = lex->term.line;
  lex->column = lex->term.column;
}

#undef _LEXER_OPTS_
#undef _LEXER_OPTS_EXPORT_

