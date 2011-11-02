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
#include "constants.h"
#include "map.h"
#include "map_bdb.h"
#include "request.h"
#include "score.h"
#include "set.h"
#include "slist.h"
#include "stage.h"

int
vt_maps_init (vt_set_t *set, vt_map_type_t *types[], const cfg_t *cfg)
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
          if ((ret = vt_set_insert_map (&id, set, map)) != VT_SUCCESS)
            return ret;
        }
      }
    }
  }
//fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  return VT_SUCCESS;
}

int
vt_types_init (vt_type_t *types[], const cfg_t *cfg)
{
  cfg_t *sec;
  char *name, *title;
  int i, n, done;
  vt_type_t **type;

  for (type=types; *type; type++) {
    if ((*type)->flags & VT_TYPE_FLAG_NEED_INIT) {
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
vt_cfg_check_create (vt_check_t **dest,
                     const vt_check_type_t **types,
                     const vt_set_t *set,
                     const cfg_t *sec)
{
  // IMPLEMENT
}

int
vt_cfg_stage_create (vt_stage_t **dest,
                     const vt_check_type_t **types,
                     const vt_set_t *set,
                     const cfg_t *sec)
{
  cfg_t *sub_sec;
  char *type;
  int i, j, n, err, ret;
  int *maps;
  vt_check_t *check;
  vt_slist_t *entry;
  vt_stage_t *stage = NULL;

  if ((stage = malloc0 (sizeof (vt_stage_t))) == NULL) {
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  /* create map list for stage */
  ret = vt_cfg_map_list_create (&stage->include, set, sec, "include");
  if (ret != VT_SUCCESS) {
    err = ret;
    goto FAILURE;
  }
  ret = vt_cfg_map_list_create (&stage->exclude, set, sec, "exclude");
  if (ret != VT_SUCCESS) {
    err = ret;
    goto FAILURE;
  }

  /* create checks defined in stage */
  for (i=0, n=cfg_size ((cfg_t *)sec, "check"); i < n; i++) {
    if ((sub_sec = cfg_getnsec ((cfg_t *)sec, "check", i)) &&
        (type = cfg_getstr (sub_sec, "type")))
    {
      check = NULL;
      for (j=0; ! check && types[j]; j++) {
        if (strcmp (type, types[j]->name) == 0) {
          if (types[j]->create_check_func (&check, sub_sec) != VT_SUCCESS) {
            err = ret;
            goto FAILURE;
          }
        }
      }

      if (! check) {
        vt_error ("%s: check type %s invalid", __func__, type);
        err = VT_ERR_INVAL;
        goto FAILURE;
      }

      /* create map list for check */
      ret = vt_cfg_map_list_create (&check->include, set, sec, "include");
      if (ret != VT_SUCCESS) {
        err = ret;
        goto FAILURE;
      }
      ret = vt_cfg_map_list_create (&check->exclude, set, sec, "include");
      if (ret != VT_SUCCESS) {
        err = ret;
        goto FAILURE;
      }
      if (! (entry = vt_slist_append (stage->checks, check)) == NULL) {
        err = VT_ERR_NOMEM;
        goto FAILURE;
      }
      if (! stage->checks) {
        stage->checks = entry;
      }
    }
  }

  stage->checks = vt_slist_sort (stage->checks, vt_check_sort);
  *dest = stage;
  return VT_SUCCESS;

FAILURE:
  vt_stage_destroy (stage);
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

  vt_type_t *check_types[5];
  vt_map_type_t *map_types[2];

  check_types[0] = vt_dnsbl_type ();
  check_types[1] = vt_rhsbl_type ();
  check_types[2] = vt_str_type ();
  check_types[3] = vt_pcre_type ();
  check_types[4] = NULL;

  map_types[0] = vt_map_bdb_type ();
  map_types[1] = NULL;


  vt_check_t *check;
  vt_set_t *set;
  vt_slist_t *root, *cur;

  // create a nice new set...
  if (vt_set_create (&set) != VT_SUCCESS)
    return EXIT_FAILURE;
  if (vt_maps_init (set, map_types, cfg))
    vt_panic ("%s: cannot initialize include/exclude maps", __func__);
  if (vt_types_init (types, cfg) != 0)
    vt_panic ("%s: configuration error", __func__);
  if (vt_checks_init (&root, (const vt_type_t**)types, set, cfg))
    vt_panic ("%s: configuration error", __func__);

  int *maps, exists, exec;

  vt_set_cache_init (set);

  //

  printf ("done, weight is %d, bye\n", score->points);
  return 0;
}
