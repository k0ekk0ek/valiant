/* config.c - simple program to test vt_lexer_t and vt_config_t */

/* system includes */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "../src/vt_error.h"
#include "../src/vt_config.h"

/* prototypes */
void usage (const char *);

void
usage (const char *prog)
{
  const char *fmt =
  "Usage: %s CONFIG_FILE";

  vt_info (fmt, prog);
  exit (1);
}

int
main (int argc, char *argv[])
{
  char *prog;
  int err;
  vt_config_t *cfg;
  vt_config_def_t opts[] = {
    VT_CONFIG_BOOL("boolean_supported", 0, 0),
    VT_CONFIG_INT("int_supported", 0, 0),
    VT_CONFIG_FLOAT("float_supported", 0, 0),
    VT_CONFIG_STR_SQUOT("string_supported", "/", "", 0),
    VT_CONFIG_END()
  };

  if ((prog = strrchr (argv[0], '/')))
    prog++;
  else
    prog = argv[0];

  if (argc < 2)
    usage (prog);

  cfg = vt_config_parse_file (opts, argv[1], &err);
  if (! cfg)
    vt_fatal ("failed to parse config\n");

  bool bln = vt_config_sec_getbool (cfg, "boolean_supported", &err);
  int lng = vt_config_sec_getint (cfg, "int_supported", &err);
  float dbl = vt_config_sec_getfloat (cfg, "float_supported", &err);
  char *str = vt_config_sec_getstr (cfg, "string_supported", &err);

  vt_info ("boolean_supported: %s", bln ? "true" : "false");
  vt_info ("int_supported: %d", lng);
  vt_info ("float_supported: %f", dbl);
  vt_info ("string_supported: %s", str);

  return 0;
}
