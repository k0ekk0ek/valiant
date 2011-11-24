/* valiant - a postfix policy daemon */

/* system includes */
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "error.h"
#include "check_dnsbl.h"
#include "check_pcre.h"
#include "check_rhsbl.h"
#include "check_str.h"
#include "conf.h"
#include "map_bdb.h"
#include "thread_pool.h"
#include "utils.h"
#include "worker.h"

vt_map_type_t *map_types[2];
vt_check_type_t *check_types[5];

void
vt_usage (void)
{
  vt_error ("Usage: valiant [OPTION]");
  vt_error ("  -c   path to configuration file");
  exit (EXIT_FAILURE);
}

int
main (int argc, char *argv[])
{
  cfg_t *cfg;
  char *path = VT_CFG_CONFIG_FILE;
  int c;
  vt_context_t *ctx;
  vt_error_t err;

  /* init map types */
  map_types[0] = vt_map_bdb_type ();
  map_types[1] = NULL;
  /* init check types */
  check_types[0] = vt_check_dnsbl_type ();
  check_types[1] = vt_check_pcre_type ();
  check_types[2] = vt_check_rhsbl_type ();
  check_types[3] = vt_check_str_type ();
  check_types[4] = NULL;

  /* parse command line arguments */
  while ((c = getopt (argc, argv, ":c:")) != -1) {
    switch (c) {
      case 'c':
        path = optarg;
        break;
      case ':': /* missing argument */
        vt_error ("option -%c requires an argument", optopt);
        vt_usage ();
        break;
      case '?': /* unrecognized option */
        vt_error ("unrecognized option -%c", optopt);
        vt_usage ();
        break;
    }
  }

  if (! (cfg = vt_cfg_parse (path)))
    vt_fatal ("cannot parse %s", path);

  if (! (ctx = vt_context_create (cfg, map_types, check_types, &err)))
    vt_fatal ("cannot create context: %d", err);

  /* initialize check types */
  if (vt_check_types_init (cfg, check_types, &err) != 0)
    vt_fatal ("cannot init check types");

  //daemonize (ctx);

  /* open socket that we will listen on */
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints, *res;
  int sockfd, newfd;
  vt_worker_arg_t *arg;

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo (NULL, ctx->port, &hints, &res);

  sockfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
  bind(sockfd, res->ai_addr, res->ai_addrlen);
  listen(sockfd, 10);

  // drop priveleges
  //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  // create workers
  vt_thread_pool_t *pool = vt_thread_pool_create ("worker", ctx->max_threads, &vt_worker, &err);
  if (! pool) {
    vt_panic ("couldn't create thread pool!");
  }
  //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  // install signal handlers

  for (;;) {
    if ((newfd = accept (sockfd, (struct sockaddr *)&their_addr, &addr_size)) < 0)
      vt_fatal ("%s: accept: %s", __func__, strerror (errno));
    if (! (arg = calloc (1, sizeof (vt_worker_arg_t))))
      vt_fatal ("%s: calloc: %s", __func__, strerror (errno));

    arg->fd = newfd;
    arg->ctx = ctx;
    //fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
    vt_thread_pool_task_push (pool, (void *)arg, &err);
  }

  return EXIT_SUCCESS;
}
