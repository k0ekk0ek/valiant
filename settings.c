/* system includes */
#include <confuse.h>
#include <stdbool.h>

/* valiant includes */
#include "consts.h"
#include "request.h"
#include "settings.h"

unsigned int
vt_cfg_size_opts (cfg_t *cfg)
{
  unsigned int i = 0;

  if (cfg->opts) {
    for (i=0; (cfg->opts)[i].type != CFGT_NONE; i++)
      ;
  }

  return i;
}

cfg_opt_t *
vt_cfg_getnopt (cfg_t *cfg, unsigned int index)
{
  cfg_opt_t *opt = NULL;
  unsigned int n = vt_cfg_size_opts (cfg);

  if (n && n >= index) {
    opt = &((cfg->opts)[index]);
  }

  return opt;
}

/** Returns if an option was given in the config file.
 */
unsigned int
vt_cfg_opt_parsed (cfg_opt_t *opt)
{
  if (opt && opt->nvalues && ! (opt->flags & CFGF_RESET))
    return 1;
  return 0;
}

int
vt_cfg_validate_check_rbl (cfg_t *cfg, cfg_opt_t *opt)
{
	bool exists;
  cfg_opt_t *subopt;
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *p, *title, *zone;
  char *name;
  char *names[] = {"in", "ipv4", "ipv6", "type", "zone", "weight", "maps",
                   "default", NULL};
  unsigned int i, j, n;

  title = (char *)cfg_title (sec);

  if (! (zone = cfg_getstr (sec, "zone"))) {
    cfg_error (cfg, "zone missing in check %s", title);
    return -1;
  }

  /* I don't know of any blocklists that use idn in their hostnames, so I'll
     just do this the old fashioned way. */
  for (p=zone; *p; p++) {
    if ((*p <  '0' && *p  > '9') &&
        (*p <  'a' && *p  > 'z') &&
        (*p <  'A' && *p  > 'Z') &&
         *p != '.' && *p != '-')
    {
      cfg_error (cfg, "invalid zone in check %s", title);
      return -1;
    }
  }

  /* Make sure no dnsbl type unsupported options are specified. */
  for (i=0, n=vt_cfg_size_opts (sec); i < n; i++) {
    subopt = vt_cfg_getnopt (sec, i);
    name = (char *)cfg_opt_name (subopt);
    exists = false;

    for (j=0; ! exists && names[j]; j++) {
      if (strcmp (name, names[j]) == 0)
        exists = true;
    }

    if (! exists && vt_cfg_opt_parsed (subopt)) {
      cfg_error (cfg, "invalid option or section '%s' in check %s", name, title);
      return -1;
    }
  }

  return 0; /* valid */
}

int
vt_cfg_validate_check_str (cfg_t *cfg, cfg_opt_t *opt)
{
  bool exists;
  cfg_opt_t *subopt;
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *mbr, *name, *title;
  char *names[] = {"default", "maps", "member", "negate", "nocase", "pattern",
                   "type", "weight", NULL};
  unsigned int i, j, n;
  vt_request_mbr_t mbrid;

  title = (char *)cfg_title (sec);

  /* Verify user specified a valid member. */
  if (! (mbr = (char *)cfg_getstr (sec, "member"))) {
    cfg_error (cfg, "member missing in check %s", title);
    return -1;
  }
  if (vt_request_mbrtoid (&mbrid, mbr) != VT_SUCCESS) {
    cfg_error (cfg, "invalid member '%s' in check %s", mbr, title);
    return -1;
  }

    /* Make sure no dnsbl type unsupported options are specified. */
  for (i=0, n=vt_cfg_size_opts (sec); i < n; i++) {
    subopt = vt_cfg_getnopt (sec, i);
    name = (char *)cfg_opt_name (subopt);
    exists = false;

    for (j=0; ! exists && names[j]; j++) {
      if (strcmp (name, names[j]) == 0)
        exists = true;
    }

    if (! exists && vt_cfg_opt_parsed (subopt)) {
      cfg_error (cfg, "invalid option or section '%s' in check %s", name, title);
      return -1;
    }
  }

  return 0; /* valid */
}

int
vt_cfg_validate_check (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *title, *type;

  if (! (title = (char *)cfg_title (sec))) {
    cfg_error (cfg, "check without name");
    return -1;
  }
  if (! (type = cfg_getstr (sec, "type"))) {
    cfg_error (cfg, "type missing in check %s", title);
    return -1;
  }

  if (strcmp (type, "dnsbl") == 0 ||
      strcmp (type, "rhsbl") == 0)
    return vt_cfg_validate_check_rbl (cfg, opt);
  if (strcmp (type, "pcre")  == 0 ||
      strcmp (type, "str")   == 0)
    return vt_cfg_validate_check_str (cfg, opt);

  cfg_error (cfg, "invalid type '%s' in check %s", type, title);
  return -1;
}

int
vt_cfg_validate_stage (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);

  if (! cfg_size (sec, "check")) {
    cfg_error (cfg, "stage without check");
    return -1;
  }
  return 0;
}

cfg_t *
vt_cfg_parse (const char *path)
{
  cfg_opt_t check_in_opts[] = {
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t check_opts[] = {
    CFG_SEC ("in", (cfg_opt_t *) check_in_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_STR ("member", 0, CFGF_NODEFAULT),
    CFG_STR ("pattern", 0, CFGF_NODEFAULT),
    CFG_BOOL ("negate", cfg_false, CFGF_NONE),
    CFG_BOOL ("nocase", cfg_false, CFGF_NONE),
    CFG_STR ("type", 0, CFGF_NODEFAULT),
    CFG_STR ("zone", 0, CFGF_NODEFAULT),
    CFG_BOOL ("ipv4", cfg_true, CFGF_NONE),
    CFG_BOOL ("ipv6", cfg_false, CFGF_NONE),
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_STR_LIST ("maps", 0, CFGF_NONE),
    CFG_STR ("default", "do", CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t stage_opts[] = {
    CFG_SEC ("check", check_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_STR_LIST ("maps", 0, CFGF_NONE),
    CFG_STR ("default", "do", CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t map_opts[] = {
    CFG_STR ("type", 0, CFGF_NODEFAULT),
    CFG_STR ("member", 0, CFGF_NODEFAULT),
    CFG_STR ("path", 0, CFGF_NODEFAULT),
    CFG_END ()
  };

  cfg_opt_t type_opts[] = {
    CFG_INT ("max_idle_threads", 10, CFGF_NONE),
    CFG_INT ("max_tasks", 200, CFGF_NONE),
    CFG_INT ("max_threads", 100, CFGF_NONE),
    CFG_INT ("min_threads", 10, CFGF_NONE),
    CFG_END ()    
  };

  cfg_opt_t opts[] = {
    CFG_SEC ("stage", stage_opts, CFGF_MULTI),
    CFG_SEC ("map", map_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("type", type_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_END ()
  };

  cfg_t *cfg = cfg_init (opts, CFGF_NONE);
  cfg_set_validate_func(cfg, "stage|check", vt_cfg_validate_check);
  cfg_set_validate_func(cfg, "stage", vt_cfg_validate_stage);

  switch(cfg_parse(cfg, path)) {
    case CFG_FILE_ERROR:
      printf("warning: configuration file '%s' could not be read: %s\n",
             path, strerror(errno));
      printf("continuing with default values...\n\n");
    case CFG_SUCCESS:
      break;
    case CFG_PARSE_ERROR:
      return NULL;
  }

  return cfg;
}
