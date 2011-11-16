/* system includes */
#include <confuse.h>
#include <stdbool.h>
#include <string.h>


//#include <syslog.h>

/* valiant includes */
#include "consts.h"
#include "request.h"
#include "settings.h"
#include "utils.h"

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

char *
vt_cfg_getstrdup (cfg_t *cfg, const char *name)
{
  char *s1, *s2;

  if (! (s1 = cfg_getstr (cfg, name)))
    return NULL;
  if (! (s2 = strdup (s1)))
    return NULL;
  return s2;
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

int
vt_cfg_validate_bind_address (cfg_t *cfg, cfg_opt_t *opt)
{
  // IMPLEMENT
  return 0;
}

int
vt_cfg_validate_port (cfg_t *cfg, cfg_opt_t *opt)
{
  // IMPLEMENT
  return 0;
}

int
vt_cfg_validate_log_facility (cfg_t *cfg, cfg_opt_t *opt)
{
  char *str;

  if (! (str = cfg_getnstr (opt, 0)))
    return -1;
  return (vt_log_facility (str) >= 0) ? 0 : -1;
}

int
vt_cfg_validate_log_priority (cfg_t *cfg, cfg_opt_t *opt)
{
  char *str;

  if (! (str = cfg_getnstr (opt, 0)))
    return -1;
  return (vt_log_priority (str) >= 0) ? 0 : -1;
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

  //cfg_opt_t policy_opts[] = {
  //  CFG_SEC ("block", block
  //};

  cfg_opt_t opts[] = {
    CFG_SEC ("stage", stage_opts, CFGF_MULTI),
    CFG_SEC ("map", map_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("type", type_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_STR ("pid_file", 0, CFGF_NODEFAULT),
    CFG_STR ("log_ident", VT_LOG_IDENT, CFG_NONE),
    CFG_STR ("log_facility", VT_LOG_FACILITY, CFGF_NONE),
    CFG_STR ("log_level", 0, CFGF_NODEFAULT),
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

int
vt_context_init (vt_context_t *ctx,
                 cfg_t *cfg,
                 vt_map_type_t *map_types,
                 vt_check_type_t *check_types)
{
  cfg_t *sec;
  char *str;
  int err;
  int i, j, k, n, o; /* counters */
  unsigned int id;
  vt_map_t *map;
  vt_slist_t *root, *next;

  assert (ctx);
  memset (ctx, 0, sizeof (vt_context_t));

  if (! (ctx->bind_address = vt_cfg_getstrdup (cfg, "bind_address")) ||
      ! (ctx->port = vt_cfg_getstrdup (cfg, "port")) ||
      ! (ctx->pid_file = vt_cfg_getstrdup (cfg, "pid_file")) ||
      ! (ctx->log_ident = vt_cfg_getstrdup (cfg, "log_identity")))
  {
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }

  ctx->log_facility = vt_log_facility (cfg_getstr (cfg, "log_facility"));
  ctx->log_prio = vt_log_priority (cfg_getstr (cfg, "log_priority"));

  if (ctx->log_facility < 0 || ctx->log_prio < 0)
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }

  /* init stats */
  if (! (ctx->stats = malloc0 (sizeof (vt_stats_t)))) {
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  if ((err = vt_stats_init (ctx->stats)) != VT_SUCCESS) {
    goto FAILURE;
  }
  /* counters are added below when adding checks */

  /* init maps */
  if (! (ctx->maps = malloc0 (sizeof (vt_map_list_t)))) {
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  if ((err = vt_map_list_init (ctx->maps)) != VT_SUCCESS) {
    goto FAILURE;
  }

  for (i = 0, n = cfg_size (cfg, "map"); i < n; i++) {
    if ((sec = cfg_getnsec ((cfg_t *)cfg, "map", i)) &&
        (str = cfg_getstr ((cfg_t *)sec, "type")))
    {
      for (j = 0; map_types[j]; j++) {
        if (strcmp (str, map_types[j]->name) == 0) {
          if ((err = map_types[j]->create_map_func (&map, sec)) != VT_SUCCESS)
            goto FAILURE;
          if ((err = vt_map_list_add_map (&id, ctx->maps, map)) != VT_SUCCESS)
            goto FAILURE;
          break;
        }
      }
    }
  }

  /* init stages/checks */
  for (i = 0, n = cfg_size (cfg, "stage"); i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "stage", i))) {

      /* init stage */
      if (! (stage = malloc0 (sizeof (vt_stage_t)))) {
        err = VT_ERR_NOMEM;
        goto FAILURE;
      }
      if ((err = vt_stage_init (stage)) != VT_SUCCESS) {
        goto FAILURE;
      }

      // assign the maps 'n stuff to check
      // add stage to list

      for (j = 0, o = cfg_size (sec, "check"); j < o; j++) {
        if ((subsec = cfg_getnsec (sec, "check", j)) &&
            (type = cfg_getstr (subsec, "type")))
        {
          /* init check */
          for (k = 0, check = NULL; ! check && check_types[k]; k++) {
            if (strcmp (type, check_types[j]->name) == 0) {
              err = check_types[j]->create_check_func (&check, set, subsec);
              if (err != VT_SUCCESS) {
                /* cleanup */
                goto FAILURE;
              }
            }
          }

          if (! check) {
            // big fat error
          }
          // vt_map_list_ids
          if ((err = vt_stats_add_cntr (&check->id, stats, check)) != VT_SUCCESS) {
            check->destroy_func (check);
            goto FAILURE;
          }
          if ((err = vt_stage_add_check (stage, check)) != VT_SUCCESS) {
            goto FAILURE;
          }
          // assign the maps 'n stuff to check
          // add it to the stats 'n stuff
          // add it to the stage 'n stuff
        }
      }

      //

      // if (! check) {
      // vt_error ("%s: invalid check type %s", __func__, type);
      // err = VT_ERR_INVAL;
      // goto FAILURE;
      // }
      ////fprintf (stderr, "%s (%d): check name: %s\n", __func__, __LINE__, check->name);
      //if ((ret = vt_stage_set_check (stage, check)) != VT_SUCCESS) {
      // err = ret;
      // goto FAILURE;
      // }
      ////fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
      //}
      //  }
      //}

      //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
      //if ((ret = vt_cfg_stage_create (&stage, types, set, sec)) != VT_SUCCESS) {
      //  err = ret;
      //  goto FAILURE;
      //}
      //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
      //vt_stage_lineup (stage); /* align checks and calculate weights */
      //
      //if ((next = vt_slist_append (root, stage)) == NULL) {
      //  err = VT_ERR_NOMEM;
      //  goto FAILURE;
      //}
      //if (root == NULL) {
      //  root = next;
      //}
    }
  }

  return VT_SUCCESS;

FAILURE:
  (void)vt_context_deinit (ctx);
  return err;
}

int
vt_context_deinit (vt_context_t *ctx)
{
  if (ctx) {
    if (ctx->pid_file)
      free (ctx->pid_file);
    if (ctx->log_ident)
      free (ctx->log_ident);

    /* stages */
    /* checks */
    /* maps */  // FIXME: should destroy set?!?!

    memset (ctx, 0, sizeof (vt_context_t));
  }
  return VT_SUCCESS;
}





















































