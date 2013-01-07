#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED

/* functions like the ones in <ctype.h> that are not affected by locale.
   copied from GLib. */

typedef enum {
  ASCII_ALNUM = 1 << 0,
  ASCII_ALPHA = 1 << 1,
  ASCII_CNTRL = 1 << 2,
  ASCII_DIGIT = 1 << 3,
  ASCII_GRAPH = 1 << 4,
  ASCII_LOWER = 1 << 5,
  ASCII_PRINT = 1 << 6,
  ASCII_PUNCT = 1 << 7,
  ASCII_SPACE = 1 << 8,
  ASCII_UPPER = 1 << 9,
  ASCII_XDIGIT = 1 << 10
} ascii_type_t;

extern const uint16_t * const ascii_table;

#define ascii_isalnum(c) \
((ascii_table[(unsigned char) (c)] & ASCII_ALNUM) != 0)

#define ascii_isalpha(c) \
((ascii_table[(unsigned char) (c)] & ASCII_ALPHA) != 0)

#define ascii_iscntrl(c) \
((ascii_table[(unsigned char) (c)] & ASCII_CNTRL) != 0)

#define ascii_isdigit(c) \
((ascii_table[(unsigned char) (c)] & ASCII_DIGIT) != 0)

#define ascii_isgraph(c) \
((ascii_table[(unsigned char) (c)] & ASCII_GRAPH) != 0)

#define ascii_islower(c) \
((ascii_table[(unsigned char) (c)] & ASCII_LOWER) != 0)

#define ascii_isprint(c) \
((ascii_table[(unsigned char) (c)] & ASCII_PRINT) != 0)

#define ascii_ispunct(c) \
((ascii_table[(unsigned char) (c)] & ASCII_PUNCT) != 0)

#define ascii_isspace(c) \
((ascii_table[(unsigned char) (c)] & ASCII_SPACE) != 0)

#define ascii_isupper(c) \
((ascii_table[(unsigned char) (c)] & ASCII_UPPER) != 0)

#define ascii_isxdigit(c) \
((ascii_table[(unsigned char) (c)] & ASCII_XDIGIT) != 0)

extern const uint16_t * const ascii_lc_table;

#define ascii_tolower(c) \
  ((int)ascii_lc_table[(unsigned char) (c)])

extern const uint16_t * const ascii_uc_table;

#define ascii_toupper(c) \
  ((int)ascii_uc_table[(unsigned char) (c)])

ascii_strcasecmp (const char *, const char *);
ascii_strncasecmp (const char *, const char *, size_t);

#endif
