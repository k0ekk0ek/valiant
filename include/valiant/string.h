#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED 1

#include <stdint.h>
#include <sys/types.h>
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

#define isalnum_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_ALNUM) != 0)

#define isalpha_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_ALPHA) != 0)

#define iscntrl_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_CNTRL) != 0)

#define isdigit_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_DIGIT) != 0)

#define isgraph_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_GRAPH) != 0)

#define islower_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_LOWER) != 0)

#define isprint_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_PRINT) != 0)

#define ispunct_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_PUNCT) != 0)

#define isspace_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_SPACE) != 0)

#define isupper_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_UPPER) != 0)

#define isxdigit_c(c) \
  ((ascii_table[(uint8_t) (c)] & ASCII_XDIGIT) != 0)

int strcasecmp_c (const uint8_t *, const uint8_t *)
  __attribute__ ((nonnull));
int strncasecmp_c (const uint8_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1,2)));

int todigit_c (int);
int tolower_c (int);
int toupper_c (int);

int strtol_c (long *, const uint8_t *, uint8_t **, int)
  __attribute__ ((nonnull(1,2)));
int strtod_c (double *, const uint8_t *, uint8_t **)
  __attribute__ ((nonnull(1,2)));


typedef struct {
  uint8_t *buf;
  size_t lim; /* maximum number of bytes */
  size_t len; /* available number of bytes */
  size_t cnt; /* occupied number of bytes */
  size_t pos; /* byte offset in buffer */
} string_t;

int string_construct (string_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1)));
int string_create (string_t **, const uint8_t *, size_t)
  __attribute__ ((nonnull (1)));
void string_destruct (string_t *)
  __attribute__ ((nonnull));
void string_destroy (string_t *);
void string_clear (string_t *)
  __attribute__ ((nonnull));

uint8_t *string_get_buffer (string_t *)
  __attribute__ ((nonnull));
size_t string_get_length (string_t *)
  __attribute__ ((nonnull));
size_t string_get_limit (string_t *)
  __attribute__ ((nonnull));
int string_set_limit (string_t *, size_t)
  __attribute__ ((nonnull(1)));
size_t string_get_size (string_t *)
  __attribute__ ((nonnull));
int string_set_size (string_t *, size_t)
  __attribute__ ((nonnull(1)));
size_t string_get_state (string_t *)
  __attribute__ ((nonnull));
int string_set_state (string_t *, size_t)
  __attribute__ ((nonnull(1)));

int string_copy (string_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1,2)));
int string_append (string_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1,2)));

ssize_t string_find_char (string_t *, ucs4_t)
  __attribute__ ((nonnull(1)));

int string_get_char (ucs4_t *, string_t *)
  __attribute__ ((nonnull));
int string_get_next_char (ucs4_t *, string_t *)
  __attribute__ ((nonnull(2)));
int string_get_previous_char (ucs4_t *, string_t *)
  __attribute__ ((nonnull(2)));

#endif /* STRING_H_INCLUDED */
