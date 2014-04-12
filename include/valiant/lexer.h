#ifndef LEXER_H_INCLUDED
#define LEXER_H_INCLUDED 1

#include <valiant/value.h>

/* lexical analyzer heavily inspired by GLib's GScanner interface, modified to
   suit my needs. It's not as feature packed as GScanner, but allows for more
   dynamic behavior where I need it. */

typedef enum {
  TOKEN_NONE = 0,
  TOKEN_EOF = TOKEN_NONE, /* TOKEN_EOF is equivalent of TOKEN_NONE */
  TOKEN_LEFT_PAREN = '(',
  TOKEN_RIGHT_PAREN = ')',
  TOKEN_LEFT_CURLY = '{',
  TOKEN_RIGHT_CURLY = '}',
  TOKEN_LEFT_BRACE = '[',
  TOKEN_RIGHT_BRACE = ']',
  TOKEN_EQUAL_SIGN = '=',
  TOKEN_COMMA = ',',
  TOKEN_BOOL,
  TOKEN_CHAR,
  TOKEN_INT,
  TOKEN_FLOAT,
  TOKEN_STR,
  TOKEN_IDENT,
  TOKEN_IDENT_NULL, /* "null" or "NULL" */
  TOKEN_COMMENT_SINGLE,
  TOKEN_COMMENT_MULTI,
  TOKEN_STR_SQUOT,
  TOKEN_STR_DQUOT,
  TOKEN_LAST
} token_t;

/* Unicode defines a LOT of code points. instead of specifying which characters
   can be part of an identifier, it's also possible to exclude certain
   characters from the default set using not_* */

/* lexical analyzer scans for boolean and numeric values using POSIX locale. to
   scan for a boolean or numeric value in a different locale or script, quote
   the value and offload interpretation */

/* CHAR_AS_TOKEN removed as a result of Unicode support */

/* TOKEN_OCT and TOKEN_HEX tokens removed because they where considered
   ambiguous. user can still enable/disable support, but they are reported as
   either TOKEN_INT or TOKEN_FLOAT based on the numerical value as interpreted
   by the lexical analyzer */

/* TOKEN_ERROR removed because lexical analyzer does not verify input, it
   simply recognizes sequences opportunistically */

#define LEXER_STR_SQUOT '\''
#define LEXER_STR_DQUOT '"'

#define LEXER_CONTEXT_APPLY(X)      \
  X(STR,skip_chars,NULL)            \
  X(STR,ident_first,NULL)           \
  X(STR,not_ident_first,NULL)       \
  X(STR,ident,NULL)                 \
  X(STR,not_ident,NULL)             \
  X(CHAR,str_squot,LEXER_STR_SQUOT) \
  X(CHAR,str_dquot,LEXER_STR_DQUOT) \
  X(BOOL,scan_comment_single,true)  \
  X(BOOL,scan_comment_multi,true)   \
  X(BOOL,skip_comment_single,true)  \
  X(BOOL,skip_comment_multi,true)   \
  X(BOOL,scan_str_squot,true)       \
  X(BOOL,scan_str_dquot,true)       \
  X(BOOL,skip_str_squot,true)       \
  X(BOOL,skip_str_dquot,true)       \
  X(BOOL,scan_bool,true)            \
  X(BOOL,scan_oct,true)             \
  X(BOOL,scan_int,true)             \
  X(BOOL,scan_hex,true)             \
  X(BOOL,scan_float,true)           \
  X(BOOL,int_as_float,false)        \
  X(BOOL,ident_as_str,false)

/* internal structure of lexer must remain opague */
typedef struct lexer lexer_t;

int lexer_create (lexer_t **, const uint8_t *, size_t)
  __attribute__ ((nonnull (1,2)));
void lexer_destroy (lexer_t *)
  __attribute__ ((nonnull));

/* generate context accessor function prototypes */
#define LEXER_X_STR(name)                           \
int lexer_get_ ## name (uint8_t **, lexer_t *)      \
  __attribute__ ((nonnull));                        \
int lexer_set_ ## name (lexer_t *, const uint8_t *) \
  __attribute__ ((nonnull(1)));
#define LEXER_X_CHAR(name)                          \
ucs4_t lexer_get_ ## name (lexer_t *)               \
  __attribute__ ((nonnull));                        \
void lexer_set_ ## name (lexer_t *, ucs4_t)         \
  __attribute__ ((nonnull(1)));
#define LEXER_X_BOOL(name)                          \
bool lexer_get_ ## name (lexer_t *)                 \
  __attribute__ ((nonnull));                        \
void lexer_set_ ## name (lexer_t *, bool)           \
  __attribute__ ((nonnull (1)));
#define LEXER_X(type,name,default) LEXER_X_ ## type (name)

LEXER_CONTEXT_APPLY(LEXER_X)

#undef LEXER_X
#undef LEXER_X_BOOL
#undef LEXER_X_CHAR
#undef LEXER_X_STR

int lexer_get_token (token_t *, lexer_t *)
  __attribute__ ((nonnull));
int lexer_get_next_token (token_t *, lexer_t *)
  __attribute__ ((nonnull));
/* lexer is stateless, except for quotes. ungetting a token does not rewind
   lexer state, and is useful for e.g. PCRE search and replace support */
void lexer_unget_token (lexer_t *)
  __attribute__ ((nonnull));

size_t lexer_get_line (lexer_t *)
  __attribute__ ((nonnull));
size_t lexer_get_column (lexer_t *)
  __attribute__ ((nonnull));
int lexer_get_value (value_t *, lexer_t *)
  __attribute__ ((nonnull));

/* lexer.c cleans up after itself */
#if !defined LEXER_EXPORT_MACROS
#undef LEXER_CONTEXT_APPLY
#endif

#endif /* LEXER_H_INCLUDED */
