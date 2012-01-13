/* system includes */
#include <confuse.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "conf.h"
#include "context.h"
#include "dict_dnsbl.h"
#include "dict_hash.h"
#include "dict_pcre.h"
#include "dict_rhsbl.h"
#include "dict_spf.h"
#include "dict_str.h"
#include "result.h"

int
main (int argc, char *argv[])
{
  cfg_t *config;
  int i;
  vt_context_t *ctx;
  vt_check_t *check;
  vt_dict_t *dict;
  vt_dict_type_t *types[7];
  vt_error_t err;
  vt_result_t *res;
  vt_stage_t *stage;

  if (argc < 2)
    return 1;

  if (! (config = vt_cfg_parse (argv[1])))
    return 1;

  types[0] = vt_dict_dnsbl_type ();
  types[1] = vt_dict_hash_type ();
  types[2] = vt_dict_pcre_type ();
  types[3] = vt_dict_rhsbl_type ();
  types[4] = vt_dict_spf_type ();
  types[5] = vt_dict_str_type ();
  types[6] = NULL;

  if (! (ctx = vt_context_create (types, config, &err)))
    return 1;
  if (! (res = vt_result_create (ctx->ndicts, &err)))
    return 1;
  // context and dicts initialized, parse request and run checks

  vt_request_t *req = vt_request_create (NULL);
  int fd = open ("request.txt", O_RDONLY);

  vt_request_parse (req, fd, NULL);

  //
  //for (i = 0; i < context->ndicts; i++) {
  //  vt_error ("%s:%d: pos: %d, pos in dict: %d", __func__, __LINE__, i, context->dicts[i]->pos);
  //}
  //

  /* it's impossible to checks more dicts per iteration than the maximum number
     of dicts configured */
  int found;
  int run, j, k, checkno;
  float points;
  int stageno;
  // find another name for tasks...
  int ndicts, dictno, *dicts;
  int depno, invert, pos;
  int depend;

  if (! (dicts = calloc (ctx->ndicts + 1, sizeof (int)))) {
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return 1;
  }

vt_debug ("stages: %d", ctx->nstages);

  for (stageno = -1; stageno < ctx->nstages; stageno++) {
    memset (dicts, 0, ctx->ndicts * sizeof (int));
    ndicts = 0;
vt_debug ("stage: %d", stageno);
    /* run checks in current stage */
    if (stageno >= 0) {
      run = 1;
      stage = ctx->stages[stageno];
      /* don't run stage if dependencies failed */
      for (depno = 0; run && depno < stage->ndepends; depno++) {
        pos = stage->depends[depno];

        if (! res->results[pos]->ready)
          vt_panic ("%s: dict %s not ready",
            __func__, ctx->dicts[pos]->name);
        if (res->results[pos]->points)
          run = 1;
      }
//vt_debug ("%s:%d: run: %d", __func__, __LINE__, run);
      if (run) {
        for (checkno = 0; checkno < stage->nchecks; checkno++) {
          check = stage->checks[checkno];
          run = check->ndepends ? 0 : 1;
//vt_debug ("%s:%d: run: %d", __func__, __LINE__, run);
          for (depno = 0; ! run && depno < check->ndepends; depno++) {
            pos = check->depends[depno];

            if (! res->results[pos]->ready)
              vt_panic ("%s: dict %s not ready",
                __func__, ctx->dicts[pos]->name);
            if (res->results[pos]->points)
              run = 1;
          }

          for (dictno = 0; run && dictno < ndicts; dictno++) {
            if (dicts[dictno] == check->dict)
              run = 0;
          }
//vt_debug ("%s:%d: run: %d", __func__, __LINE__, run);
          if (run) {
            if (res->results[ check->dict ]->ready)
              points += res->results[ check->dict ]->points;
            else
              dicts[ndicts++] = check->dict;
          }
        }
      }
    }

    if ((stageno + 1) < ctx->nstages) {
      stage = ctx->stages[(stageno + 1)];

      for (checkno = 0; checkno < stage->nchecks; checkno++) {
        check = stage->checks[checkno];

        for (depno = 0; depno < check->ndepends; depno++) {
          pos = check->depends[depno];

          for (dictno = 0, run = 1; run && dictno < ndicts; dictno++) {
            if (dicts[dictno] == pos)
              run = 0;
          }
          if (run && ! res->results[pos]->ready)
            dicts[ndicts++] = pos;
        }
      }
    }

    /* run checks */
    for (dictno = 0; dictno < ndicts; dictno++) {
      pos = dicts[dictno];
      fprintf (stderr, "%s: pos: %d, name: %s\n", __func__, pos, ctx->dicts[pos]->name);
      vt_dict_check (ctx->dicts[pos], req, res, pos, &err);
    }

    vt_result_wait (res);

    /* evaluate scores */
    if (stageno >= 0) {
      stage = ctx->stages[stageno];

      for (checkno = 0; checkno < stage->nchecks; checkno++) {
        pos = (stage->checks[checkno])->dict;

        for (dictno = 0; dictno < ndicts; dictno++) {
          if (pos == dicts[dictno]) {
            points += res->results[pos]->points;
            break;
          }
        }
      }
    }
  }

vt_debug ("points: %f", points);

  return 0;
}
