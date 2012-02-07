/* system includes */
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "vt_error.h"
#include "vt_lexer.h"

/* prototypes */
int vt_lexer_def_in_class (int, const char *, const char *, const char *);
int vt_lexer_def_is_true (int, int, int);

void
vt_lexer_def_reset (vt_lexer_def_t *def)
{
  assert (def);

  def->space = VT_CHRS_SPACE;
  def->str_first = VT_CHRS_STR_FIRST;
  def->str = VT_CHRS_STR;
  def->dquot = VT_CHRS_DQUOT;
  def->squot = VT_CHRS_SQUOT;

  def->skip_comment_multi = VT_SKIP_COMMENT_MULTI;
  def->skip_comment_single = VT_SKIP_COMMENT_SINGLE;
  def->scan_bool = VT_SCAN_BOOL;
  def->scan_binary = VT_SCAN_BINARY;
  def->scan_octal = VT_SCAN_OCTAL;
  def->scan_float = VT_SCAN_FLOAT;
  def->scan_hex = VT_SCAN_HEX;
  def->scan_hex_dollar = VT_SCAN_HEX_DOLLAR;
  def->scan_str_dquot = VT_SCAN_STR_DQUOT;
  def->scan_str_squot = VT_SCAN_STR_SQUOT;
  def->numbers_as_int = VT_NUMBERS_AS_INT;
  def->int_as_float = VT_INT_AS_FLOAT;
  def->char_as_token = VT_CHAR_AS_TOKEN;
}

vt_token_t
vt_lexer_value_clr (vt_lexer_t *lxr)
{
  assert (lxr);

  if (lxr->value.type == VT_VALUE_TYPE_STRING &&
      lxr->value.data.str)
    free (lxr->value.data.str);

  lxr->token = VT_TOKEN_NONE;
  lxr->line = lxr->lines;
  lxr->column = lxr->columns;
  lxr->value.type = VT_VALUE_TYPE_CHAR;
  lxr->value.data.chr = '\0';
  return lxr->token;
}

vt_token_t
vt_lexer_get_token (vt_lexer_t *lxr)
{
  assert (lxr);
  return lxr->token;
}

vt_token_t
vt_lexer_set_token (vt_lexer_t *lxr,
                    vt_token_t token,
                    size_t line,
                    size_t column)
{
  assert (lxr);
  lxr->token = token;
  lxr->line = line;
  lxr->column = column;
  return token;
}

vt_token_t
vt_lexer_set_token_bool (vt_lexer_t *lxr,
                         vt_token_t token,
                         size_t line,
                         size_t column,
                         bool bln)
{
  assert (lxr);
  lxr->value.type = VT_VALUE_TYPE_BOOL;
  lxr->value.data.bln = bln;
  return vt_lexer_set_token (lxr, token, line, column);
}

vt_token_t
vt_lexer_set_token_char (vt_lexer_t *lxr,
                         vt_token_t token,
                         size_t line,
                         size_t column,
                         char chr)
{
  assert (lxr);
  lxr->value.type = VT_VALUE_TYPE_CHAR;
  lxr->value.data.chr = chr;
  return vt_lexer_set_token (lxr, token, line, column);
}

vt_token_t
vt_lexer_set_token_int (vt_lexer_t *lxr,
                        vt_token_t token,
                        size_t line,
                        size_t column,
                        long lng)
{
  assert (lxr);
  lxr->value.type = VT_VALUE_TYPE_INT;
  lxr->value.data.lng = lng;
  return vt_lexer_set_token (lxr, token, line, column);
}

vt_token_t
vt_lexer_set_token_float (vt_lexer_t *lxr,
                          vt_token_t token,
                          size_t line,
                          size_t column,
                          double dbl)
{
  assert (lxr);
  lxr->value.type = VT_VALUE_TYPE_FLOAT;
  lxr->value.data.dbl = dbl;
  return vt_lexer_set_token (lxr, token, line, column);
}

vt_token_t
vt_lexer_set_token_string (vt_lexer_t *lxr,
                           vt_token_t token,
                           size_t line,
                           size_t column,
                           char *str)
{
  assert (lxr);
  lxr->value.type = VT_VALUE_TYPE_STRING;
  lxr->value.data.str = str;
  return vt_lexer_set_token (lxr, token, line, column);
}

char *
vt_lexer_unesc_str_dquot (const char *str, size_t len, int dquot, vt_error_t *err)
{
  int esc, quot;
  int first, i, j;
  char *nstr;
  size_t nlen;

  assert (str);

  esc = 0;
  nlen = 1;

  if (str[0] == dquot) {
    first = 1;
    quot = dquot;
  } else {
    first = 0;
    quot = '\0';
  }

  for (i = first; i < len && str[i]; i++) {
    if (esc) {
      esc = 0;
      nlen++;
    } else if (str[i] == '\\') {
      esc = 1;
    } else if (str[i] == quot) {
      break;
    } else {
      nlen++;
    }
  }

  if (! (nstr = calloc (len, sizeof (char)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return NULL;
  }

  esc = 0;

  for (i = first, j = 0; i < len && str[i]; i++) {
    if (esc) {
      esc = 0;
      if (str[i] == 'a')
        nstr[j++] = '\a';
      else if (str[i] == 'b')
        nstr[j++] = '\b';
      else if (str[i] == 'f')
        nstr[j++] = '\f';
      else if (str[i] == 'n')
        nstr[j++] = '\n';
      else if (str[i] == 'r')
        nstr[j++] = '\r';
      else if (str[i] == 't')
        nstr[j++] = '\t';
      else if (str[i] == 'v')
        nstr[j++] = '\v';
      else
        nstr[j++] = str[i];
    } else if (str[i] == '\\') {
      esc = 1;
    } else if (str[i] == quot) {
      break;
    } else {
      nstr[j++] = str[i];
    }
  }

  nstr[j] = '\0'; /* null terminate */

  return nstr;
}

char *
vt_lexer_unesc_str_squot (const char *str, size_t len, int squot, vt_error_t *err)
{
  int esc, quot;
  int first, i, j;
  char *nstr;
  size_t nlen;

  assert (str);

  esc = 0;
  nlen = 1;

  if (str[0] == squot) {
    first = 1;
    quot = squot;
  } else {
    first = 0;
    quot = '\0';
  }

  for (i = first; i < len && str[i]; i++) {
    if (esc) {
      esc = 0;
      if (str[i] != squot)
        nlen++;
      nlen++;
    } else if (str[i] == '\\') {
      esc = 1;
    } else if (str[i] == quot) {
      break;
    } else {
      nlen++;
    }
  }

  if (! (nstr = calloc (len, sizeof (char)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return NULL;
  }

  esc = 0;

  for (i = first, j = 0; i < len && str[i]; i++) {
    if (esc) {
      esc = 0;
      if (str[i] != squot)
        nstr[j++] = '\\';
      nstr[j++] = str[i];
    } else if (str[i] == '\\') {
      esc = 1;
    } else if (str[i] == quot) {
      break;
    } else {
      nstr[j++] = str[i];
    }
  }

  nstr[j] = '\0'; /* null terminate */

  return nstr;
}

size_t
vt_lexer_get_pos (vt_lexer_t *lxr)
{
  assert (lxr);
  return (lxr->pos > 0) ? lxr->pos - 1 : 0;
}

size_t
vt_lexer_get_cur_column (vt_lexer_t *lxr)
{
  assert (lxr);
  return (lxr->columns > 0) ? lxr->columns - 1 : 1;
}

size_t
vt_lexer_get_cur_line (vt_lexer_t *lxr)
{
  assert (lxr);
  return lxr->lines;
}

int
vt_lexer_get_chr (vt_lexer_t *lxr)
{
  int chr;

  assert (lxr);

  chr = lxr->str[lxr->pos];
  if (chr == '\n') {
    lxr->columns = 1;
    lxr->lines++;
  } else {
    lxr->columns++;
  }

  if (lxr->eof) {
    chr = '\0';
  } else if (lxr->pos < lxr->len) {
    chr = lxr->str[ lxr->pos++ ];
  } else {
    chr = '\0';
  }

  if (chr == '\0') {
    lxr->eof = 1;
  }

  return chr;
}

int
vt_lexer_peek_chr (vt_lexer_t *lxr, ssize_t pos)
{
  int chr;

  assert (lxr);

  pos--;
  if (lxr->eof || (pos < 0 && ((ssize_t)lxr->pos + pos) < 0) || ((ssize_t)lxr->pos + pos) >= lxr->len)
    chr = '\0';
  else
    chr = lxr->str[ lxr->pos + pos ];

  return chr;
}

int
vt_lexer_unget_chr (vt_lexer_t *lxr)
{
  int chr;
  size_t pos;

  assert (lxr);

  if (lxr->pos > 0) {
    chr = lxr->str[ --lxr->pos ];
    if (chr == '\n') {
      lxr->lines--;
      /* count number of columns since last newline */
      pos = lxr->pos;
      if (pos == 0) {
        lxr->columns = 1;
      } else {
        for (--pos; ; pos--) {
          if (pos == 0) {
            lxr->columns = (lxr->pos - pos) + 1;
            break;
          } else if (lxr->str[pos] == '\n') {
            lxr->columns = lxr->pos - pos;
            break;
          }
        }
      }
    } else {
      lxr->columns--;
    }
  } else {
    chr = '\0';
  }

  return chr;
}

int
vt_lexer_def_in_class (int chr, const char *chrs, const char *def_chrs,
  const char *scp_def_chrs)
{
  if (chr) {
    if (scp_def_chrs)
      return strchr (scp_def_chrs, chr) ? chr : 0;
    if (def_chrs)
      return strchr (def_chrs, chr) ? chr : 0;
    if (chrs)
      return strchr (chrs, chr) ? chr : 0;
  }
  return 0;
}

#define __vt_lexer_def_in_class(chr, chrs, def, scp_def, mbr) \
  (vt_lexer_def_in_class ((chr), (chrs), ((def) ? (def)->mbr : NULL), \
    ((scp_def) ? (scp_def)->mbr: NULL)))

#define vt_is_space(chr, def, scp_def) \
  ((chr) == '\n' || __vt_lexer_def_in_class ((chr), VT_CHRS_SPACE, (def), \
    (scp_def), space))
#define vt_is_str_first(chr, def, scp_def) \
  (__vt_lexer_def_in_class ((chr), VT_CHRS_STR_FIRST, (def), (scp_def), \
    str_first))
#define vt_is_str(chr, def, scp_def) \
  (__vt_lexer_def_in_class ((chr), VT_CHRS_STR, (def), (scp_def), str))
#define vt_is_dquot(chr, def, scp_def) \
  (__vt_lexer_def_in_class ((chr), VT_CHRS_DQUOT, (def), (scp_def), dquot))
#define vt_is_squot(chr, def, scp_def) \
  (__vt_lexer_def_in_class ((chr), VT_CHRS_SQUOT, (def), (scp_def), squot))

int
vt_lexer_def_is_true (int bln, int def_bln, int scp_def_bln)
{
  if (scp_def_bln != -1)
    return (scp_def_bln) ? 1 : 0;
  if (def_bln != -1)
    return (def_bln) ? 1 : 0;
  return glbl_bln;
}

#define __vt_lexer_def_is_true(bln, def, scp_def, mbr) \
  (vt_lexer_def_mbr_is_true ((bln), ((def) ? ((def)->mbr ? 1 : 0) : -1), \
    ((scp_def) ? ((scp_def)->mbr ? 1 : 0) : -1)))

#define vt_skip_comment_single(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SKIP_COMMENT_SINGLE, (def), (scp_def), \
    skip_comment_single))
#define vt_skip_comment_multi(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SKIP_COMMENT_MULTI, (def), (scp_def), \
    skip_comment_multi))
#define vt_scan_bool(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SCAN_BOOL, (def), (scp_def), scan_bool))
#define vt_scan_binary(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SCAN_BINARY, (def), (scp_def), scan_binary))
#define vt_scan_octal(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SCAN_OCTAL, (def), (scp_def), scan_octal))
#define vt_scan_float(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SCAN_FLOAT, (def), (scp_def), scan_float))
#define vt_scan_int(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SCAN_INT, (def), (scp_def), scan_int))
#define vt_scan_hex(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SCAN_HEX, (def), (scp_def), scan_hex))
#define vt_scan_hex_dollar(def, scp_def) \
  (__vt_lexer_def_is_true (VT_SCAN_HEX_DOLLAR, (def), (scp_def), \
    scan_hex_dollar))
#define vt_numbers_as_int(def, scp_def) \
  (__vt_lexer_def_is_true (VT_NUMBERS_AS_INT, (def), (scp_def), numbers_as_int))
#define vt_int_as_float(def, scp_def) \
  (__vt_lexer_def_is_true (VT_INT_AS_FLOAT, (def), (scp_def), int_as_float))
#define vt_char_as_token(def, scp_def) \
  (__vt_lexer_def_is_true (VT_CHAR_AS_TOKEN, (def), (scp_def), char_as_token))

vt_token_t
vt_lexer_get_next_token (vt_lexer_t *lxr, vt_lexer_def_t *scp_def, int *err)
{
  bool bln;
  char *str, *ptr;
  double dbl;
  int chr, nchr, dot, esc, quot;
  int len, pos;
  int loop;
  long lng;
  size_t column, line;
  vt_error_t tmperr;
  vt_lexer_def_t *def;
  vt_token_t token;

  assert (lxr);
  vt_lexer_value_clr (lxr);

  def = lxr->def;
  token = VT_TOKEN_NONE;

  for (loop = 1; loop > 0 && (chr = vt_lexer_get_chr (lxr)) ;) {
    if (loop == 1) {
      switch (token) {
        case VT_TOKEN_COMMENT_SINGLE:
          if (chr == '\n') {
            if (vt_skip_comment_single (def, scp_def))
              token = VT_TOKEN_NONE;
            else
              loop = 0;
          }
          break;
        case VT_TOKEN_COMMENT_MULTI:
          if (chr == '*' && (chr = vt_lexer_get_chr (lxr)) == '/') {
            if (vt_skip_comment_multi (def, scp_def))
              token = VT_TOKEN_NONE;
            else
              loop = 0;
          }
          break;
        case VT_TOKEN_STRING:
          if (quot) {
            if (esc)
              esc = 0;
            else if (chr == quot)
              loop = 0;
            else if (chr == '\\')
              esc = 1;
            break;
          } else {
            if (vt_is_space (chr, def, scp_def) || ! vt_is_str (chr, def, scp_def))
              loop = -1;
          }
          break;
        case VT_TOKEN_BINARY:
        case VT_TOKEN_OCTAL:
        case VT_TOKEN_INT:
        case VT_TOKEN_HEX:
        case VT_TOKEN_FLOAT:
          if (chr == '.') {
            if (dot) {
              goto error_float_malformed;
            } else if (token == VT_TOKEN_INT || token == VT_TOKEN_OCTAL) {
              dot = 1;
              token = VT_TOKEN_FLOAT;
            } else {
              goto error_non_digit_in_const;
            }
          /* hexadecimal value */
          /* scientific notation in floating point value */
          } else if (chr == 'e' || chr == 'E') {
            nchr = vt_lexer_peek_chr (lxr, 1);
            /* scientific notation in floating point value */
            if (nchr == '-' || nchr == '+') {
              if (token == VT_TOKEN_FLOAT)
                chr = vt_lexer_get_chr (lxr);
              else
                goto error_non_digit_in_const;
            } else if ((token != VT_TOKEN_HEX   && ! (def->scan_float)) ||
                       (token != VT_TOKEN_HEX   &&
                        token != VT_TOKEN_FLOAT &&
                        token != VT_TOKEN_OCTAL &&
                        token != VT_TOKEN_INT))
            {
              goto error_non_digit_in_const;
            } else if (token != VT_TOKEN_HEX) {
              token = VT_TOKEN_FLOAT;
              loop = 0;
            }
          /* end of */
          } else if (! isdigit (chr) && ! (isxdigit (chr) && token == VT_TOKEN_HEX)) {
            loop = -1;
          }
          break;

        default:
          /* double/single quoted string values */
          if (vt_is_dquot (chr, def, scp_def) || vt_is_squot (chr, def, scp_def)) {
            token = VT_TOKEN_STRING;
            esc = 0;
            quot = chr;
            goto register_position;
          /* floating point value */
          } else if (chr == '.' && vt_scan_float (def, scp_def)) {
            dot = 1;
            token = VT_TOKEN_FLOAT;
            goto register_position;
          /* hexadecimal value */
          } else if (chr == '$' && vt_scan_hex_dollar (def, scp_def)) {
            dot = 0;
            token = VT_TOKEN_HEX;
            goto register_position;
          } else if (chr == '0') {
            dot = 0;
            nchr = vt_lexer_peek_chr (lxr, 1);
            /* hexadecimal value */
            if ((nchr == 'x' || nchr == 'X') && vt_scan_hex (def, scp_def)) {
              loop = 2;
              token = VT_TOKEN_HEX;
            /* binary value */
            } else if ((nchr == 'b' || nchr == 'B') && vt_scan_binary (def, scp_def)) {
              loop = 2;
              token = VT_TOKEN_BINARY;
            /* octal value */
            } else if (vt_scan_octal (def, scp_def)) {
              token = VT_TOKEN_OCTAL;
            /* integer value */
            } else {
              token = VT_TOKEN_INT;
            }
            goto register_position;
          /* integer value */
          } else if (chr == '-' || chr == '+' || isdigit (chr)) {
            dot = 0;
            token = VT_TOKEN_INT;
            goto register_position;
          /* string value */
          } else if (vt_is_str_first (chr, def, scp_def)) {
            token = VT_TOKEN_STRING;
            quot = '\0'; /* just to be safe */
            goto register_position;
          /* single line comment */
          } else if (chr == '#') {
            token = VT_TOKEN_COMMENT_SINGLE;
            goto register_position;
          } else if (chr == '/') {
            nchr = vt_lexer_peek_chr (lxr, 1);
            /* single line comment */
            if (nchr == '/')
              token = VT_TOKEN_COMMENT_SINGLE;
            /* multi line comment */
            else if (nchr == '*')
              token = VT_TOKEN_COMMENT_MULTI;
            else
              goto unrecognized_token;
            goto register_position;
          /* char value */
          } else if (! vt_is_space (chr, def, scp_def)) {
unrecognized_token:
            token = VT_TOKEN_CHAR;
            loop = 0;
register_position:
            pos = vt_lexer_get_pos (lxr);
            line = lxr->lines;
            column = vt_lexer_get_cur_column (lxr);//->columns;
          }
          break;
      }
    } else if (loop > 1) {
      loop--;
    }
  }

  for (; loop < 0 && (nchr = vt_lexer_unget_chr (lxr)); loop++)
    /* do nothing */ ;

  /* special cases apply to end of file in certain situations */
  if (chr == '\0' && (token == VT_TOKEN_COMMENT_MULTI ||
                     (token == VT_TOKEN_STRING && quot != '\0')))
  {
    vt_set_error (err, VT_ERR_INVAL);
    vt_error ("unexpected eof");
    goto error;
  }

  len = lxr->pos - pos;

  /* duplicate string if required */
  switch (token) {
    case VT_TOKEN_STRING:
      if (quot) {
        if (vt_is_dquot (quot, def, scp_def))
          str = vt_lexer_unesc_str_dquot (lxr->str + pos, len, quot, err);
        else
          str = vt_lexer_unesc_str_squot (lxr->str + pos, len, quot, err);
        if (str)
          break;
        else
          goto error;
      } else if (vt_scan_bool (def, scp_def)) {
        /* boolean value (true) */
        if (strncasecmp (lxr->str + pos, "true",  len) == 0 ||
            strncasecmp (lxr->str + pos, "yes",   len) == 0 ||
            strncasecmp (lxr->str + pos, "on",    len) == 0)
          token = vt_lexer_set_token_bool (lxr, VT_TOKEN_BOOL, line, column, true);
        /* boolean value (false) */
        if (strncasecmp (lxr->str + pos, "false", len) == 0 ||
            strncasecmp (lxr->str + pos, "no",    len) == 0 ||
            strncasecmp (lxr->str + pos, "off",   len) == 0)
          token = vt_lexer_set_token_bool (lxr, VT_TOKEN_BOOL, line, column, false);
        if (token == VT_TOKEN_BOOL)
          break;
      }
    /* fall through */
    case VT_TOKEN_COMMENT_SINGLE:
    case VT_TOKEN_COMMENT_MULTI:
    case VT_TOKEN_BINARY:
    case VT_TOKEN_OCTAL:
    case VT_TOKEN_INT:
    case VT_TOKEN_HEX:
    case VT_TOKEN_FLOAT:
      if (! (str = strndup (lxr->str + pos, len))) {
        vt_set_error (err, VT_ERR_NOMEM);
        vt_error ("%s: calloc: %s", __func__, strerror (errno));
        goto error;
      }
      break;
  }

  switch (token) {
    case VT_TOKEN_COMMENT_SINGLE:
    case VT_TOKEN_COMMENT_MULTI:
    case VT_TOKEN_STRING:
      vt_lexer_set_token_string (lxr, token, line, column, str);
      break;
    case VT_TOKEN_BOOL:
      vt_lexer_set_token_bool (lxr, token, line, column, bln);
      break;
    case VT_TOKEN_CHAR:
      if (vt_char_as_token (def, scp_def))
        token = chr;
      vt_lexer_set_token_char (lxr, token, line, column, chr);
      break;
    case VT_TOKEN_BINARY:
    case VT_TOKEN_OCTAL:
    case VT_TOKEN_INT:
    case VT_TOKEN_HEX:
    case VT_TOKEN_FLOAT:
      ptr = NULL;
      if (token == VT_TOKEN_FLOAT)
        dbl = strtod (str, &ptr);
      else if (token == VT_TOKEN_BINARY)
        lng = strtol (str+2, &ptr, 2);
      else if (token == VT_TOKEN_OCTAL)
        lng = strtol (str, &ptr, 8);
      else if (token == VT_TOKEN_INT)
        lng = strtol (str, &ptr, 10);
      else if (token == VT_TOKEN_HEX)
        lng = strtol (str, &ptr, 16);

      if (ptr && *ptr) {
        if (*ptr == 'e' || *ptr == 'E') {
error_non_digit_in_const:
          vt_error ("error_non_digit_in_const");
        } else if (*ptr == '.') {
error_float_malformed:
          vt_error ("error_float_malformed");
        } else {
error_digit_radix:
          vt_error ("error_digit_radix: %c", *ptr);
        }
        goto error;
      }

      free (str);

      /* convert binary, octal, and hexadecimal values to integer value */
      if (vt_numbers_as_int (def, scp_def) && token != VT_TOKEN_FLOAT) {
        token = VT_TOKEN_INT;
      }

      /* convert integer value to floating point value */
      if (vt_int_as_float (def, scp_def) && token == VT_TOKEN_INT) {
        dbl = lng;
        token = VT_TOKEN_FLOAT;
      }

      if (token == VT_TOKEN_FLOAT)
        vt_lexer_set_token_float (lxr, token, line, column, dbl);
      else if (token != VT_TOKEN_ERROR)
        vt_lexer_set_token_int (lxr, token, line, column, lng);
      break;
    case VT_TOKEN_EOF:
      vt_lexer_set_token_char (lxr, token, lxr->lines, lxr->columns, '\0');
      break;
    default:
error:
      token = VT_TOKEN_ERROR;
      vt_lexer_set_token_char (lxr, token, lxr->lines, lxr->columns, '\0');
      break;
  }

  return token;
}

#undef vt_is_space
#undef vt_is_str_fist
#undef vt_is_str
#undef vt_is_dquot
#undef vt_is_squot
