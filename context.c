/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "conf.h"
#include "context.h"
#include "error.h"
#include "stage.h"

vt_map_list_t *
vt_context_map_list_create (cfg_t *cfg, vt_map_type_t **map_types,
  vt_error_t *err)
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
        (type = cfg_getstr (sec, "type")))
    {
      for (j = 0; map_types[j]; j++) {
        if (strcmp (type, map_types[j]->name) == 0) {
          if (! (map = map_types[j]->create_map_func (sec, err)))
            goto FAILURE;
          if (vt_map_list_add_map (list, map, err) < 0) {
            (void)map->destroy_func (map, NULL);
            goto FAILURE;
          }
          break;
        }
      }
    }
  }

  return list;
FAILURE:
  (void)vt_map_list_destroy (list, 1, NULL);
  return NULL;
}

vt_stage_t *
vt_context_stage_create (vt_stats_t *stats,
                         cfg_t *sec,
                         vt_check_type_t **check_types,
                         vt_map_list_t *maps,
                         vt_error_t *err)
{
  cfg_t *subsec;
  char *type;
  int i, j, n;
  int pos;
  vt_error_t lerr;
  vt_check_t *check;
  vt_stage_t *stage;

  if (! (stage = vt_stage_create (maps, sec, &lerr))) {
    vt_set_error (err, lerr);
    goto FAILURE;
  }
vt_error ("%s (%d)", __func__, __LINE__);
  for (i = 0, n = cfg_size (sec, "check"); i < n; i++) {
    if ((subsec = cfg_getnsec (sec, "check", i)) &&
        (type = cfg_getstr (subsec, "type")))
    {
vt_error ("%s (%d): type: %s", __func__, __LINE__, type);
      for (j = 0, check = NULL; ! check && check_types[j]; j++) {
        if (strcmp (type, check_types[j]->name) == 0) {
          if (! (check = check_types[j]->create_check_func (maps, subsec, &lerr))) {
            vt_set_error (err, lerr);
vt_error ("%s (%d): error: %d", __func__, __LINE__, lerr);
            goto FAILURE;
          }
        }
      }

      if (! check) {
        vt_set_error (err, VT_ERR_BADCFG);
        vt_error ("%s (%d): invalid option 'type' with value '%s' in section 'check'", __func__, __LINE__, type);
        goto FAILURE;
      }
      if (vt_stage_add_check (stage, check, &lerr) < 0) {
        vt_set_error (err, lerr);
        (void)check->destroy_func (check, NULL);
        goto FAILURE;
      }
      if ((pos = vt_stats_add_cntr (stats, check->name, &lerr)) < 0) {
        vt_set_error (err, lerr);
        goto FAILURE;
      }
    }
  }

  if (i < 1) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s (%d): invalid section 'stage'");
    goto FAILURE;
  }

  return stage;
FAILURE:
  (void)vt_stage_destroy (stage, 1, NULL);
  return NULL;
}

vt_slist_t *
vt_context_stages_create (vt_stats_t *stats,
                          cfg_t *cfg,
                          vt_check_type_t **check_types,
                          vt_map_list_t *maps,
                          vt_error_t *err)
{
  cfg_t *sec;
  float min, max;
  int i, n;
  int ret;
  vt_slist_t *cur, *next, *stages = NULL;
  vt_stage_t *stage = NULL;

  stages = NULL;

  for (i = 0, n = cfg_size (cfg, "stage"); i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "stage", i))) {
      if (! (stage = vt_context_stage_create (stats, sec, check_types, maps, &ret))) {
        vt_set_error (err, ret);
        goto FAILURE;
      }
vt_error ("%s (%d)", __func__, __LINE__);
      if (! (next = vt_slist_append (stages, stage))) {
        vt_set_error (err, ENOMEM);
        (void)vt_stage_destroy (stage, 1, NULL);
        goto FAILURE;
      }
      if (! stages) {
vt_error ("%s (%d)", __func__, __LINE__);
        stages = next;
      }
vt_error ("%s (%d)", __func__, __LINE__);
      vt_stage_update (stage);
    }
  }
vt_error ("%s (%d)", __func__, __LINE__);
  /* calculate stage weights */
  for (cur = stages; cur; cur = cur->next) {
    stage = (vt_stage_t *)cur->data;
    min = 0;
    max = 0;

    for (next = cur->next; next; next = next->next) {
      min += ((vt_stage_t *)next->data)->min_weight;
      max += ((vt_stage_t *)next->data)->max_weight;
    }

    stage->min_weight_diff = min;
    stage->max_weight_diff = max;
  }

  return stages;
FAILURE:
  vt_slist_free (stages, &vt_stage_destroy_slist, 0);
  return NULL;
}

vt_context_t *
vt_context_create (cfg_t *cfg,
                   vt_map_type_t **map_types,
                   vt_check_type_t **check_types,
                   vt_error_t *err)
{
  char *str;
  vt_context_t *ctx = NULL;

  if (! (ctx = calloc (1, sizeof (vt_context_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    goto FAILURE;
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
    goto FAILURE;
  }

  /* NOTE: syslog_facility and syslog_priority validated by vt_cfg_parse */
  ctx->syslog_facility = vt_syslog_facility (cfg_getstr (cfg, "syslog_facility"));
  ctx->syslog_prio = vt_syslog_priority (cfg_getstr (cfg, "syslog_priority"));
  ctx->block_threshold = vt_check_weight (cfg_getfloat (cfg, "block_threshold"));
  ctx->delay_threshold = vt_check_weight (cfg_getfloat (cfg, "delay_threshold"));
fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  if (! (ctx->stats = vt_stats_create (err)) ||
      ! (ctx->maps = vt_context_map_list_create (cfg, map_types, err)) ||
      ! (ctx->stages = vt_context_stages_create (ctx->stats, cfg, check_types, ctx->maps, err)))
    {
vt_error ("%s (%d): error: %d", __func__, __LINE__, err);
    goto FAILURE;
    }

  return ctx;
FAILURE:
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
    if (ctx->stages) {
      for (cur = ctx->stages, next = NULL; cur; cur = next) {
        (void)vt_stage_destroy ((vt_stage_t *)cur->data, 1 /* checks */, NULL);
        next = cur->next;
        free (cur);
      }
    }

    /* stats */
    if (ctx->stats) {
      (void)vt_stats_destroy (ctx->stats, NULL);
      free (ctx->stats);
    }

    /* maps */
    if (ctx->maps) {
      vt_map_list_destroy (ctx->maps, 1 /* maps */, NULL);
      //free (ctx->maps);
    }

    memset (ctx, 0, sizeof (vt_context_t));
    return 0;
  }
  return EINVAL;
}
