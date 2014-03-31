#include <stdio.h>
#include <unistr.h>
#include <valiant/string.h>
#include <CUnit/Basic.h>

#define DIGIT "0123456789"
#define XDIGIT DIGIT "abcdef" "ABCDEF"
#define LOWER "abcdefghijklmnopqrstuvwxyz"
#define UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define ALPHA LOWER UPPER
#define ALNUM DIGIT ALPHA

const uint8_t digit[] = DIGIT;
const uint8_t xdigit[] = XDIGIT;
const uint8_t lower[] = LOWER;
const uint8_t upper[] = UPPER;
const uint8_t alpha[] = ALPHA;
const uint8_t alnum[] = ALNUM;

#define FOO "foo"
#define BAR "bar"

const uint8_t foo[] = FOO;
const uint8_t bar[] = BAR;
const uint8_t foobar[] = FOO BAR;

static void
string_test_isalnum_c (void)
{
  int res = 0;
  uint8_t *ptr;

  for (ptr = (uint8_t *)alnum; *ptr && res == 0; ptr++) {
    if (!isalnum_c (*ptr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (!isalnum_c ('\0'));
}

static void
string_test_isalpha_c (void)
{
  int res = 0;
  uint8_t *ptr;

  for (ptr = (uint8_t *)alpha; *ptr && res == 0; ptr++) {
    if (!isalpha_c (*ptr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (!isalpha_c ('\0'));
}

static void
string_test_iscntrl_c (void)
{
  int chr, res = 0;

  /* In C locale all characters between 0x00 and 0x1f, plus 0x7f, are
     considered control characters */

  for (chr = 0x00; chr <= 0x1f; chr++) {
    if (!iscntrl_c (chr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (iscntrl_c (0x7f));
  CU_ASSERT (!iscntrl_c (' '));
}

static void
string_test_isdigit_c (void)
{
  int res = 0;
  uint8_t *ptr;

  for (ptr = (uint8_t *)digit; *ptr && res == 0; ptr++) {
    if (!isdigit_c (*ptr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (!isdigit_c ('\0'));
}

static void
string_test_isgraph_c (void)
{
  int chr, res = 0;

  /* In C locale all printing characters (isprint_c), except for space, are
     considered graphic characters */

  for (chr = 0x21; chr < 0x7f && res == 0; chr++) {
    if (!isalnum_c (chr) && !isgraph_c (chr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (!isgraph_c (0x20));
  CU_ASSERT (!isgraph_c (0x7f));
}

static void
string_test_islower_c (void)
{
  int res = 0;
  uint8_t *ptr;

  for (ptr = (uint8_t *)lower; *ptr && res == 1; ptr++) {
    if (!islower_c (*ptr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (!islower_c ('\0'));
}

static void
string_test_isprint_c (void)
{
  int chr, res = 0;

  /* In C locale all characters greater than 0x1f, except 0x7f, are
     considered printing characters. */

  for (chr = 0x20; chr < 0x7f && res == 0; chr++) {
    if (!isprint_c (chr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (!isprint_c (0x1f));
  CU_ASSERT (!isprint_c (0x7f));
}

static void
string_test_ispunct_c (void)
{
  int chr, res = 0;

  /* In C locale all graphic characters (isgraph_c) that are not
     alphanumeric (isalnum_c) are considered punctuation characters */

  for (chr = 0x21; chr < 0x7f && res == 0; chr++) {
    if (!isalnum_c (chr) && !ispunct_c (chr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (!ispunct_c (0x1f));
  CU_ASSERT (!ispunct_c (0x7f));
}

static void
string_test_isspace_c (void)
{
  CU_ASSERT (isspace_c (' '));
  CU_ASSERT (isspace_c ('\t'));
  CU_ASSERT (isspace_c ('\n'));
  CU_ASSERT (isspace_c ('\v'));
  CU_ASSERT (isspace_c ('\f'));
  CU_ASSERT (isspace_c ('\r'));
}

static void
string_test_isupper_c (void)
{
  int res = 0;
  uint8_t *ptr;

  for (ptr = (uint8_t *)upper; *ptr && res == 0; ptr++) {
    if (!isupper_c (*ptr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
}

static void
string_test_isxdigit_c (void)
{
  int res = 0;
  uint8_t *ptr;

  for (ptr = (uint8_t *)xdigit; *ptr && res == 0; ptr++) {
    if (!isxdigit_c (*ptr)) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
}

static void
string_test_strcasecmp_c (void)
{
  CU_ASSERT (strcasecmp_c (lower, lower) == 0);
  CU_ASSERT (strcasecmp_c (lower, upper) == 0);
  CU_ASSERT (strcasecmp_c (upper, upper) == 0);
  CU_ASSERT (strcasecmp_c (lower, digit) > 0);
  CU_ASSERT (strcasecmp_c (digit, lower) < 0);
}

static void
string_test_strncasecmp_c (void)
{
  CU_ASSERT (strncasecmp_c (lower, lower, sizeof (lower)) == 0);
  CU_ASSERT (strncasecmp_c (lower, upper, sizeof (lower)) == 0);
  CU_ASSERT (strncasecmp_c (upper, upper, sizeof (upper)) == 0);
  CU_ASSERT (strncasecmp_c (lower, digit, sizeof (digit)) > 0);
  CU_ASSERT (strncasecmp_c (digit, lower, sizeof (digit)) < 0);
  CU_ASSERT (strncasecmp_c (upper, digit, sizeof (digit)) > 0);
  CU_ASSERT (strncasecmp_c (digit, upper, sizeof (digit)) < 0);
}

static void
string_test_todigit_c (void)
{
  int res = 0;
  uint8_t *ptr;

  for (ptr = (uint8_t *)digit; *ptr && res == 0; ptr++) {
    res = todigit_c (*ptr) - (ptr - digit);
  }

  CU_ASSERT (res == 0);
}

static void
string_test_tolower_c (void)
{
  int res = 0;
  size_t pos;

  for (pos = 0; upper[pos] != '\0' && res == 0; pos++) {
    if (tolower_c (upper[pos]) != lower[pos]) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
}

static void
string_test_toupper_c (void)
{
  int res = 0;
  size_t pos;

  for (pos = 0; lower[pos] != '\0' && res == 0; pos++) {
    if (toupper_c (lower[pos]) != upper[pos]) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
}

static void
string_test_strtol_c (void)
{
  long lng;

  CU_ASSERT (strtol_c (&lng, (const uint8_t *)"+101", NULL, 0) == 0 && lng == 101);
  CU_ASSERT (strtol_c (&lng, (const uint8_t *)"-101", NULL, 0) == 0 && lng == -101);
  CU_ASSERT (strtol_c (&lng, (const uint8_t *)"0101", NULL, 0) == 0 && lng == 0101);
  CU_ASSERT (strtol_c (&lng, (const uint8_t *)"-0101", NULL, 0) == 0 && lng == -0101);
  CU_ASSERT (strtol_c (&lng, (const uint8_t *)"0x101", NULL, 0) == 0 && lng == 0x101);
  CU_ASSERT (strtol_c (&lng, (const uint8_t *)"-0x101", NULL, 0) == 0 && lng == -0x101);
}

static void
string_test_strtod_c (void)
{
  double dbl;

  CU_ASSERT (strtod_c (&dbl, (const uint8_t *)"+1.01", NULL) == 0 && dbl == 1.01);
  CU_ASSERT (strtod_c (&dbl, (const uint8_t *)"-1.01", NULL) == 0 && dbl == -1.01);
}

static void
string_test_string_create (void)
{
  string_t *str;
  uint8_t *ptr;

  CU_ASSERT (string_create (&str, NULL, 0) == 0);
  CU_ASSERT (string_get_buffer (str) == NULL);
  CU_ASSERT (string_get_length (str) == 0);
  string_destroy (str);
  CU_ASSERT (string_create (&str, foobar, sizeof (foobar) - 1) == 0);
  CU_ASSERT ((ptr = string_get_buffer (str)) != NULL);
  CU_ASSERT (strcmp ((char *)ptr, (char *const)foobar) == 0);
}

static void
string_test_string_limit (void)
{
  string_t str;
  uint8_t *ptr = NULL;

  CU_ASSERT (string_construct (&str, NULL, 0) == 0);
  CU_ASSERT (string_set_limit (&str, SIZE_MAX) != 0);
  CU_ASSERT (string_set_limit (&str, sizeof (foobar) - 4) == 0);
  CU_ASSERT (string_copy (&str, foobar, sizeof (foobar) - 1) != 0);
  CU_ASSERT (string_copy (&str, foobar, sizeof (foobar) - 4) == 0);
  CU_ASSERT (string_append (&str, foobar, sizeof (foobar) - 4) != 0);
  CU_ASSERT ((ptr = string_get_buffer (&str)) != NULL);
  CU_ASSERT (strlen ((char *)ptr) == 3);

  string_destruct (&str);
}

static void
string_test_string_copy (void)
{
  string_t str;
  uint8_t *ptr = NULL;

  CU_ASSERT (string_construct (&str, NULL, 0) == 0);
  CU_ASSERT (string_copy (&str, foobar, sizeof (foobar) - 1) == 0);
  CU_ASSERT ((ptr = string_get_buffer (&str)) != NULL);
  CU_ASSERT (strcmp ((char *)ptr, (char *const)foobar) == 0);

  string_destruct (&str);
}

static void
string_test_string_append (void)
{
  string_t str;
  uint8_t *ptr = NULL;

  CU_ASSERT (string_construct (&str, foo, sizeof (foo) - 1) == 0);
  CU_ASSERT (string_append (&str, bar, sizeof (bar) - 1) == 0);
  CU_ASSERT ((ptr = string_get_buffer (&str)) != NULL);
  CU_ASSERT (strcmp ((char *)ptr, (char *const)foobar) == 0);

  string_destruct (&str);
}

static void
string_test_string_find_char (void)
{
  string_t str;

  CU_ASSERT (string_construct (&str, foobar, sizeof (foobar) - 1) == 0);
  CU_ASSERT (string_find_char (&str, 'F') == -1);
  CU_ASSERT (string_find_char (&str, 'f') == 0);
  CU_ASSERT (string_find_char (&str, 'b') == 3);
}

static void
string_test_string_iterator (void)
{
  int res = 0;
  size_t pos, len = sizeof (foobar) - 1;
  string_t str;
  ucs4_t chr;

  CU_ASSERT (string_construct (&str, foobar, len) == 0);

  for (pos = 0;
       pos < len && res == 0;
       pos++, string_get_next_char (NULL, &str))
  {
    if (string_get_char (&chr, &str) != 0 || foobar[pos] != chr) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (string_get_state (&str) == len - 1);

  for (pos = 0;
       pos < len && res == 0;
       pos++, string_get_previous_char (NULL, &str))
  {
    if (string_get_char (&chr, &str) != 0 || foobar[(len - 1) - pos] != chr) {
      res = 1;
    }
  }

  CU_ASSERT (res == 0);
  CU_ASSERT (string_get_state (&str) == 0);
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

  if (!CU_add_test(suite, "isalnum_c", &string_test_isalnum_c) ||
      !CU_add_test(suite, "isalpha_c", &string_test_isalpha_c) ||
      !CU_add_test(suite, "iscntrl_c", &string_test_iscntrl_c) ||
      !CU_add_test(suite, "isdigit_c", &string_test_isdigit_c) ||
      !CU_add_test(suite, "isgraph_c", &string_test_isgraph_c) ||
      !CU_add_test(suite, "islower_c", &string_test_islower_c) ||
      !CU_add_test(suite, "isprint_c", &string_test_isprint_c) ||
      !CU_add_test(suite, "ispunct_c", &string_test_ispunct_c) ||
      !CU_add_test(suite, "isspace_c", &string_test_isspace_c) ||
      !CU_add_test(suite, "isupper_c", &string_test_isupper_c) ||
      !CU_add_test(suite, "isxdigit_c", &string_test_isxdigit_c) ||
      !CU_add_test(suite, "strcasecmp_c", &string_test_strcasecmp_c) ||
      !CU_add_test(suite, "strncasecmp_c", &string_test_strncasecmp_c) ||
      !CU_add_test(suite, "todigit_c", &string_test_todigit_c) ||
      !CU_add_test(suite, "tolower_c", &string_test_tolower_c) ||
      !CU_add_test(suite, "toupper_c", &string_test_toupper_c) ||
      !CU_add_test(suite, "strtol_c", &string_test_strtol_c) ||
      !CU_add_test(suite, "strtod_c", &string_test_strtod_c) ||
      !CU_add_test(suite, "string creation", &string_test_string_create) ||
      !CU_add_test(suite, "string size limit", &string_test_string_limit) ||
      !CU_add_test(suite, "string_copy", &string_test_string_copy) ||
      !CU_add_test(suite, "string_append", &string_test_string_append) ||
      !CU_add_test(suite, "string_find_char", &string_test_string_find_char) ||
      !CU_add_test(suite, "string iterator", &string_test_string_iterator))
  {
     CU_cleanup_registry();
     return CU_get_error();
  }

  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  CU_cleanup_registry();
  return CU_get_error();
}
