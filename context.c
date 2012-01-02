/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "conf.h"
#include "context.h"
#include "error.h"
#include "stage.h"

/* prototypes */
int vt_context_get_dict_pos (const char *);

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
  vt_dict_type_t *type;

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
    dict_sec = cfg_getnsec (cfg, "dict", i);
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

      if (! (dict = vt_dict_create (type, type_sec, dict_sec, err)))
        goto failure;

      vt_debug ("%s: dict %s", __func__, title);
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
vt_check_create (vt_context_t *ctx, cfg_t *sec, vt_error_t *err)
{
  cfg_t *subsec;
  int i, n;
  int pos, *require;
  char *name;
  vt_check_t *check;

  if (! (check = calloc (1, sizeof (vt_check_t)))) {
    vt_set_error (VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return NULL;
  }

  if ((name = cfg_name (sec)) && strcmp (name, "check") == 0) {
    if (! (dict = cfg_getstr (sec, "dict"))) {
      vt_set_error (err, VT_ERR_BADCFG);
      vt_error ("%s: check without dict", __func__);
      goto failure;
    }
    if ((pos = vt_context_get_dict_pos (ctx, dict)) < 0) {
      vt_set_error (err, VT_ERR_BADCFG);
      vt_error ("%s: check references undefined dict %s", __func__, dict);
      goto failure;
    }

    check->dict = pos;
  } else {
    check->dict = -1;
  }

  n = cfg_size (sec, "require");
  if ((check->ndepends = n) && ! (check->depends = calloc (n, sizeof (int)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  for (i = 0; i < n; i++) {
    if ((dict = cfg_getnstr (cfg, "require", i))) {
      if ((pos = vt_context_get_dict_pos (ctx, dict)) < 0) {
        vt_set_error (err, VT_ERR_BADCFG);
        vt_error ("%s: %s references unknown dict %s", __func__, name, dict);
        goto failure;
      }

      *(check->depends + i) = pos;
    }
  }

  n = cfg_size (sec, "check");
  if ((check->nchecks = n) && ! (check->checks = calloc (n, sizeof (vt_check_t *)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }

  for (i = 0; i < n; i++) {
    if ((subsec = cfg_getnsec (sec, "check"))) {
      if (! (*(check->checks + i) = vt_check_create (ctx, subsec, err)))
        goto failure;
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
  int i, n;

  if (check) {
    if (check->checks) {
      for (i = 0, n = check->nchecks; i < n; i++) {
        if ((check->check + i) && vt_check_destroy (*(check->check + i), err) != 0)
          return -1;
      }
      free (check->checks);
    }
    for (check->require)
      free (check->require);
    free (check);
  }
  return 0;
}

int
vt_context_checks_init (vt_context_t *ctx, cfg_t *cfg, vt_error_t *err)
{
  char *name;
  cfg_t *sec;
  cfg_opt_t *opt;
  int i, n;
  int no_stages;

  if ((n = cfg_getsize (cfg, "stage"))) {
    no_stages = 0;
    /* checks may be defined without stage, but only if no stages are defined */
    if (cfg_getsize (cfg, "check") > 0) {
      vt_set_error (VT_ERR_BADCFG);
      vt_error ("%s: one or more checks defined outside stage", __func__);
      return -1;
    }
  } else if (cfg_getsize (cfg, "check")) {
    no_stages = 1;
    n = 1;
  } else {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: no check or stage definitions", __func__);
    return -1;
  }

  ctx->nchecks = n;
  ctx->checks = calloc (n, sizeof (vt_check_t *));
  if (! ctx->checks) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return -1;
  }

  if (no_stages) {
    if (! (*(ctx->checks) = vt_check_create (ctx, cfg, err)))
      goto failure;
  } else {
    for (i = 0; i < n; i++) {
      if ((sec = cfg_getnsec (cfg, "stage", i))) {
        if ((*(ctx->checks + i) = vt_check_create (ctx, cfg, err)))
          goto failure;
      }
    }
  }

  return 0;
failure:
  if (ctx->checks) {
    for (i = 0; i < ctx->nchecks; i++) {
      if (*(ctx->checks + i))
        (void)vt_check_destroy (*(ctx->checks + i));
    }
    free (ctx->checks);
  }
  ctx->checks = NULL;
  ctx->nchecks = 0;
  return -1;
}

vt_context_t *
vt_context_create (cfg_t *cfg, vt_dict_type_t **types, vt_error_t *err)
{
  char *str;
  int i, n;
  vt_context_t *ctx = NULL;

  if (! (ctx = calloc (1, sizeof (vt_context_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    goto failure;
  }

  if (! (ctx->port = vt_cfg_getstrdup (cfg, "port")) ||
      ! (ctx->bind_address = vt_cfg_getstrdup (cfg, "bind_address")) ||
      ! (ctx->pid_file = vt_cfg_getstrdup (cfg, "pid_file")) ||
      ! (ctx->syslog_ident = vt_cfg_getstrdup (cfg, "syslog_identity")) ||
      ! (ctx->allow_resp = vt_cfg_getstrdup (cfg, "allow_response")) ||
      ! (ctx->block_resp = vt_cfg_getstrdup (cfg, "block_response")) ||
      ! (ctx->delay_resp = vt_cfg_getstrdup (cfg, "delay_response")) ||
      ! (ctx->error_resp = vt_cfg_getstrdup (cfg, "error_response")))
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

  if (vt_context_dicts_init (ctx, types, err) != 0 ||
      vt_context_checks_init (ctx, err) != 0)
    goto failure;

  return ctx;
failure:
  (void)vt_context_destroy (ctx, NULL);
  return NULL;
}

int
vt_context_destroy (vt_context_t *ctx, vt_error_t *err)
{
  vt_slist_t *cur, *next;

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
