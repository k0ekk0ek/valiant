#include <errno.h>
#include <stdlib.h>
#include <valiant/lexer.h>
#include <CUnit/Basic.h>


lexer_t *lex = NULL;
token_t tok;
value_t val;

int
lexer_test_init (const uint8_t *str, const size_t len)
{
  int err = lexer_create (&lex, str, len);
  CU_ASSERT (err == 0);
  tok = TOKEN_NONE;
  return err;
}

void
lexer_test_deinit (void)
{
  if (lex != NULL) {
    lexer_destroy (lex);
    lex = NULL;
    value_clear (&val);
  }
}

void
lexer_test_empty (void)
{
  int err = 0;
  const uint8_t str[] = "\n ";

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_NONE);
    if (err == 0) {
      CU_ASSERT (lexer_get_line (lex) == 2);
      CU_ASSERT (lexer_get_column (lex) == 1);
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_ident (void)
{
  int err = 0;
  const uint8_t str[] = "ïdênt";
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_IDENT);
    if (err == 0) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
      if (err == 0 && type == VALUE_STR) {
        err = value_get_str (&ptr, &val);
        CU_ASSERT (err == 0 && strcmp (ptr, str) == 0);
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

void
lexer_test_ident_as_str (void)
{
  int err = 0;
  const uint8_t str[] = "ïdênt";
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_ident_as_str (lex, true);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_STR);
    if (err == 0) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
      if (err == 0 && type == VALUE_STR) {
        err = value_get_str (&ptr, &val);
        CU_ASSERT (err == 0 && strcmp (ptr, str) == 0);
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

void
lexer_test_ident_null (void)
{
  int err;
  const uint8_t str[] = "null";

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_IDENT_NULL);
    if (err == 0) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && value_get_type (&val) == VALUE_NULL);
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_ident_null_as_str (void)
{
  int err;
  const uint8_t str[] = "null";
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_ident_as_str (lex, true);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_STR);
    if (err == 0) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
      if (err == 0 && type == VALUE_STR) {
        err = value_get_str (&ptr, &val);
        CU_ASSERT (err == 0 && strcmp (ptr, str) == 0);
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

void
lexer_test_bool (void)
{
  int err = 0;
  const uint8_t str[] = "true";
  bool bln;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_BOOL);
    if (err == 0 && tok == TOKEN_BOOL) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_BOOL);
      if (err == 0 && type == VALUE_BOOL) {
        CU_ASSERT (value_get_bool (&bln, &val) == 0 && bln == true);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_int (void)
{
  int err = 0;
  const uint8_t str[] = "101";
  long lng;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_INT);
    if (err == 0 && tok == TOKEN_INT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_LONG);
      if (err == 0 && type == VALUE_LONG) {
        CU_ASSERT (value_get_long (&lng, &val) == 0 && lng == 101);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_int_as_float (void)
{
  int err = 0;
  const uint8_t str[] = "101";
  double dbl;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_int_as_float (lex, true);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_FLOAT);
    if (err == 0 && tok == TOKEN_FLOAT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_DOUBLE);
      if (err == 0 && type == VALUE_DOUBLE) {
        CU_ASSERT (value_get_double (&dbl, &val) == 0 && dbl == 101.0);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_int_no_scan_float (void)
{
  int err = 0;
  const uint8_t str[] = "101.1";
  long lng;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_scan_float (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_INT);
    if (err == 0 && tok == TOKEN_INT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_LONG);
      if (err == 0 && type == VALUE_LONG) {
        CU_ASSERT (value_get_long (&lng, &val) == 0 && lng == 101);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_int_no_scan_oct (void)
{
  int err = 0;
  const uint8_t str[] = "0101";
  long lng;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_scan_oct (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_INT);
    if (err == 0 && tok == TOKEN_INT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_LONG);
      if (err == 0 && type == VALUE_LONG) {
        CU_ASSERT (value_get_long (&lng, &val) == 0 && lng == 101);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_int_no_scan_hex (void)
{
  int err = 0;
  const uint8_t str[] = "0x101";
  long lng;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_scan_hex (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_INT);
    if (err == 0 && tok == TOKEN_INT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_LONG);
      if (err == 0 && type == VALUE_LONG) {
        CU_ASSERT (value_get_long (&lng, &val) == 0 && lng == 0);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_float (void)
{
  int err = 0;
  const uint8_t str[] = "101.1";
  double dbl;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_FLOAT);
    if (err == 0 && tok == TOKEN_FLOAT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_DOUBLE);
      if (err == 0 && type == VALUE_DOUBLE) {
        CU_ASSERT (value_get_double (&dbl, &val) == 0 && dbl == 101.1);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_float_no_scan_int (void)
{
  int err = 0;
  const uint8_t str[] = "101";
  double dbl;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_scan_int (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_FLOAT);
    if (err == 0 && tok == TOKEN_FLOAT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_DOUBLE);
      if (err == 0 && type == VALUE_DOUBLE) {
        CU_ASSERT (value_get_double (&dbl, &val) == 0 && dbl == 101.0);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_float_no_scan_oct (void)
{
  int err = 0;
  const uint8_t str[] = "0101";
  double dbl;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_scan_int (lex, false);
    lexer_set_scan_oct (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_FLOAT);
    if (err == 0 && tok == TOKEN_FLOAT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_DOUBLE);
      if (err == 0 && type == VALUE_DOUBLE) {
        CU_ASSERT (value_get_double (&dbl, &val) == 0 && dbl == 101.0);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_float_no_scan_hex (void)
{
  int err = 0;
  const uint8_t str[] = "0x101";
  double dbl;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_scan_int (lex, false);
    lexer_set_scan_hex (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_FLOAT);
    if (err == 0 && tok == TOKEN_FLOAT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_DOUBLE);
      if (err == 0 && type == VALUE_DOUBLE) {
        CU_ASSERT (value_get_double (&dbl, &val) == 0 && dbl == 0.0);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_oct (void)
{
  int err = 0;
  const uint8_t str[] = "0145";
  long lng;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_INT);
    if (err == 0 && tok == TOKEN_INT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_LONG);
      if (err == 0 && type == VALUE_LONG) {
        CU_ASSERT (value_get_long (&lng, &val) == 0 && lng == 101);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_hex (void)
{
  int err = 0;
  const uint8_t str[] = "0x65";
  long lng;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_INT);
    if (err == 0 && tok == TOKEN_INT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_LONG);
      if (err == 0 && type == VALUE_LONG) {
        CU_ASSERT (value_get_long (&lng, &val) == 0 && lng == 101);
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_float_hex (void)
{
  int err = 0;
  const uint8_t str[] = "0x65.19999999A";
  double dbl;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_FLOAT);
    if (err == 0 && tok == TOKEN_FLOAT) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_DOUBLE);
      if (err == 0 && type == VALUE_DOUBLE) {
        CU_ASSERT (value_get_double (&dbl, &val) == 0 && (dbl >= 101.1 && dbl <= 101.101));
      }
    }
  }

  lexer_test_deinit ();
}

void
lexer_test_str_squot (void)
{
  int err = 0;
  const uint8_t str[] = "\'s\\t\\ring\'";
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_STR);
    if (err == 0) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
      if (err == 0 && type == VALUE_STR) {
        err = value_get_str (&ptr, &val);
        CU_ASSERT (err == 0 && strcmp (ptr, "s\\t\\ring") == 0);
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

void
lexer_test_no_skip_str_squot (void)
{
  int err = 0;
  const uint8_t str[] = "\'s\\t\\ring\'";
  token_t tok;
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_skip_str_squot (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_STR_SQUOT);
    if (err == 0) {
      err = lexer_get_next_token (&tok, lex);
      CU_ASSERT (err == 0 && tok == TOKEN_STR);
      if (err == 0) {
        err = lexer_get_value (&val, lex);
        CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
        if (err == 0 && type == VALUE_STR) {
          err = value_get_str (&ptr, &val);
          CU_ASSERT (err == 0 && strcmp (ptr, "s\\t\\ring") == 0);
          if (err == 0) {
            err = lexer_get_next_token (&tok, lex);
            CU_ASSERT (err == 0 && tok == TOKEN_STR_SQUOT);
          }
        }
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

void
lexer_test_str_dquot (void)
{
  int err = 0;
  const uint8_t str[] = "\"s\\t\\ring\"";
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_STR);
    if (err == 0) {
      err = lexer_get_value (&val, lex);
      CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
      if (err == 0 && type == VALUE_STR) {
        err = value_get_str (&ptr, &val);
        CU_ASSERT (err == 0 && strcmp (ptr, "s\t\ring") == 0);
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

void
lexer_test_no_skip_str_dquot (void)
{
  int err = 0;
  const uint8_t str[] = "\"s\\t\\ring\"";
  token_t tok;
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_skip_str_dquot (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_STR_DQUOT);
    if (err == 0) {
      err = lexer_get_next_token (&tok, lex);
      CU_ASSERT (err == 0 && tok == TOKEN_STR);
      if (err == 0) {
        err = lexer_get_value (&val, lex);
        CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
        if (err == 0 && type == VALUE_STR) {
          err = value_get_str (&ptr, &val);
          CU_ASSERT (err == 0 && strcmp (ptr, "s\t\ring") == 0);
          if (err == 0) {
            err = lexer_get_next_token (&tok, lex);
            CU_ASSERT (err == 0 && tok == TOKEN_STR_DQUOT);
          }
        }
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

void
lexer_test_unget_no_skip_str_squot (void)
{
  int err = 0;
  const uint8_t str[] = "\'foo\'bar\'";
  token_t tok;
  uint8_t *ptr = NULL;
  value_type_t type;

  if (lexer_test_init (str, sizeof (str) - 1) == 0) {
    lexer_set_skip_str_squot (lex, false);
    err = lexer_get_token (&tok, lex);
    CU_ASSERT (err == 0 && tok == TOKEN_STR_SQUOT);
    if (err == 0) {
      err = lexer_get_next_token (&tok, lex);
      CU_ASSERT (err == 0 && tok == TOKEN_STR);
      if (err == 0) {
        err = lexer_get_value (&val, lex);
        CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
        if (err == 0 && type == VALUE_STR) {
          err = value_get_str (&ptr, &val);
          CU_ASSERT (err == 0 && strcmp (ptr, "foo") == 0);
          if (err == 0) {
            err = lexer_get_next_token (&tok, lex);
            CU_ASSERT (err == 0 && tok == TOKEN_STR_SQUOT);
            if (err == 0) {
              lexer_unget_token (lex);
              err = lexer_get_next_token (&tok, lex);
              CU_ASSERT (err == 0 && tok == TOKEN_STR_SQUOT);
              if (err == 0 && tok == TOKEN_STR_SQUOT) {
                err = lexer_get_next_token (&tok, lex);
                CU_ASSERT (err == 0 && tok == TOKEN_STR);
                if (err == 0 && tok == TOKEN_STR) {
                  err = lexer_get_value (&val, lex);
                  CU_ASSERT (err == 0 && (type = value_get_type (&val)) == VALUE_STR);
                  if (err == 0 && type == VALUE_STR) {
                    free (ptr);
                    ptr = NULL;
                    err = value_get_str (&ptr, &val);
                    CU_ASSERT (err == 0 && strcmp (ptr, "bar") == 0);
                    if (err == 0) {
                      err = lexer_get_next_token (&tok, lex);
                      CU_ASSERT (err == 0 && tok == TOKEN_STR_SQUOT);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (ptr != NULL) {
    free (ptr);
  }
  lexer_test_deinit ();
}

int
main (int argc, char *argv[])
{
  CU_pSuite suite = NULL;

  if (CUE_SUCCESS != CU_initialize_registry())
     return CU_get_error();

  suite = CU_add_suite("string", NULL, NULL);
  if (NULL == suite) {
     CU_cleanup_registry();
     return CU_get_error();
  }

  if (!CU_add_test(suite, "lexer_test_empty", &lexer_test_empty) ||
      !CU_add_test(suite, "lexer_test_ident", &lexer_test_ident) ||
      !CU_add_test(suite, "lexer_test_ident_as_str", &lexer_test_ident_as_str) ||
      !CU_add_test(suite, "lexer_test_ident_null", &lexer_test_ident_null) ||
      !CU_add_test(suite, "lexer_test_ident_null_as_str", &lexer_test_ident_null_as_str) ||
      !CU_add_test(suite, "lexer_test_bool", &lexer_test_bool) ||
      !CU_add_test(suite, "lexer_test_int", &lexer_test_int) ||
      !CU_add_test(suite, "lexer_test_int_as_float", &lexer_test_int_as_float) ||
      !CU_add_test(suite, "lexer_test_int_no_scan_float", &lexer_test_int_no_scan_float) ||
      !CU_add_test(suite, "lexer_test_int_no_scan_oct", &lexer_test_int_no_scan_oct) ||
      !CU_add_test(suite, "lexer_test_int_no_scan_hex", &lexer_test_int_no_scan_hex) ||
      !CU_add_test(suite, "lexer_test_float", &lexer_test_float) ||
      !CU_add_test(suite, "lexer_test_float_no_scan_int", &lexer_test_float_no_scan_int) ||
      !CU_add_test(suite, "lexer_test_float_no_scan_oct", &lexer_test_float_no_scan_oct) ||
      !CU_add_test(suite, "lexer_test_float_no_scan_hex", &lexer_test_float_no_scan_hex) ||
      !CU_add_test(suite, "lexer_test_oct", &lexer_test_oct) ||
      !CU_add_test(suite, "lexer_test_hex", &lexer_test_hex) ||
      !CU_add_test(suite, "lexer_test_float_hex", &lexer_test_float_hex) ||
      !CU_add_test(suite, "lexer_test_str_squot", &lexer_test_str_squot) ||
      !CU_add_test(suite, "lexer_test_no_skip_str_squot", &lexer_test_no_skip_str_squot) ||
      !CU_add_test(suite, "lexer_test_str_dquot", &lexer_test_str_dquot) ||
      !CU_add_test(suite, "lexer_test_no_skip_str_dquot", &lexer_test_no_skip_str_dquot) ||
      !CU_add_test(suite, "lexer_test_unget_no_skip_str_squot", &lexer_test_unget_no_skip_str_squot))
  {
     CU_cleanup_registry();
     return CU_get_error();
  }

  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  CU_cleanup_registry();
  return CU_get_error();
}
