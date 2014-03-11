#ifndef LEXER_H_INCLUDED
#define LEXER_H_INCLUDED

/* lexical analyser heavily inspired by GLib's GScanner interface, modified to
   suit my needs. It's not as feature packed as GScanner, but allows for more
   dynamic behaviour where I need it. */

typedef enum {
  TOKEN_EOF = 0,
  TOKEN_LEFT_PAREN = '(',
  TOKEN_RIGHT_PAREN = ')',
  TOKEN_LEFT_CURLY = '{',
  TOKEN_RIGHT_CURLY = '}',
  TOKEN_LEFT_BRACE = '[',
  TOKEN_RIGHT_BRACE = ']',
  TOKEN_EQUAL_SIGN = '=',
  TOKEN_COMMA = ',',
  TOKEN_NONE = 256,
  TOKEN_ERROR,
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
  TOKEN_IDENT,
  TOKEN_LAST
} token_t;

// FIXME: octal and hexadecimal tokens aren't even necessary... the user can
//        still specify if he/she wants to scan for those... but it's not a
//        seperate token anymore... this is because one can specify hexadecimal
//        values with doubles to... it would make it confusing!
//  TOKEN_OCT, /* base-8 integer */
//  TOKEN_HEX, /* base-16 integer */
//

/* IT'S UP TO THE USER TO CONVERT BETWEEN LOCALES! THIS LEXER REQUIRES THE
   USER TO PASS THE TEXT AS UTF8 */

// I've come to the conclusion that I simply cannot support different
// numerical systems in this code. It would involve using the locale and then
// some to hopefully identify anything usefull
//
// And even then we'll run into a lot of problems... roman notation for
// instance.
//
// plus there are a lot of scripts that don't even closely resemble the way in
// which I'm used to notate numbers
// a post on the libunistring list mentions the following:
//
/*
Yes, this would be useful. You can implement such a function
yourself, roughly like this:
1) Decide what kinds of decimal point character, decimal
grouping character, exponent marker, sign character,
and digit scripts you want to support in that conversion.
2) Verify that all digits in the input are from the same
script.
3) Convert the digits to their numerical value using the
uc_digit_value function.
4) Build up a 'char *' string with the corresponding ASCII
digits and sign, and without grouping characters.
5) Pass that string to strtol or strtod.

Such a function will not be in libunistring in the near term,
because the decisions in 1) are not clear to me yet, how to
do them right. 
*/
//
// just to indicate some of the problems
//
// the only way to support those kinds of numerical systems (if that's even
// the proper name) is to quote or double quote them. There's simply no other
// way to have this functionality in a lexer
//
// the same situation applies to boolean values... it's just not possible to
// support true/false in other language e.g. in dutch is would be something
// like waar/onwaar... it's t much work (and would be subject to opionions
// etc) to do something like that in this code...
//
// in short... just disable the options etc of scanning boolean values and
// numerical values if you want to use this lexer for locales other than en_US
// and implement that functionality in the configuration parser you write
//
// ... I know ... it isn't fair ... life hardly ever is ... sorry about that

// we can get away with most of the stuff for strings... that's easy...
// numbers however that's something else:
// http://icu-project.org/apiref/icu4j/com/ibm/icu/text/DecimalFormat.html
// http://userguide.icu-project.org/formatparse/number
// http://savannah.gnu.org/support/?106998


/* NOTE: unicode defines a LOT of code points. instead of specifying which
         characters can be part of an identifier, it's also possible to
         exclude certain characters from the default set using not_* */
/* NOTE: user defined boolean values could be a nice feature */
/* NOTE: CHAR_AS_TOKEN removed as a result of Unicode support */

#define LEXER_STR_SQUOT '\''
#define LEXER_STR_DQUOT '"'

#define LEXER_CONTEXT_EXPAND()      \
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

/* internal structure of lexer should remain opague */
typedef struct lexer lexer_t;

int lexer_create (lexer_t **, const uint8_t *, size_t)
  __attribute__ ((nonnull (1,2)));
int lexer_destroy (lexer_t *)
  __attribute__ ((nonnull));

/* generate context accessor function prototypes */
#define X_STR(name)                                 \
int lexer_get_ ## name (uint8_t **, lexer_t *)      \
  __attribute__ ((nonnull (1,2)));                  \
int lexer_set_ ## name (lexer_t *, const uint8_t *) \
  __attribute__ ((nonnull (1)));
#define X_CHAR(name)                                \
ucs4_t lexer_get_ ## name (lexer_t *)               \
  __attribute__ ((nonnull));                        \
void lexer_set_ ## name (lexer_t *, ucs4_t)         \
  __attribute__ ((nonnull));
#define X_BOOL(name)                                \
bool lexer_get_ ## name (lexer_t *)                 \
  __attribute__ ((nonnull));                        \
void lexer_set_ ## name (lexer_t *, bool)           \
  __attribute__ ((nonnull (1)));
#define X(type,name,default) X_ ## type (name)

LEXER_CONTEXT_EXPAND();

#undef X
#undef X_BOOL
#undef X_CHAR
#undef X_STR


//int lexer_get_token (token_t *, lexer_t *)
//  __attribute__((nonnull));

int lexer_shift_token (token_t *, lexer_t *);

int lexer_unshift_token (lexer_t *);

/* dynamically change behaviour of lexical analyzer by setting token,
   note however that only delimiting tokens can be specified. e.g.
   TOKEN_STR_SQUOT, TOKEN_LEFT_PAREN, etc. */
//int lexer_set_token (lexer_t *, token_t *);
// shouldn't we just implement an unget_token, but instead of ungetc, don't
// allow the user to specify a token himself?!?! >> it just seems to make more
// sense... eg. we don't have to worry about keeping state?!?!
//int lexer_unget_token (lexer_t *); >> not supporting to pass a token generates
// more work because we'd have to keep more state?!?! >> yes... that's correct
// but we cannot allow the user to specify a line number etc ... so it wouldn't
// fit... unget_token without token seems the most logical solution... it does
// however introduce a lot of work... depending on the token that's ungotten
//int lexer_shift_
//int lexer_lapse_token (lexer_t *); // I'm not happy with this name as well
// it gives the wrong impression
//
// using shift instead of get
// and unshift instead of unget >> get and unget are better... it really
// doesn't say anything about state?? >> yeah actually it kinda does!

int lexer_get_line (unsigned int **, lexer_t *);
int lexer_get_column (unsigned int **, lexer_t *);
int lexer_get_value (value_t **, lexer_t *);

/* lexer.c cleans up after itself */
#ifndef LEXER_OPTIONS_EXPORT
#undef LEXER_OPTIONS
#endif

#endif
