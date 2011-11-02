
int
vt_cfg_validate_check (cfg_t cfg, cfg_opt_t *opt)
{
  // IMPLEMENT
  return 0;
}

int
vt_cfg_parse (cfg_t **dest, const char *path)
{
  cfg_opt_t check_in_opts[] = {
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t check_opts[] = {
    CFG_SEC ("in", (cfg_opt_t *) check_in_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_STR_LIST ("include", 0, CFGF_NONE),
    CFG_STR_LIST ("exclude", 0, CFGF_NONE),
    CFG_STR ("member", 0, CFGF_NODEFAULT),
    CFG_STR ("pattern", 0, CFGF_NODEFAULT),
    CFG_BOOL ("negate", cfg_false, CFGF_NONE),
    CFG_BOOL ("nocase", cfg_false, CFGF_NONE),
    CFG_STR ("type", 0, CFGF_NODEFAULT),
    CFG_STR ("zone", 0, CFGF_NODEFAULT),
    CFG_BOOL ("ipv4", cfg_true, CFGF_NONE),
    CFG_BOOL ("ipv6", cfg_false, CFGF_NONE),
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
  };

  cfg_opt_t map_opts[] = {
    CFG_STR ("type", 0, CFGF_NODEFAULT),
    CFG_STR ("member", 0, CFGF_NODEFAULT),
    CFG_STR ("path", 0, CFGF_NODEFAULT),
    CFG_END ()
  };

  cfg_opt_t stage_opts[] = {
    CFG_SEC ("check", check_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_STR_LIST ("include", 0, CFGF_NONE),
    CFG_STR_LIST ("exclude", 0, CFGF_NONE),
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
    CFG_SEC ("stage", stage_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("map", map_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("type", type_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_END ()
  };

  cfg_t *cfg = cfg_init (opts, CFGF_NONE);

  cfg_parse (cfg, path);
  return VT_SUCCESS;
}
