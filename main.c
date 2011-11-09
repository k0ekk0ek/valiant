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
#include "valiant.h"
#include "map.h"
#include "map_bdb.h"
#include "request.h"
#include "score.h"
#include "slist.h"
#include "stage.h"

int
vt_cfg_maps_create (vt_map_list_t *set, vt_map_type_t *types[], const cfg_t *cfg)
{
  // IMPLEMENT
  // aka... populate the vt_set_t structure!
  cfg_t *sec;
  char *type;
  int i, j, n, done, ret, id;
  vt_map_t *map;

  for (i=0, n=cfg_size ((cfg_t *)cfg, "map"); i < n; i++) {
    if ((sec = cfg_getnsec ((cfg_t *)cfg, "map", i)) &&
        (type = cfg_getstr ((cfg_t *)sec, "type")))
    {
//fprintf (stderr, "%s (%d): type = %s\n", __func__, __LINE__, type);
      for (j=0, done=0; ! done && types[j]; j++) {
//fprintf (stderr, "%s (%d): type = %s, name = %s\n", __func__, __LINE__, type, types[j]->name);
        if (strcmp (type, types[j]->name) == 0) {
          done = 1;
//fprintf (stderr, "found\n");
          if ((ret = types[j]->create_map_func (&map, sec)) != VT_SUCCESS)
            return ret; // FIXME: should destroy set?!?!
          if ((ret = vt_map_list_set_map (&id, set, map)) != VT_SUCCESS)
            return ret;
        }
      }
    }
  }
//fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  return VT_SUCCESS;
}

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

int
vt_cfg_stage_create (vt_stage_t **dest,
                     const vt_check_type_t **types,
                     const vt_map_list_t *set,
                     const cfg_t *sec)
{
  cfg_t *subsec;
  char *type;
  int err, ret;
  int i, j, n;
  vt_check_t *check;
  vt_stage_t *stage = NULL;

  if ((ret = vt_stage_create (&stage, set, (cfg_t *)sec)) != VT_SUCCESS)
    return ret;

  /* create checks defined in stage */
  for (i=0, n=cfg_size ((cfg_t *)sec, "check"); i < n; i++) {
    if ((subsec = cfg_getnsec ((cfg_t *)sec, "check", i)) &&
        (type = cfg_getstr (subsec, "type")))
    {
      check = NULL;
      for (j=0; ! check && types[j]; j++) {
//fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
        if (strcmp (type, types[j]->name) == 0) {
          ret = types[j]->create_check_func (&check, set, subsec);
          if (ret != VT_SUCCESS) {
            err = ret;
            goto FAILURE;
          }
        }
      }

      if (! check) {
        vt_error ("%s: invalid check type %s", __func__, type);
        err = VT_ERR_INVAL;
        goto FAILURE;
      }
//fprintf (stderr, "%s (%d): check name: %s\n", __func__, __LINE__, check->name);
      if ((ret = vt_stage_set_check (stage, check)) != VT_SUCCESS) {
        err = ret;
        goto FAILURE;
      }
//fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
    }
  }

  *dest = stage;
  return VT_SUCCESS;

FAILURE:
  vt_stage_destroy (stage, true);
  return err;
}

int
vt_cfg_stages_create (vt_slist_t **dest,
                      const vt_check_type_t **types,
                      const vt_map_list_t *set,
                      const cfg_t *cfg)
{
  cfg_t *sec, *subsec;
  int err, ret;
  int i, n;
  vt_slist_t *root, *next;
  vt_stage_t *stage;

  root = next = NULL;
  for (i=0, n=cfg_size ((cfg_t *)cfg, "stage"); i < n; i++) {
    if ((sec = cfg_getnsec ((cfg_t *)cfg, "stage", i))) {
//fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
      if ((ret = vt_cfg_stage_create (&stage, types, set, sec)) != VT_SUCCESS) {
        err = ret;
        goto FAILURE;
      }
//fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
      vt_stage_lineup (stage); /* align checks and calculate weights */

      if ((next = vt_slist_append (root, stage)) == NULL) {
        err = VT_ERR_NOMEM;
        goto FAILURE;
      }
      if (root == NULL) {
        root = next;
      }
    }
  }

  *dest = root;
  return VT_SUCCESS;
FAILURE:
  // free stuff
  return err;
}

int
main (int argc, char *argv[])
{
  if (argc != 2) {
    fprintf (stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
    return EXIT_FAILURE;
  }

  vt_request_t request = {
    .helo_name = "localhost",
    .sender = "root@localhost",
    .client_address = "127.0.0.1",
    .client_name = "localhost",
    .reverse_client_name = "localhost"
  };

  vt_score_t *score = vt_score_create ();

  int i = 0;
  int n = 2;

  vt_check_type_t *check_types[5];
  vt_map_type_t *map_types[2];

  check_types[0] = vt_check_dnsbl_type ();
  check_types[1] = vt_check_rhsbl_type ();
  check_types[2] = vt_check_str_type ();
  check_types[3] = vt_check_pcre_type ();
  check_types[4] = NULL;

  map_types[0] = vt_map_bdb_type ();
  map_types[1] = NULL;


  vt_check_t *check;
  vt_map_list_t *maps;
  vt_slist_t *stages, *cur;
  vt_stage_t *stage;

  cfg_opt_t check_in_opts[] = {
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t check_opts[] = {
    CFG_SEC ("in", (cfg_opt_t *) check_in_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_STR_LIST ("maps", 0, CFGF_NONE),
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
    CFG_STR_LIST ("maps", 0, CFGF_NONE),
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
    // policy included here
		CFG_SEC ("stage", stage_opts, CFGF_MULTI),
    CFG_SEC ("map", map_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("type", type_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_END ()
  };

  cfg_t *cfg = cfg_init (opts, CFGF_NONE);

  cfg_parse (cfg, argv[1]);

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
  //if (vt_checks_init (&root, (const vt_type_t**)types, set, cfg))
  //  vt_panic ("%s: configuration error", __func__);
  //int *maps, exists, exec;

	// ... ...

  vt_map_list_cache_reset (maps);
//fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  for (cur=stages; cur; cur=cur->next) {
    stage = (vt_stage_t *)cur->data;
    //vt_request_t *, vt_score_t *, vt_stats_t *, vt_map_list_t *
    vt_stage_enter (stage, &request, score, NULL, maps);
  }

  // start making it a daemon... think

  printf ("done, weight is %d, bye\n", score->points);
  return 0;
}
