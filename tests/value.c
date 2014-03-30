#include <errno.h>
#include <stdlib.h>
#include <valiant/value.h>
#include <CUnit/Basic.h>

static value_t val = { 0 };

static int
value_suite_init (void)
{
  return 0;
}

static int
value_suite_deinit (void)
{
  value_clear (&val);
  return 0;
}

static CU_BOOL
value_is_type (value_t *val, value_type_t type)
{
  bool bln;
  double dbl;
  long lng;
  ucs4_t chr;
  uint8_t *str = NULL;
  CU_BOOL res = CU_TRUE;

  if (value_get_type (val) == type) {
    if (type != VALUE_BOOL && value_get_bool (&bln, val) != EINVAL) {
      res = CU_FALSE;
    } else if (type != VALUE_CHAR && value_get_char (&chr, val) != EINVAL) {
      res = CU_FALSE;
    } else if (type != VALUE_LONG && value_get_long (&lng, val) != EINVAL) {
      res = CU_FALSE;
    } else if (type != VALUE_DOUBLE && value_get_double (&dbl, val) != EINVAL) {
      res = CU_FALSE;
    } else if (type != VALUE_STR && value_get_str (&str, val) != EINVAL) {
      res = CU_FALSE;
    }
  } else {
    res = CU_FALSE;
  }

  if (str != NULL) {
    free (str);
  }

  return res;
}

static void
value_test_null (void)
{
  bool bln;
  double dbl;
  long lng;
  ucs4_t chr;
  uint8_t *str = NULL;

  value_clear (&val);
  CU_ASSERT (value_is_type (&val, VALUE_NULL) == CU_TRUE);
}

static void
value_test_bool (void)
{
  bool bln;

  value_set_bool (&val, true);
  CU_ASSERT (value_is_type (&val, VALUE_BOOL) == CU_TRUE);
  CU_ASSERT (value_get_bool (&bln, &val) == 0 && bln == true);
}

static void
value_test_char (void)
{
  ucs4_t chr;

  value_set_char (&val, 'a');
  CU_ASSERT (value_is_type (&val, VALUE_CHAR) == CU_TRUE);
  CU_ASSERT (value_get_char (&chr, &val) == 0 && chr == 'a');
}

static void
value_test_long (void)
{
  long lng;

  value_set_long (&val, 101);
  CU_ASSERT (value_is_type (&val, VALUE_LONG) == CU_TRUE);
  CU_ASSERT (value_get_long (&lng, &val) == 0 && lng == 101);
}

static void
value_test_double (void)
{
  double dbl;

  value_set_double (&val, 1.01);
  CU_ASSERT (value_is_type (&val, VALUE_DOUBLE) == CU_TRUE);
  CU_ASSERT (value_get_double (&dbl, &val) == 0 && dbl == 1.01);
}

static void
value_test_str (void)
{
  const uint8_t str[] = "string with UTF-8 char ö";
  uint8_t *ptr = NULL;

  CU_ASSERT (value_set_str (&val, str, 0) == 0);
  CU_ASSERT (value_is_type (&val, VALUE_STR) == CU_TRUE);
  CU_ASSERT (value_get_str (&ptr, &val) == 0 && ptr && strcmp (ptr, str) == 0);

  if (ptr != NULL) {
    free (ptr);
  }
}

int
main (int argc, char *argv[])
{
  CU_pSuite suite = NULL;

  if (CUE_SUCCESS != CU_initialize_registry())
     return CU_get_error();

  suite = CU_add_suite("value", &value_suite_init, &value_suite_deinit);
  if (NULL == suite) {
     CU_cleanup_registry();
     return CU_get_error();
  }

  if (!CU_add_test(suite, "test of empty value", &value_test_null) ||
      !CU_add_test(suite, "test of boolean value", &value_test_bool) ||
      !CU_add_test(suite, "test of char value", &value_test_char) ||
      !CU_add_test(suite, "test of long value", &value_test_long) ||
      !CU_add_test(suite, "test of double value", &value_test_double) ||
      !CU_add_test(suite, "test of string value", &value_test_str))
  {
     CU_cleanup_registry();
     return CU_get_error();
  }

  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  CU_cleanup_registry();
  return CU_get_error();
}
