#ifndef VT_LEXER_H_INCLUDED
#define VT_LEXER_H_INCLUDED

/* This lexical analyser is heavily inspired by GLib's GScanner and even uses
   some of it's code, although modified to fit my needs. It's not as feature
   packed as it's big brother, but it allows for more dynamic behaviour in
   situations where I need it. */

/* system includes */
#include <stdbool.h>

/* valiant includes */
#include "vt_error.h"
#include "vt_value.h"

#define VT_CHRS_AZ_LC "abcdefghijklmnopqrstuvwxyz"
#define VT_CHRS_AZ_UC "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define VT_CHRS_DIGIT "0123456789"

/* character classes used directly by lexer */
#define VT_CHRS_SPACE     " \t\r\n"
#define VT_CHRS_STR_FIRST VT_CHRS_AZ_LC VT_CHRS_AZ_UC VT_CHRS_DIGIT
#define VT_CHRS_STR       VT_CHRS_STR_FIRST "-_"
#define VT_CHRS_DQUOT     "\""
#define VT_CHRS_SQUOT     "\'"

#define VT_SKIP_COMMENT_SINGLE (1)
#define VT_SKIP_COMMENT_MULTI (1)
#define VT_SCAN_BOOL (1)
#define VT_SCAN_BINARY (0)
#define VT_SCAN_OCTAL (1)
#define VT_SCAN_FLOAT (1)
#define VT_SCAN_HEX (1)
#define VT_SCAN_HEX_DOLLAR (1)
#define VT_SCAN_STR_DQUOT (1)
#define VT_SCAN_STR_SQUOT (1)
#define VT_NUMBERS_AS_INT (1)
#define VT_INT_AS_FLOAT (0)
#define VT_CHAR_AS_TOKEN (1)

typedef enum _vt_token vt_token_t;

enum _vt_token {
  VT_TOKEN_EOF = 0,
  VT_TOKEN_LEFT_PAREN = '(',
  VT_TOKEN_RIGHT_PAREN = ')',
  VT_TOKEN_LEFT_CURLY = '{',
  VT_TOKEN_RIGHT_CURLY = '}',
  VT_TOKEN_LEFT_BRACE = '[',
  VT_TOKEN_RIGHT_BRACE = ']',
  VT_TOKEN_EQUAL_SIGN = '=',
  VT_TOKEN_COMMA = ',',
  VT_TOKEN_NONE = 256,
  VT_TOKEN_ERROR,
  VT_TOKEN_BOOL,
  VT_TOKEN_CHAR,
  VT_TOKEN_BINARY,
  VT_TOKEN_OCTAL,
  VT_TOKEN_INT,
  VT_TOKEN_HEX,
  VT_TOKEN_FLOAT,
  VT_TOKEN_STR,
  VT_TOKEN_COMMENT_SINGLE,
  VT_TOKEN_COMMENT_MULTI,
  VT_TOKEN_LAST
};

typedef struct _vt_lexer_def vt_lexer_def_t;

struct _vt_lexer_def {
  char *space; /* char used as white space, NOTE: can't overwrite newline */
  char *str_first; /* chars allowed as first character of identifier */
  char *str; /* chars allowed in identifier */
  char *dquot; /* chars used as double quote */
  char *squot; /* chars used as single quote */

  int skip_comment_multi : 1;
  int skip_comment_single : 1;
  int scan_bool : 1;
  int scan_binary : 1;
  int scan_octal : 1;
  int scan_float : 1;
  int scan_int : 1;
  int scan_hex : 1;
  int scan_hex_dollar : 1;
  int scan_str_dquot : 1;
  int scan_str_squot : 1;
  int numbers_as_int : 1;
  int int_as_float : 1;
  int char_as_token : 1;
  int padding_dummy;
};

typedef struct _vt_lexer vt_lexer_t;

struct _vt_lexer {
  char *str;
  size_t len;
  size_t pos;
  size_t columns;
  size_t lines;
  int eof;
  vt_lexer_def_t *def;
  /* public */
  vt_token_t token;
  size_t column;
  size_t line;
  vt_value_t value;
};

void vt_lexer_init (vt_lexer_t *, const char *, size_t);
void vt_lexer_deinit (vt_lexer_t *);
vt_token_t vt_lexer_get_next_token (vt_lexer_t *, vt_lexer_def_t *, int *);

#endif
