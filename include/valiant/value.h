#ifndef VALUE_H_INCLUDED
#define VALUE_H_INCLUDED 1

#include <stdbool.h>
#include <stdint.h>
#include <unitypes.h>

typedef enum {
  VALUE_NULL,
  VALUE_BOOL,
  VALUE_CHAR,
  VALUE_LONG,
  VALUE_DOUBLE,
  VALUE_STR
} value_type_t;

typedef struct {
  value_type_t type;
  union {
    bool bln; /* boolean */
    ucs4_t chr; /* char */
    long lng; /* long */
    double dbl; /* double */
    uint8_t *str; /* string */
  } data;
} value_t;

int value_create (value_t **)
  __attribute__ ((nonnull));
void value_destroy (value_t *)
  __attribute__ ((nonnull));
void value_clear (value_t *)
  __attribute__ ((nonnull));
int value_copy (value_t *, value_t *)
  __attribute__ ((nonnull));

value_type_t value_get_type (value_t *)
  __attribute__ ((nonnull));

int value_get_bool (bool *, value_t *)
  __attribute__ ((nonnull));
void value_set_bool (value_t *, bool)
  __attribute__ ((nonnull(1)));
int value_get_char (ucs4_t *, value_t *)
  __attribute__ ((nonnull));
void value_set_char (value_t *, ucs4_t)
  __attribute__ ((nonnull(1)));
int value_get_long (long *, value_t *)
  __attribute__ ((nonnull));
void value_set_long (value_t *, long)
  __attribute__ ((nonnull(1)));
int value_get_double (double *, value_t *)
  __attribute__ ((nonnull));
void value_set_double (value_t *, double)
  __attribute__ ((nonnull(1)));
int value_get_str (uint8_t **, value_t *)
  __attribute__ ((nonnull));
int value_set_str (value_t *, const uint8_t *, size_t)
  __attribute__ ((nonnull(1,2)));

#endif /* VALUE_H_INCLUDED */
