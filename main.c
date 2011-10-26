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
#include "request.h"
#include "score.h"
#include "slist.h"

typedef void(*VT_CFG_OPT_WALK_FUNC)(void *, cfg_opt_t *opt);

void
vt_cfg_opt_walk_copy (void *arg, cfg_opt_t *opt) {

  cfg_opt_t **cur = (cfg_opt_t **)arg;
  memcpy (*cur, opt, sizeof (cfg_opt_t));
  (*cur)++;
}

void
vt_cfg_opt_walk_count (void *arg, cfg_opt_t *opt) {

  int *num = (int *)arg;
  (*num)++;
}

void
vt_cfg_opt_walk (VT_CFG_OPT_WALK_FUNC func, void *arg, vt_type_t *types[], size_t offset)
{
  int i, j, skip;
  cfg_opt_t *p, *q;

  for (i=0; types[i]; i++) {
    p = *((cfg_opt_t **) (((char *) types[i]) + offset));

    for (; p && p->type != CFGF_NONE; p++) {
      skip = 0;

      for (j=0; ! skip && types[j] && j < i; j++) {
        q = *((cfg_opt_t **) (((char *) types[j]) + offset));

        for (; ! skip && q && q->type != CFGF_NONE; q++) {
          if (strcmp (p->name, q->name) == 0)
            skip = 1;
        }
      }

      if (! skip)
        func (arg, p);
    }
  }
}

#define vt_cfg_opts_merge(a,b,c) (vt_cfg_opts_merge_offset ((a), (b), offsetof (vt_type_t, c)))

int
vt_cfg_opts_merge_offset (cfg_opt_t **opts, vt_type_t *types[], size_t offset)
{
  cfg_opt_t *root, *cur;
  cfg_opt_t end[] = { CFG_END () };
  int num = 0;

  vt_cfg_opt_walk (&vt_cfg_opt_walk_count, (void *)&num, types, offset);
  if (! (root = malloc ((sizeof (cfg_opt_t) * (num + 1)))))
    return VT_ERR_NOMEM;

  cur = root;
  vt_cfg_opt_walk (&vt_cfg_opt_walk_copy, (void *)&cur, types, offset);
  memcpy (root+num, end, sizeof (cfg_opt_t));
  *opts = root;
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

// FIXME: cleanup memory on failure!
int
vt_checks_init (vt_slist_t **dest, const vt_type_t *types[], const cfg_t *cfg)
{
  cfg_t *sec;
  char *type, *title;
  int i, j, n;
  vt_check_t *check;
  vt_slist_t *root, *next;

  root = next = NULL;

  for (i=0, n=cfg_size ((cfg_t *)cfg, "check"); i < n; i++) {
    if ((sec = cfg_getnsec ((cfg_t *)cfg, "check", i)) &&
        (title = (char *)cfg_title ((cfg_t *)sec)))
    {
      if ((type = cfg_getstr ((cfg_t *)sec, "type")) == NULL) {
        vt_error ("%s: type for %s missing", __func__, title);
        return VT_ERR_INVAL;
      }

      for (j=0; types[j] && strcmp (type, types[j]->name); j++)
        ;

      if (types[j] == NULL) {
        vt_error ("%s: type for %s does not exist", __func__, title);
        return VT_ERR_INVAL;
      }
      if (types[j]->create_check_func (&check, sec) != VT_SUCCESS) {
        /* create_check_func reports errors itself */
        return VT_ERR_INVAL;
      }

      if ((next = vt_slist_append (root, check)) == NULL) {
        vt_error ("%s: slist_append: %s", __func__, strerror (errno));
        return VT_ERR_INVAL;
      }

      if (root == NULL)
        root = next;
    }
  }

  root = vt_slist_sort (root, &vt_check_sort);
  *dest = root;

  return VT_SUCCESS;
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
    //.client_address = "127.0.0.1",
    .client_address = "217.64.101.51",
    .client_name = "localhost",
    .reverse_client_name = "localhost"
  };

  vt_score_t *score = vt_score_create ();

  int i = 0;
  int n = 2;

  vt_type_t *types[5];
  cfg_opt_t *type_opts, *type_opt;
  cfg_opt_t *check_opts, *check_opt;

  types[0] = vt_dnsbl_type ();
  types[1] = vt_rhsbl_type ();
  types[2] = vt_str_type ();
  types[3] = vt_pcre_type ();
  types[4] = NULL;

  vt_cfg_opts_merge (&type_opts, types, type_opts);
  vt_cfg_opts_merge (&check_opts, types, check_opts);

  cfg_opt_t opts[] = {
    CFG_SEC ("type", type_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_SEC ("check", check_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_END ()
  };

  vt_check_t *check;
  vt_slist_t *root, *cur;
  cfg_t *cfg = cfg_init (opts, CFGF_NONE);
  cfg_parse (cfg, argv[1]);

  if (vt_types_init (types, cfg) != 0)
    vt_panic ("%s: configuration error", __func__);
  if (vt_checks_init (&root, (const vt_type_t**)types, cfg))
    vt_panic ("%s: configuration error", __func__);

  for (cur=root; cur; cur=cur->next) {
    check = (vt_check_t *)cur->data;
    fprintf (stderr, "%s: check: %s, priority: %d\n", __func__, check->name, check->prio);
    check->check_func (check, &request, score, NULL);
  }

  vt_score_wait (score);

  printf ("done, weight is %d, bye\n", score->points);
  return 0;
}

