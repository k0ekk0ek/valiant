/* system includes */
#include <confuse.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "conf.h"
#include "context.h"

/* prototypes */
int vt_context_get_dict_pos (vt_context_t *, const char *);

int
vt_context_get_dict_pos (vt_context_t *ctx, const char *dict)
{
  int i;

  for (i = 0; i < ctx->ndicts; i++) {
    if (strcmp (ctx->dicts[i]->name, dict) == 0)
      return i;
  }
  return -1;
}

int
vt_context_dicts_init (vt_context_t *ctx,
                       vt_dict_type_t **types,
                       cfg_t *cfg,
                       vt_error_t *err)
{
  cfg_t *dict_sec, *type_sec;
  char *title, *dict_type;
  int dict_pos, ndicts, ntypes, type_pos;
  vt_dict_t *dict, **dicts;
  vt_dict_type_t **type;

  if (! (ndicts = cfg_size (cfg, "dict"))) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: no dicts", __func__);
    return -1;
  }
  if (! (dicts = calloc (ndicts, sizeof (vt_dict_t *)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return -1;
  }

  for (dict_pos = 0; dict_pos < ndicts; dict_pos++)
  {
    dict_sec = cfg_getnsec (cfg, "dict", dict_pos);
    if (dict_sec && (dict_type = cfg_getstr (dict_sec, "type"))) {
      /* lookup generic configuration for given dictionary type */
      title = (char *)cfg_title (dict_sec);
      type = NULL;
      type_sec = NULL;

      for (type_pos = 0, ntypes = cfg_size (cfg, "type");
           type_pos < ntypes;
           type_pos++)
      {
        type_sec = cfg_getnsec (cfg, "type", type_pos);
        if (type_sec && strcmp (dict_type, cfg_title (type_sec)) == 0)
          break;
      }

      if (type_pos >= ntypes)
        type_sec = NULL;

      for (type = types; type; type++) {
        if (strcmp ((*type)->name, dict_type) == 0)
          break;
      }

      if (! type) /* programmer error */
        vt_panic ("%s: unknown type %s for dict %s",
          __func__, dict_type, title);

      if (! (dict = vt_dict_create (*type, type_sec, dict_sec, err)))
        goto failure;

      vt_debug ("%s: dict: %s, pos: %d", __func__, title, dict_pos);
      *(dicts + dict_pos) = dict;
    }
  }

  *(dicts + ndicts) = NULL; /* null terminate */
  ctx->dicts = dicts;
  ctx->ndicts = ndicts;

  return 0;
failure:
  for (dict_pos = 0; dict_pos < ndicts; dict_pos++) {
    if ((dict = *(dicts + dict_pos)))
      (void)vt_dict_destroy (dict, NULL);
  }
  free (dicts);
  return -1;
}

vt_check_t *
vt_check_create (vt_context_t *ctx, cfg_t *cfg, vt_error_t *err)
{
  int i, n;
  char *dict;
  vt_check_t *check;

  if (! (check = calloc (1, sizeof (vt_check_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  dict = cfg_getstr (cfg, "dict");

  if ((check->dict = vt_context_get_dict_pos (ctx, dict)) < 0) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: reference to unknown dict %s", __func__, dict);
    goto failure;
  }

  /* support dependencies */
  if ((check->ndepends = cfg_size (cfg, "depends"))) {
    if (! (check->depends = calloc (check->ndepends, sizeof (int)))) {
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: calloc: %s", __func__, strerror (errno));
      goto failure;
    }
  }

  for (i = 0; i < check->ndepends; i++) {
    if ((dict = cfg_getnstr (cfg, "depends", i))) {
      check->depends[i] = vt_context_get_dict_pos (ctx, dict);
      if (check->depends[i] < 0) {
        vt_set_error (err, VT_ERR_BADCFG);
        vt_error ("%s: reference to unknown dict %s", __func__, dict);
        goto failure;
      }
    }
  }

  return check;
failure:
  (void)vt_check_destroy (check, NULL);
  return NULL;
}

int
vt_check_destroy (vt_check_t *check, vt_error_t *err)
{
  if (check) {
    if (check->depends)
      free (check->depends);
    free (check);
  }
  return 0;
}

vt_stage_t *
vt_stage_create (vt_context_t *ctx, cfg_t *cfg, vt_error_t *err)
{
  cfg_t *sec;
  char *dict;
  int i, n;
  int invert, use_depends;
  vt_stage_t *stage;

  if (! (stage = calloc (1, sizeof (vt_stage_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;;
  }

  stage->nchecks = cfg_size (cfg, "check");

  if (! (stage->checks = calloc (stage->nchecks, sizeof (int)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  for (i = 0; i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "check", i))) {
      if (! (stage->checks[i] = vt_check_create (ctx, sec, err)))
        goto failure;
    }
  }

  /* support dependencies */
  if (strcmp (cfg_name (cfg), "stage") == 0) {
    if ((stage->ndepends = cfg_size (cfg, "depends"))) {
      if (! (stage->depends = calloc (stage->ndepends, sizeof (int)))) {
        vt_set_error (err, VT_ERR_NOMEM);
        vt_error ("%s: calloc: %s", __func__, strerror (errno));
        goto failure;
      }
    }

    for (i = 0; i < stage->ndepends; i++) {
      if ((dict = cfg_getnstr (cfg, "depends", i))) {
        stage->depends[i] = vt_context_get_dict_pos (ctx, dict);
        if (stage->depends[i] < 0) {
          vt_set_error (err, VT_ERR_BADCFG);
          vt_error ("%s: reference to unknown dict %s", __func__, dict);
          goto failure;
        }
      }
    }
  }

  return stage;
failure:
  (void)vt_stage_destroy (stage, NULL);
  return NULL;
}

int
vt_stage_destroy (vt_stage_t *stage, vt_error_t *err)
{
  int i, ret;

  if (stage) {
    if (stage->checks) {
      for (i = 0; i < stage->nchecks; i++) {
        if (stage->checks[i] && vt_check_destroy (stage->checks[i], err) != 0)
          return ret;
      }
      free (stage->checks);
    }
    if (stage->depends)
      free (stage->depends);
    free (stage);
  }
  return 0;
}

int
vt_context_stages_init (vt_context_t *ctx, cfg_t *cfg, vt_error_t *err)
{
  cfg_t *sec;
  vt_stage_t **stages;
  int i, n;
  int use_stages;

  if ((n = cfg_size (cfg, "stage"))) {
    use_stages = 1;
  } else if (cfg_size (cfg, "check")) {
    n = 1;
    use_stages = 0;
  } else {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: specify at least one check", __func__);
    return -1;
  }

  if (! (stages = calloc (n + 1, sizeof (vt_stage_t *)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return -1;
  }

  if (use_stages) {
    for (i = 0; i < n; i++) {
      sec = cfg_getnsec (cfg, "stage", i);
      if (! (stages[i] = vt_stage_create (ctx, sec, err)))
        goto failure;
    }
  } else {
    if (! (stages[0] = vt_stage_create (ctx, cfg, err)))
      goto failure;
  }

  ctx->stages = stages;
  ctx->nstages = n;
  return 0;
failure:
  for (i = 0; i < n; i++) {
    if (stages[i])
      (void)vt_stage_destroy (stages[i], NULL);
  }
  free (stages);
  return -1;
}

vt_context_t *
vt_context_create (vt_dict_type_t **types, cfg_t *cfg, vt_error_t *err)
{
  char *str;
  int i, n;
  vt_context_t *ctx = NULL;

  if (! (ctx = calloc (1, sizeof (vt_context_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    goto failure;
  }

  if (! (ctx->port = vt_cfg_getstr_dup (cfg, "port")) ||
      ! (ctx->bind_address = vt_cfg_getstr_dup (cfg, "bind_address")) ||
      ! (ctx->pid_file = vt_cfg_getstr_dup (cfg, "pid_file")) ||
      ! (ctx->syslog_ident = vt_cfg_getstr_dup (cfg, "syslog_identity")) ||
      ! (ctx->allow_resp = vt_cfg_getstr_dup (cfg, "allow_response")) ||
      ! (ctx->block_resp = vt_cfg_getstr_dup (cfg, "block_response")) ||
      ! (ctx->delay_resp = vt_cfg_getstr_dup (cfg, "delay_response")) ||
      ! (ctx->error_resp = vt_cfg_getstr_dup (cfg, "error_response")))
  {
    if (errno == EINVAL)
      vt_set_error (err, VT_ERR_BADCFG);
    else
      vt_set_error (err, VT_ERR_NOMEM);
    goto failure;
  }

  /* NOTE: syslog_facility and syslog_priority validated by vt_cfg_parse */
  ctx->syslog_facility = vt_syslog_facility (cfg_getstr (cfg, "syslog_facility"));
  ctx->syslog_prio = vt_syslog_priority (cfg_getstr (cfg, "syslog_priority"));
  ctx->block_threshold = cfg_getfloat (cfg, "block_threshold");
  ctx->delay_threshold = cfg_getfloat (cfg, "delay_threshold");

  if (vt_context_dicts_init (ctx, types, cfg, err) != 0 ||
      vt_context_stages_init (ctx, cfg, err) != 0)
    goto failure;

  return ctx;
failure:
  (void)vt_context_destroy (ctx, NULL);
  return NULL;
}

int
vt_context_destroy (vt_context_t *ctx, vt_error_t *err)
{
  //vt_slist_t *cur, *next;
  int i, n;

  if (ctx) {
    if (ctx->bind_address)
      free (ctx->bind_address);
    if (ctx->pid_file)
      free (ctx->pid_file);
    if (ctx->syslog_ident)
      free (ctx->syslog_ident);
    if (ctx->block_resp)
      free (ctx->block_resp);
    if (ctx->delay_resp)
      free (ctx->delay_resp);
    if (ctx->error_resp)
      free (ctx->error_resp);

    /* stages/checks */
    //if (ctx->stages) {
    //  for (cur = ctx->stages, next = NULL; cur; cur = next) {
    //    (void)vt_stage_destroy ((vt_stage_t *)cur->data, 1 /* checks */, NULL);
    //    next = cur->next;
    //    free (cur);
    //  }
    //}
    if (ctx->stages) {
      for (i = 0; i < ctx->nstages; i++) {
        if (ctx->stages[i])
          (void)vt_stage_destroy (ctx->stages[i], NULL);
      }
      free (ctx->stages);
    }

    if (ctx->dicts) {
      for (i = 0; i < ctx->ndicts; i++) {
        if (ctx->dicts[i])
          (void)vt_dict_destroy (ctx->dicts[i], NULL);
      }
      free (ctx->dicts);
    }

    /* stats */
    //if (ctx->stats) {
    //  (void)vt_stats_destroy (ctx->stats, NULL);
    //  free (ctx->stats);
    //}

    /* maps */
    //if (ctx->maps) {
    //  vt_map_list_destroy (ctx->maps, 1 /* maps */, NULL);
    //  //free (ctx->maps);
    //}

    memset (ctx, 0, sizeof (vt_context_t));
    return 0;
  }
  return EINVAL;
}
