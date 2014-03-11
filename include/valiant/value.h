#ifndef VALUE_H_INCLUDED
#define VALUE_H_INCLUDED

#include <stdbool.h>

typedef enum {
  VALUE_NULL,
  VALUE_BOOL,
  VALUE_CHAR,
  VALUE_INT,
  VALUE_FLOAT,
  VALUE_STR
} value_type_t;

typedef struct {
  variant_type_t type;
  union {
    bool b; /* boolean */
    char c[5]; /* char (with support for Unicode) */
    long i; /* int */
    double f; /* float */
    char *s; /* string */
  } data;
} value_t;

int value_create (value_t **);
void value_destroy (value_t *);
void value_reset (variant_t *);

value_type_t value_get_type (value_t *);

// FIXME: we should also support getting the variable... it might be usefull
//        in some cases ;)
//int value_get_bool (value_t *, bool *);
//int value_get_char (value_t *, char *, size_t);
//int value_get_int (value_t *, long *);
//int value_get_float (value_t *,

int value_set_bool (value_t *, bool);
int value_set_char (value_t *, const char *); // we should just use ucs4_t here?
int value_set_int (value_t *, long);
int value_set_float (value_t *, double);
int value_set_str (value_t *, const char *);
int value_set_strlen (value_t *, const char *, size_t);

#endif
