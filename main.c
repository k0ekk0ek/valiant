/* system includes */
#include <confuse.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "check.h"
#include "check_dnsbl.h"
#include "check_rhsbl.h"
#include "check_str.h"
#include "check_pcre.h"
#include "consts.h"
#include "map.h"
#include "map_bdb.h"
#include "request.h"
#include "settings.h"
#include "score.h"
#include "slist.h"
#include "stage.h"

int
vt_cfg_types_init (vt_check_type_t *types[], const cfg_t *cfg)
{
  cfg_t *sec;
  char *name, *title;
  int i, n, done;
  vt_check_type_t **type;

  for (type=types; *type; type++) {
    if ((*type)->flags & VT_CHECK_TYPE_FLAG_NEED_INIT) {
      name = (char *)(*type)->name;
      done = 0;

      for (i=0, n=cfg_size ((cfg_t *)cfg, "type"); ! done && i < n; i++) {
        if ((sec = cfg_getnsec ((cfg_t *)cfg, "type", i)) &&
            (title = (char *)cfg_title ((cfg_t *)sec)))
        {
          if (strcmp (name, title) == 0) {
            (*type)->init_func (sec);
            done = 1;
          }
        }
      }

      if (! done) {
        vt_error ("%s: type section for %s missing", __func__, name);
        return VT_ERR_INVAL;
      }
    }
  }

  return VT_SUCCESS;
}





void
vt_worker (void *arg)
{
  int sockfd;
  int err, ret;
  vt_check_t *check;
  vt_context_t *ctx;
  vt_request_t *request;
  vt_stage_t *stage;

  //vt_map_list_t *maps;
  //vt_slist_t *root, *cur; // this should be defined in context
  //vt_stage_t *stage; // this should be defined in context
  //vt_stats_t *stats; // this should be defined in context
  //vt_check_t *check;
  //vt_map_result_t *res;
  //vt_slist_t *cur;

  sockfd = ((vt_request_arg_t *)arg)->sockfd;
  ctx = ((vt_request_arg_t *)arg)->context;


  // request is read here
  // score is global or passed
  // stats
  // map list
  //
  //vt_request_arg_t *req = (vt_request_arg_t *)arg;
  //
  //int vt_stage_enter (vt_stage_t *, vt_request_t *, vt_score_t *, vt_stats_t *,
  //  vt_map_list_t *);
  //



  if (vt_request_parse (&req, fd) != VT_SUCCESS) {
    vt_error ("%s: cannot read request", __func__);
    // treat as temporary error... I definitely need the context as well
    // as a matter of fact, the context should contain the stages, just to
    // make this stuff easier
    vt_reply (fd, ctx->temporary_error);
  }

  //
  // place contents of vt_stage_enter right here...
  //
  //int
  //vt_stage_enter (vt_stage_t *stage,
  //              vt_request_t *request,
  //              vt_score_t *score,
  //              vt_stats_t *stats,
  //              vt_map_list_t *maps)
  //{

  ////st_root = ctx->stages;
  for (st_cur = ctx->stages; st_cur; st_cur = st_cur->next) {
    stage = (vt_stage_t *)st_cur->data;
    ret = vt_map_list_evaluate (&res, maps, stage->maps, req);
    if (ret != VT_SUCCESS) {
      //// reply with temporary error... nothing we can do about it!
    }
    if (res == VT_MAP_RESULT_DO) {
      for (ck_cur = stage->checks; ck_cur; ck_cur = ck_cur->next) {
        check = (vt_check_t *)ck_cur->data;
        ret = vt_map_list_evaluate (&res, maps, check->maps, req);

        if (res == VT_MAP_RESULT_DO) {
          ret = check->check_func (chk, req, score);
          if (ret != VT_SUCCESS) {
            // error
          }
        }
      }

      vt_score_wait (score);
    }
  }

  err = VT_SUCCESS;

  /* check if this stage should be entered for the current request */
  //ret = vt_map_list_evaluate (&res, maps, stage->maps, request);
  //
  //if (ret != VT_SUCCESS)
  //  return ret;
  //if (res == VT_MAP_RESULT_REJECT)
  //  return VT_SUCCESS;
  //
  //for (cur=stage->checks; cur; cur=cur->next) {
  //  check = (vt_check_t *)cur->data;
  //  /* check if this check should be evaluated for the current request */
  //  //fprintf (stderr, "%s (%d): check name: %s\n", __func__, __LINE__, check->name);
  //  //fprintf (stderr, "%s (%d): maps address: %d\n", __func__, __LINE__, check->maps);
  //  ret = vt_map_list_evaluate (&res, maps, check->maps, request);
  //  if (ret != VT_SUCCESS) {
  //    return ret;
  //  }
  //  if (res != VT_MAP_RESULT_REJECT) {
  //    ret = check->check_func (check, request, score, stats);
  //    if (ret != VT_SUCCESS)
  //      return ret;
  //  }
  //}

  vt_score_wait (score);

  //return VT_SUCCESS;
  //}
  //
}

int
main (int argc, char *argv[])
{
  if (argc != 2) {
    fprintf (stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
    return EXIT_FAILURE;
  }

  vt_check_type_t *check_types[5];
  vt_map_type_t *map_types[2];

  check_types[0] = vt_check_dnsbl_type ();
  check_types[1] = vt_check_rhsbl_type ();
  check_types[2] = vt_check_str_type ();
  check_types[3] = vt_check_pcre_type ();
  check_types[4] = NULL;

  map_types[0] = vt_map_bdb_type ();
  map_types[1] = NULL;

  // parse configuration
  vt_context_t *ctx;
  cfg_t *cfg = vt_cfg_parse (argv[1]);

  if (! (ctx = malloc0 (sizeof (vt_context_t))))
    vt_fatal ("%s: %s", __func__, strerror (ENOMEM));
  if (vt_context_init (ctx, cfg) != VT_SUCCESS)
    vt_fatal ("%s: error", __func__);

  daemonize (ctx);

  // create maps
  // create stats
  // create thread pool for handling requests...
  // ...
  // listen ();
  // ...

  // create argument that contains the stuff the worker needs

  // create stages and checks
  // open the socket
  // create the workers
  // I myself remain the "master" that will spawn new threads when necessary
  // and I will handle the signals



  vt_check_t *check;
  vt_map_list_t *maps;
  vt_slist_t *stages, *cur;
  vt_stage_t *stage;
  int *maps, exists, exec;

  // create a nice new set...
  if (vt_map_list_create (&maps) != VT_SUCCESS)
    return EXIT_FAILURE;
  if (vt_cfg_maps_create (maps, map_types, cfg))
    vt_panic ("%s: cannot initialize maps", __func__);

  if (vt_cfg_types_init (check_types, cfg) != 0)
    vt_panic ("%s: configuration error", __func__);
  //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  if (vt_cfg_stages_create (&stages, check_types, maps, cfg) != VT_SUCCESS)
    vt_panic ("%s: cannot create stages", __func__);
  //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  if (vt_checks_init (&root, (const vt_type_t**)types, set, cfg))
    vt_panic ("%s: configuration error", __func__);

  //vt_map_list_cache_reset (maps);
  //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  //for (cur=stages; cur; cur=cur->next) {
  //  stage = (vt_stage_t *)cur->data;
  //  //vt_request_t *, vt_score_t *, vt_stats_t *, vt_map_list_t *
  //  vt_stage_enter (stage, &request, score, NULL, maps);
  //}
  //printf ("done, weight is %d, bye\n", score->points);
  //return 0;
}
