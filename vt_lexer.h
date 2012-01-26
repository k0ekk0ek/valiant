#ifndef V_LEXER_H_INCLUDED
#define V_LEXER_H_INCLUDED

/* valiant includes */
#include "vt_buf.h"
#include "vt_error.h"

typedef enum _vt_lexer_state vt_lexer_state_t;

enum _vt_lexer_state {
  VT_LEXER_STATE_NONE, // 0
  VT_LEXER_STATE_COMMENT, // 1
  VT_LEXER_STATE_OPTION, // 2
  VT_LEXER_STATE_OPERATOR, // 3
  VT_LEXER_STATE_VALUE, // 4
  VT_LEXER_STATE_TITLE, // 5
  VT_LEXER_STATE_SECTION, // 6
  VT_LEXER_STATE_DELIM, //7
  VT_LEXER_STATE_VALUE_DELIM, // 8
  VT_LEXER_STATE_SEPERATOR // 9
};

typedef enum _vt_lexer_token vt_lexer_token_t;

enum _vt_lexer_token {
  VT_TOKEN_EOF, /* end of file */
  VT_TOKEN_OPTION,
  VT_TOKEN_TITLE,
  VT_TOKEN_LEFT_CURLY,
  VT_TOKEN_RIGHT_CURLY,
  VT_TOKEN_OPERATOR,
  VT_TOKEN_VALUE,
  VT_TOKEN_SEPERATOR,
  VT_TOKEN_ERROR
};

typedef struct _vt_lexer vt_lexer_t;

struct _vt_lexer {
  char *str;
  size_t len;
  size_t pos;
  size_t colno;
  size_t lineno;
  vt_lexer_state_t state;
  vt_buf_t value;
};

typedef struct _vt_lexer_opts vt_lexer_opts_t;

struct _vt_lexer_opts {
  char *class;
  char *delim;
  char *sep;
};

vt_lexer_t *vt_lexer_create (const char *, size_t, vt_error_t *err);
void vt_lexer_destroy (vt_lexer_t *lexer);
vt_lexer_token_t vt_lexer_get_token (vt_lexer_t *lexer, vt_lexer_opts_t *, vt_error_t *err);
char *vt_lexer_gets (vt_lexer_t *lexer);

#endif
