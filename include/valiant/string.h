#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED

#include <stdint.h>
/* ICU (International Components for Unicode) has been dropped in favor of
   libunistring to minimize the amount of external libraries required */
#include <unictype.h>

typedef enum {
  ASCII_ALNUM  = 1 << 0,
  ASCII_ALPHA  = 1 << 1,
  ASCII_CNTRL  = 1 << 2,
  ASCII_DIGIT  = 1 << 3,
  ASCII_GRAPH  = 1 << 4,
  ASCII_LOWER  = 1 << 5,
  ASCII_PRINT  = 1 << 6,
  ASCII_PUNCT  = 1 << 7,
  ASCII_SPACE  = 1 << 8,
  ASCII_UPPER  = 1 << 9,
  ASCII_XDIGIT = 1 << 10
} ascii_type_t;

extern const unsigned int * const ascii_table;

/* POSIX:2008 provides several *_l interfaces that take an additional locale
   parameter to select which locale is used. *_c interfaces below
       , the functions below are postfixed with *_c */

/* *_c postfix  POSIX:2008 provides several *_l functions */

// FIXME: instead of using "ascii_" prefix we should use "_c" postfix... a lot
//        of standard functions use the _l postfix. those functions allow you
//        to pass the locale... we use _c to indicate it uses the c (or POSIX)
//        locale

#define isalnum_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_ALNUM) != 0)

#define isalpha_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_ALPHA) != 0)

#define iscntrl_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_CNTRL) != 0)

#define isdigit_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_DIGIT) != 0)

#define isgraph_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_GRAPH) != 0)

#define islower_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_LOWER) != 0)

#define isprint_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_PRINT) != 0)

#define ispunct_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_PUNCT) != 0)

#define isspace_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_SPACE) != 0)

#define isupper_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_UPPER) != 0)

#define isxdigit_c(c) \
  ((ascii_table[(unsigned char) (c)] & ASCII_XDIGIT) != 0)

int strcasecmp_c (const char *, const char *)
  __attribute__ ((nonnull));
int strncasecmp_c (const char *, const char *, size_t)
  __attribute__ ((nonnull(1,2)));

int todigit_c (int);
int tolower_c (int);
int toupper_c (int);

int strtol_c (long *, const char *, char **, int)
  __attribute__ ((nonnull(1,2)));
int strtod_c (double *, const char *, char **)
  __attribute__ ((nonnull(1,2)));

#define stract_stop (-1)

typedef int(*u8_stract_t)(ucs4_t, int, void *);

int u8_strnact (const uint8_t *, size_t, u8_stract_t, void *)
  __attribute__ ((nonnull(1,3)));





// FOR NOW... STRING ONLY IS FOR ASCII, not binary data!



typedef struct {
  uint8_t *buf;
  size_t lim; /* maximum number of bytes */
  size_t len; /* available number of bytes */
  size_t cnt; /* occupied number of bytes */
  size_t pos; /* byte offset in buffer */
} string_t;


// using byte offset we can do an ltrim for example
// it's just that prepend suddenly becomes relative!
// of course append does not!
// but what about insert?!?!
// that would become relative as well...
////
//// and then of course we have the get char function... which could get ticky
//// do we want to use generics?!?! >> no it's to new for us to use...
//// we should implement something like get byte if we want to do stuff like
//// that
//
// everything will become relative
// yet somehow I don't think it's a good match... the get char stuff....
// >> I'm just not sure...
//
//
//

int string_construct (string_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1)));
int string_create (string_t **, const uint8_t *, size_t)
  __attribute__ ((nonnull));
void string_destruct (string_t *)
  __attribute__ ((nonnull(1)));
void string_destroy (string_t *);

void string_get_buffer (uint8_t **, string_t *)
  __attribute__ ((nonnull));
void string_get_length (size_t *, string_t *)
  __attribute__ ((nonnull));
void string_get_limit (size_t *, string_t *)
  __attribute__ ((nonnull));
int string_set_limit (string_t *, size_t)
  __attribute__ ((nonnull (1)));
void string_get_size (size_t *, string_t *)
  __attribute__ ((nonnull));
int string_set_size (string_t *, size_t)
  __attribute__ ((nonnull));





void string_empty (string_t *)
  __attribute__ ((nonnull));

int string_copy (string_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull (1)));
#if 0
int string_insert (string_t *, size_t, const uint8_t *, size_t)
  __attribute__ ((nonnull(1,3)))
int string_prepend (string_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1,2)));
#endif
int string_append (string_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1,2)));
#if 0
int string_prepend_char (string_t *, ucs4_t)
  __attribute__ ((nonnull (1)));
#endif
int string_append_char (string_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
//uint8_t *string_search_char (string_t *, ucs4_t)
//  __attribute__ ((nonnull (1)));

// using the pos thingy...
//int string_insert
//int string_append_char ()

/* string iterator interface */

int string_get_char (ucs4_t *, string_t *)
	__attribute__ ((nonnull));
int string_get_next_char (ucs4_t *, string_t *)
  __attribute__ ((nonnull(2)));
int string_get_previous_char (ucs4_t *, string_t *)
  __attribute__ ((nonnull(2)));
int string_get_index (size_t *, string_t *)
  __attribute__ ((nonnull));
int string_set_index (string_t *, size_t)
  __attribute__ ((nonnull(1)));


#endif
