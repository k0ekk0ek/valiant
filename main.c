/* valiant - a postfix policy daemon */

/* system includes */
#include <unistd.h>

/* valiant includes */
#include "error.h"
#include "conf.h"
#include "consts.h"
#include "utils.h"

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
  char *path;
  int c;
  vt_context_t *ctx;

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
  if (! (ctx = vt_context_create (ctx, cfg)))
    vt_fatal ("cannot create context");

  daemonize ();

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

  // create workers
  vt_thread_pool_t *pool = vt_thread_pool_create ("worker", ctx->max_threads, &vt_worker);
  if (! pool) {
    vt_panic ("couldn't create thread pool!");
  }

  // install signal handlers

  for (;;) {
    if ((clntfd = accept (sockfd, (struct sockaddr *)addr, &addr_size)) < 0)
      vt_fatal ("%s (%d): accept: %s", __func__, __LINE__, strerror (errno));
    if (! (arg = calloc (1, sizeof (vt_worker_arg_t))))
      vt_fatal ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));

    arg->fd = clntfd;
    arg->ctx = ctx

    vt_thread_pool_push (arg);
  }

  return EXIT_SUCCESS;
}
