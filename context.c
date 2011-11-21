/* system includes */
#include <confuse.h>
#include <errno.h>
#include <stdlib.h>

/* valiant includes */
#include "conf.h"
#include "context.h"
#include "map.h"

vt_map_list_t *
vt_context_map_list_create (cfg_t *cfg, vt_map_type_t *map_types, vt_errno_t *err)
{
  cfg_t *sec;
  char *type;
  int i, j, n;
  vt_map_list_t *list;
  vt_map_t *map;

  if (! (list = vt_map_list_create (err)))
    return NULL;

  for (i = 0, n = cfg_size (cfg, "map"); i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "map", i)) &&
        (str = cfg_getstr (sec, "type")))
    {
      for (j = 0; map_types[j]; j++) {
        if (strcmp (str, map_types[j]->name) == 0) {
          if (! (map = map_types[j]->create_map_func (cfg, err)))
            goto FAILURE;
          if (vt_map_list_add_map (list, map, err) < 0) {
            map->destroy_func (map);
            goto FAILURE;
          }
          break;
        }
      }
    }
  }

  return list;
FAILURE:
  vt_map_list_destroy (list);
  return NULL;
}

vt_stage_t *
vt_context_stage_create (vt_stats_t *stats,
                         cfg_t *sec,
                         vt_check_type_t *check_types,
                         vt_map_list_t *maps,
                         vt_errno_t *err)
{
  cfg_t *subsec;
  char *type;
  int i, n;
  int pos, ret;
  vt_check_t *check;
  vt_stage_t *stage;

  if (! (stage = vt_stage_create (maps, sec, &ret))) {
    vt_set_errno (err, ret);
    goto FAILURE;
  }

  for (i = 0, n = cfg_size (sec, "check"); i < n; i++) {
    if ((subsec = cfg_getnsec (sec, "check", i)) &&
        (type = cfg_getstr (subsec, "type")))
    {
      for (j = 0, check = NULL; ! check && check_types[j]; j++) {
        if (strcmp (type, check_types[j]->name) == 0) {
          if (! (check = check_types[j]->create_check_func (maps, subsec, &ret))) {
            vt_set_errno (err, ret);
            goto FAILURE;
          }
        }
      }

      if (! check) {
        vt_set_errno (err, VT_ERR_BADCFG);
        vt_error ("%s (%d): invalid option 'type' with value '%s' in section 'check'", __func__, __LINE__, type);
        goto FAILURE;
      }
      if (vt_stage_add_check (stage, check, &ret) < 0) {
        vt_set_errno (err, ret);
        check->destroy_func (check);
        goto FAILURE;
      }
      if ((pos = vt_stats_add_cntr (stats, check, &ret)) < 0) {
        vt_set_errno (err, ret);
        goto FAILURE;
      }
    }
  }

  if (i < 1) {
    vt_set_errno (err, VT_ERR_BADCFG);
    vt_error ("%s (%d): invalid section 'stage'");
    goto FAILURE;
  }

  return stage;
FAILURE:
  vt_stage_destroy (stage);
  return NULL;
}

vt_slist_t *
vt_context_stages_create (vt_stats_t *stats,
                          cfg_t *cfg,
                          vt_check_type_t *check_types,
                          vt_map_list_t *maps,
                          vt_errno_t *err)
{
  cfg_t *sec;
  float min, max;
  int i, n;
  int ret;
  vt_slist_t *cur, *next, *stages = NULL;
  vt_stage_t *stage = NULL;

  for (i = 0, n = cfg_size (cfg, "stage"); i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "stage", i))) {
      if (! (stage = vt_context_stage_create (stats, sec, check_types, maps, &ret))) {
        vt_set_errno (err, ret);
        goto FAILURE;
      }
      if (! (next = vt_slist_append (stages, stage))) {
        vt_set_errno (err, ENOMEM);
        vt_stage_destroy (stage);
        goto FAILURE;
      }
      if (stages) {
        stages = next;
      }

      vt_stage_update (stage);
    }
  }

  /* calculate stage weights */
  for (cur = stages; cur; cur = cur->next) {
    stage = (vt_stage_t *)cur->data;
    min = 0;
    max = 0;

    for (next = cur->next; next; next = next->next) {
      min += ((vt_stage_t *)next->data)->min_weight;
      max += ((vt_stage_t *)next->data)->max_weight;
    }

    stage->min_gained_weight = min;
    stage->max_gained_weight = max;
  }

  return stages;
FAILURE:
  vt_slist_free (stages, &vt_stage_destroy, 0);
  return NULL;
}

vt_context_t *
vt_context_create (cfg_t *cfg,
                   vt_map_type_t *map_types,
                   vt_check_type_t *check_types,
                   vt_errno_t **err)
{
  cfg_t *sec;
  char *str;
  int i, j, k, n, o;
  unsigned int id;
  vt_context_t *ctx = NULL;
  vt_map_t *map;
  vt_slist_t *root, *next;

  if (! (ctx = calloc (1, sizeof (vt_context_t)))) {
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    goto FAILURE;
  }

  if (! (ctx->bind_address = vt_cfg_getstrdup (cfg, "bind_address")) ||
      ! (ctx->pid_file = vt_cfg_getstrdup (cfg, "pid_file")) ||
      ! (ctx->syslog_ident = vt_cfg_getstrdup (cfg, "syslog_identity")) ||
      ! (ctx->block_resp = vt_cfg_getstrdup (cfg, "block_response")) ||
      ! (ctx->delay_resp = vt_cfg_getstrdup (cfg, "delay_response")) ||
      ! (ctx->error_resp = vt_cfg_getstrdup (cfg, "error_response")))
  {
    if (errno == EINVAL)
      vt_set_errno (err, VT_ERR_BADCFG);
    else
      vt_set_errno (err, VT_ERR_NOMEM);
    goto FAILURE;
  }

  /* NOTE: syslog_facility and syslog_priority validated by vt_cfg_parse */
  ctx->port = cfg_getint (cfg, "port");
  ctx->syslog_facility = vt_log_facility (cfg_getstr (cfg, "syslog_facility"));
  ctx->syslog_prio = vt_log_priority (cfg_getstr (cfg, "syslog_priority"));
  ctx->block_threshold = cfg_getfloat (cfg, "block_threshold");
  ctx->delay_threshold = cfg_getfloat (cfg, "delay_threshold");

  if (! (ctx->stats = vt_stats_create (err))) ||
      ! (ctx->maps = vt_context_map_list_create (cfg, map_types, err))) ||
      ! (ctx->stages = vt_context_stages_create (ctx->stats, cfg, check_types, ctx->maps, err)))
    goto FAILURE;

  return ctx;
FAILURE:
  (void)vt_context_destroy (ctx);
  return NULL;
}

int
vt_context_destroy (vt_context_t *ctx)
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
    if (ctx->stages) {
      for (cur = ctx->stages, next = NULL; cur; cur = next) {
        vt_stage_destroy ((vt_stage_t *)cur->data, 1 /* checks */);
        next = cur->next;
        free (cur);
      }
    }

    /* stats */
    if (ctx->stats) {
      vt_stats_destroy (ctx->stats);
      free (ctx->stats);
    }

    /* maps */
    if (ctx->maps) {
      vt_map_list_destory (ctx->maps, 1 /* maps */);
      free (ctx->maps);
    }

    memset (ctx, 0, sizeof (vt_context_t));
    return 0;
  }
  return EINVAL;
}
