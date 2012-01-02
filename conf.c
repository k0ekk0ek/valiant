/* system includes */
#include <errno.h>
#include <stdarg.h>
#include <string.h>


/* valiant includes */
#include "request.h"
#include "conf.h"
#include "error.h"
#include <syslog.h>
#include "utils.h"

//unsigned int
//vt_cfg_size_opts (cfg_t *cfg)
//{
//  unsigned int i = 0;
//
//  if (cfg->opts) {
//    for (i=0; (cfg->opts)[i].type != CFGT_NONE; i++)
//      ;
//  }
//
//  return i;
//}

//cfg_opt_t *
//vt_cfg_getnopt (cfg_t *cfg, unsigned int index)
//{
//  cfg_opt_t *opt = NULL;
//  unsigned int n = vt_cfg_size_opts (cfg);
//
//  if (n && n >= index) {
//    opt = &((cfg->opts)[index]);
//  }
//
//  return opt;
//}

unsigned int
vt_cfg_opt_isset (cfg_opt_t *opt)
{
  if (opt && opt->nvalues && ! (opt->flags & CFGF_RESET))
    return 1;
  return 0;
}

//char *
//vt_cfg_getstrdup (cfg_t *cfg, const char *name)
//{
//  char *s1, *s2;
//
//  if ((s1 = cfg_getstr (cfg, name))) {
//    s2 = strdup (s1);
//  } else {
//    s2 = NULL;
//    errno = EINVAL;
//  }
//
//  return s2;
//}

//char *
//vt_cfg_titledup (cfg_t *cfg)
//{
//  char *s1, *s2 = NULL;
//
//  if ((s1 = (char *)cfg_title (cfg))) {
//    s2 = strdup (s1);
//  } else {
//    s2 = NULL;
//    errno = EINVAL;
//  }
//
//  return s2;
//}

void
vt_cfg_error (cfg_t *cfg, const char *fmt, va_list ap)
{
  vt_log (LOG_ERR, fmt, ap);
}

int
vt_cfg_validate_check (cfg_t *cfg, cfg_opt_t *opt)
{
  // IMPLEMENT
}

int
vt_cfg_validate_dict_opts (cfg_t *sec, const char **opts, unsigned int nopts)
{
  cfg_opt_t *opt;
  char *name, *title;
  int i, n, found;

  title = (char *)cfg_title (sec);

  for (i = 0, n = vt_cfg_size_opts (sec); i < n; i++) {
    opt = vt_cfg_getnopt (sec, i);
    name = (char *)cfg_opt_name (subopt);
    found = 0;

    for (j = 0; ! found && j < nopts && opts[j]; j++) {
      if (strcmp (name, opts[j]) == 0)
        found = 1;
    }

    if (! found && opt->nvalues && ! (opt->flags & CFGF_RESET)) {
      cfg_error (cfg, "invalid %s '%s' in dict section '%s'",
        ((opt->type == CFGT_SEC) ? "section" : "option"), name, title);
      return -1;
    }
  }

  return 0;
}

int
vt_cfg_validate_check_rbl (cfg_t *cfg, cfg_opt_t *opt)
{
	cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *ptr, *title, *zone;
  char *opts[] = {"in", "ipv4", "ipv6", "type", "zone", "weight", NULL};
  int nopts = 7;

  title = (char *)cfg_title (sec);

  if (! (zone = cfg_getstr (sec, "zone"))) {
    cfg_error (cfg, "missing option 'zone' in dict section '%s'", title);
    return -1;
  }

  for (ptr = zone; *ptr; ptr++) {
    if (! isalnum (*ptr) && *ptr != '.' && *ptr != '-') {
      cfg_error (cfg, "invalid option 'zone' in dict section '%s'", title);
      return -1;
    }
  }

  return vt_cfg_validate_dict_opts (sec, opts, nopts);
}

int
vt_cfg_validate_dict_hash (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *member, *path;
  char *title = (char *)cfg_title (sec);
  vt_request_member_t memberid;

  if (! (member = cfg_getstr (sec, "member")) || strlen (member) == 0) {
    cfg_error (cfg, "missing option 'member' in dict section '%s'", title);
    return -1;
  }
  if ((memberid = vt_request_mbrtoid (member)) == VT_REQUEST_MEMBER_NONE) {
    cfg_error (cfg, "invalid option 'member' in dict section '%s'", title);
    return -1;
  }
  if (! (path = cfg_getstr (sec, "path")) || strlen (path) == 0) {
    cfg_error (cfg, "missing option 'path' in dict section '%s'", title);
    return -1;
  }

  return vt_cfg_validate_dict_opts (sec, opts, nopts);
}

int
vt_cfg_validate_dict_pcre (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *member;
  char *title = (char *)cfg_title (sec);
  char *opts[] = {"member", "invert", "nocase", "path", "pattern", "type",
                  "weight", NULL};
  int n, nopts = 7;
  vt_request_member_t memberid;

  if ((member = cfg_getstr (sec, "member")) == NULL) {
    cfg_error (cfg, "missing option 'member' in dict section '%s'", title);
    return -1;
  }
  if ((memberid = vt_request_mbrtoid (member)) == VT_REQUEST_MEMBER_NONE) {
    cfg_error (cfg, "invalid option 'member' in dict section '%s'", title);
    return -1;
  }

  /* user must either define pattern or path, never both */
  if ((opt = cfg_getopt (sec, "pattern")) && vt_cfg_opt_isset (opt))
    n++;
  if ((opt = cfg_getopt (sec, "path")) && vt_cfg_opt_isset (opt))
    n++;

  if (n > 1) {
    cfg_error (cfg, "invalid combination of options 'path' and 'pattern' in dict section '%s'", title);
    return -1;
  }
  if (n < 1) {
    cfg_error (cfg, "missing option 'pattern' in dict section '%s'", title);
    return -1;
  }

  return vt_cfg_validate_dict_opts (sec, opts, nopts);
}

int
vt_cfg_validate_dict_spf (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *opts[] = {"pass", "fail", "soft_fail", "neutral", "none", "perm_error",
                  "temp_error", "type", "weight", NULL};
  int nopts = 9;

  return vt_cfg_validate_dict_opts (sec, opts, nopts);
}

int
vt_cfg_validate_dict_str (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *member, *title;
  char *opts[] = {"member", "invert", "nocase", "pattern", "type",
                  "weight", NULL};
  int nopts = 6;
  vt_request_member_t memberid;

  title = (char *)cfg_title (sec);
  if (! (member = cfg_getstr (sec, "member")) || strlen (member) == 0) {
    cfg_error (cfg, "missing option 'member' in dict section '%s'", title);
    return -1;
  }
  if ((memberid = vt_request_mbrtoid) == VT_REQUEST_NONE) {
    cfg_error (cfg, "invalid option 'member' in dict section '%s'", title);
    return -1;
  }

  return vt_cfg_validate_dict_opts (sec, opts, nopts);
}

int
vt_cfg_validate_dict (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);
  char *title, *type;

  if (! (title = (char *)cfg_title (sec))) {
    cfg_error (cfg, "missing title for dict section");
    return -1;
  }
  if (! (type = cfg_getstr (sec, "type")) || strlen (type) == 0) {
    cfg_error (cfg, "missing type in dict section '%s'", title);
    return -1;
  }

  if (strcmp (type, "dnsbl") == 0 ||
      strcmp (type, "rhsbl") == 0)
    return vt_cfg_validate_dict_rbl (cfg, opt);
  if (strcmp (type, "hash") == 0)
    return vt_cfg_validate_dict_hash (cfg, opt);
  if (strcmp (type, "pcre") == 0)
    return vt_cfg_validate_dict_pcre (cfg, opt);
  if (strcmp (type, "spf") == 0)
    return vt_cfg_validate_dict_spf (cfg, opt);
  if (strcmp (type, "str") == 0)
    return vt_cfg_validate_dict_str (cfg, opt);

  cfg_error (cfg, "invalid type '%s' in dict section '%s'", type, title);
  return -1;
}

int
vt_cfg_validate_stage (cfg_t *cfg, cfg_opt_t *opt)
{
  cfg_t *sec = cfg_opt_getnsec (opt, cfg_opt_size (opt) - 1);

  if (! cfg_size (sec, "check")) {
    cfg_error (cfg, "missing section 'check' in stage section");
    return -1;
  }
  return 0;
}

int
vt_cfg_validate_syslog_facility (cfg_t *cfg, cfg_opt_t *opt)
{
  char *str;

  if (! (str = cfg_opt_getnstr (opt, 0)))
    return -1;
  return (vt_syslog_facility (str) >= 0) ? 0 : -1;
}

int
vt_cfg_validate_syslog_priority (cfg_t *cfg, cfg_opt_t *opt)
{
  char *str;

  if (! (str = cfg_opt_getnstr (opt, 0)))
    return -1;
  return (vt_syslog_priority (str) >= 0) ? 0 : -1;
}

cfg_t *
vt_cfg_parse (const char *path)
{
  cfg_opt_t dict_in_opts[] = {
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t dict_spf_opts[] = {
    CFG_FLOAT ("weight", 0.0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t dict_opts[] = {
    CFG_SEC ("in", dict_in_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("pass", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("fail", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("soft_fail", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("neutral", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("none", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("perm_error", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("temp_error", dict_spf_opts, CFGF_NONE),
    CFG_STR ("path", 0, CFGF_NODEFAULT),
    CFG_STR ("member", 0, CFGF_NODEFAULT),
    CFG_STR ("pattern", 0, CFGF_NODEFAULT),
    CFG_BOOL ("invert", cfg_false, CFGF_NONE),
    CFG_BOOL ("nocase", cfg_false, CFGF_NONE),
    CFG_STR ("type", 0, CFGF_NODEFAULT),
    CFG_STR ("zone", 0, CFGF_NODEFAULT),
    CFG_BOOL ("ipv4", cfg_true, CFGF_NONE),
    CFG_BOOL ("ipv6", cfg_false, CFGF_NONE),
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_INT ("max_idle_threads", 10, CFGF_NONE),
    CFG_INT ("max_queued", 200, CFGF_NONE),
    CFG_INT ("max_threads", 100, CFGF_NONE),
    CFG_INT ("min_threads", 10, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t type_opts[] = {
    CFG_INT ("max_idle_threads", 10, CFGF_NONE),
    CFG_INT ("max_queued", 200, CFGF_NONE),
    CFG_INT ("max_threads", 100, CFGF_NONE),
    CFG_INT ("min_threads", 10, CFGF_NONE),
    CFG_END ()    
  };

  cfg_opt_t check_opts[] = {
    CFG_STR ("dict", 0, CFGF_NODEFAULT),
    CFG_STR_LIST ("depends", 0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t stage_opts[] = {
    CFG_SEC ("check", check_opts, CFGF_MULTI),
    CFG_STR_LIST ("depends", 0, CFGF_NONE),
    CFG_END ()
  }

  cfg_opt_t opts[] = {
    CFG_SEC ("dict", dict_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("type", type_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("check", check_opts, CFGF_MULTI),
    CFG_SEC ("stage", stage_opts, CFGF_MULTI),
    CFG_STR ("bind_address", VT_CFG_BIND_ADDR, CFGF_NONE),
    CFG_STR ("port", VT_CFG_PORT, CFGF_NONE),
    CFG_INT ("max_idle_threads", 10, CFGF_NONE),
    CFG_INT ("max_queued", 200, CFGF_NONE),
    CFG_INT ("max_threads", 100, CFGF_NONE),
    CFG_INT ("min_threads", 10, CFGF_NONE),
    CFG_STR ("allow_response", VT_CFG_ALLOW_RESP, CFGF_NONE),
    CFG_FLOAT ("block_threshold", VT_CFG_BLOCK_THRESHOLD, CFGF_NONE),
    CFG_STR ("block_response", VT_CFG_BLOCK_RESP, CFGF_NONE),
    CFG_FLOAT ("delay_threshold", VT_CFG_DELAY_THRESHOLD, CFGF_NONE),
    CFG_STR ("delay_response", VT_CFG_DELAY_RESP, CFGF_NONE),
    CFG_STR ("error_response", VT_CFG_ERROR_RESP, CFGF_NONE),
    CFG_STR ("pid_file", VT_CFG_PID_FILE, CFGF_NONE),
    CFG_STR ("syslog_identity", VT_CFG_SYSLOG_IDENT, CFGF_NONE),
    CFG_STR ("syslog_facility", VT_CFG_SYSLOG_FACILITY, CFGF_NONE),
    CFG_STR ("syslog_priority", VT_CFG_SYSLOG_PRIO, CFGF_NONE),
    CFG_END ()
  };

  cfg_t *cfg = cfg_init (opts, CFGF_NONE);
  cfg_set_error_function (cfg, vt_cfg_error);
  cfg_set_validate_func (cfg, "dict", vt_cfg_validate_dict);
  cfg_set_validate_func (cfg, "check", vt_cfg_validate_check);
  cfg_set_validate_func (cfg, "stage|check", vt_cfg_validate_check);
  cfg_set_validate_func (cfg, "stage", vt_cfg_validate_stage);
  cfg_set_validate_func (cfg, "syslog_facility", vt_cfg_validate_syslog_facility);
  cfg_set_validate_func (cfg, "syslog_priority", vt_cfg_validate_syslog_priority);

  // IMPLEMENT
  //switch(cfg_parse(cfg, path)) {
  //  case CFG_FILE_ERROR:
  //  case CFG_PARSE_ERROR:
  //    return NULL;
  //}

  return cfg;
}
