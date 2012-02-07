/* system includes */
#include <assert.h>
#include <confuse.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
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
#include "stats.h"
#include "thread_pool.h"
#include "watchdog.h"
#include "worker.h"

#define VT_SELECT_TIMEOUT_SECONDS (2)
#define VT_SELECT_TIMEOUT_MICROSECONDS (0)

typedef struct _vt_cleanup_arg vt_cleanup_arg_t;

struct _vt_cleanup_arg {
  vt_context_t *context;
  vt_thread_pool_t *workers;
};

/* prototypes */
void help (const char *);
void usage (const char *);
void version (const char *);

void *vt_cleanup_worker (void *);
void vt_cleanup (vt_thread_pool_t *, vt_context_t *, int);

void
help (const char *prog)
{
  const char *fmt =
  "Usage: %s <options>\n"
  "\n"
  "Options\n"
  "\t-c, --config-file FILE\tread configuration from FILE"
  "\t-h, --help\t\tshow help information\n"
  "\t-V, --version\t\tcopyright and version information\n";

  printf (fmt, prog);
  exit (EXIT_SUCCESS);
}

void
usage (const char *prog)
{
  const char *fmt =
  "Usage: %s <options>\n"
  "See %s --help for available options\n";

  printf (fmt, prog, prog);
  exit (EXIT_FAILURE);
}

void
version (const char *prog)
{
  const char *fmt =
  "%s version %s\n"
  "Copyright (c) 2011 Jeroen Koekkoek\n"
  "\n"
  "COPYRIGHT\n";

  printf (fmt, prog, "0.1");
  exit (EXIT_SUCCESS);
}

void *
vt_cleanup_worker (void *arg)
{
  vt_context_t *context;
  vt_thread_pool_t *workers;

  assert (arg);

  context = ((vt_cleanup_arg_t *)arg)->context;
  workers = ((vt_cleanup_arg_t *)arg)->workers;
vt_error ("%s:%d: ", __func__, __LINE__);
  free (arg);
vt_error ("%s:%d: ", __func__, __LINE__);
  (void)vt_thread_pool_destroy (workers, NULL);
vt_error ("%s:%d: ", __func__, __LINE__);
  (void)vt_context_destroy (context, NULL);
vt_error ("%s:%d: ", __func__, __LINE__);
  return NULL;
}

void
vt_cleanup (vt_thread_pool_t *workers, vt_context_t *context, int async)
{
  int ret;
  pthread_t thread;
  vt_cleanup_arg_t *arg;

  arg = NULL;
  if (! async)
    goto failure;
  if (! (arg = calloc (1, sizeof (vt_cleanup_arg_t)))) {
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
    
  }

  arg->context = context;
  arg->workers = workers;

  if ((ret = pthread_create (&thread, NULL, vt_cleanup_worker, (void *)arg)) < 0) {
    vt_error ("%s: pthread_create: %s", __func__, strerror (ret));
    goto failure;
  }

  return;
failure:
  if (arg)
    free (arg);
  // fine... need to do it ourselves
  // handle errors
}

int
main (int argc, char *argv[])
{
  cfg_t *cfg;
  char *prog;
  char *config_file = "/etc/valiant/valiant.conf";
  vt_context_t *ctx;
  vt_dict_type_t *types[7];
  vt_error_t err;
  vt_thread_pool_t *pool;
  vt_stats_t *stats, *new_stats;

  if ((prog = strrchr (argv[0], '/')))
    prog++;
  else
    prog = argv[0];

  /* parse command line options */
  int c;
  char *short_opts = "c:hV";
  struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"config-file", required_argument, NULL, 'c'}
  };

  for (; (c = getopt_long (argc, argv, short_opts, long_opts, NULL)) != EOF; ) {
    switch (c) {
      case 'c':
        config_file = optarg;
        break;
      case 'h':
        help (prog);
        break; /* never reached */
      case 'V':
        version (prog);
        break; /* never reached */
      default:
        usage (prog);
        break; /* never reached */
    }
  }

  /* it's important that wathchdog is initialized before any threads are
     created because it sets the calling threads signal mask to ignore all
     signals */
  vt_watchdog_init ();







  /* parse configuration file */
  if (! (cfg = vt_cfg_parse (config_file)))
    return 1;

  types[0] = vt_dict_dnsbl_type ();
  types[1] = vt_dict_hash_type ();
  types[2] = vt_dict_pcre_type ();
  types[3] = vt_dict_rhsbl_type ();
  types[4] = vt_dict_spf_type ();
  types[5] = vt_dict_str_type ();
  types[6] = NULL;

  if (! (ctx = vt_context_create (types, cfg, &err)))
    vt_fatal ("cannot create context: %d", err);

  cfg_free (cfg);


  // drop priveleges
  // fprintf (stderr, "%s (%d)\n", __func__, __LINE__);
  // create workers

  // create stats printer
  stats = vt_stats_create (ctx->dicts, ctx->ndicts, &err);
  vt_stats_thread (stats);

  vt_worker_arg_t warg;
  warg.context = ctx;
  warg.stats = stats;

  if (! (pool = vt_thread_pool_create ((void *)&warg, ctx->max_threads, &vt_worker, &err)))
    return EXIT_FAILURE;
  vt_thread_pool_set_max_idle_threads (pool, ctx->max_idle_threads);
  vt_thread_pool_set_max_queued (pool, ctx->max_tasks);
  vt_debug ("created thread pool");
  /* open socket that we will listen on */
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints, *res;
  int sock, conn;
  //vt_worker_arg_t *arg;
  int *task;

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo (NULL, ctx->port, &hints, &res);

  sock = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
  bind(sock, res->ai_addr, res->ai_addrlen);
  listen(sock, 10);

  fd_set fds;
  struct timeval tv;
  int dead, ret;
  cfg_t *new_cfg;
  vt_context_t *new_ctx;
  vt_thread_pool_t *new_pool;
  tv.tv_sec = VT_SELECT_TIMEOUT_SECONDS;
  tv.tv_usec = VT_SELECT_TIMEOUT_MICROSECONDS;

  for (dead = 0; ! dead; ) {
    FD_ZERO (&fds);
    FD_SET (sock, &fds);

    ret = select (sock+1, &fds, NULL, NULL, &tv);

    /* always check if we received a signal or not */
    switch (vt_watchdog_signal ()) {
      case SIGTERM: /* terminate */
        dead = 1;
        break;
      case SIGHUP: /* reload */
        new_cfg = NULL;
        new_ctx = NULL;
        new_pool = NULL;
vt_debug ("%s:%d", __func__, __LINE__);
        if (! (new_cfg = vt_cfg_parse (config_file))) {
          vt_error ("%s: could not parse %s: reload aborted",
            __func__, config_file);
          goto failure_reload;
        }
vt_debug ("%s:%d", __func__, __LINE__);
        if (! (new_ctx = vt_context_create (types, new_cfg, &err))) {
          vt_error ("%s: could not create context: reload aborted", __func__);
          goto failure_reload;
        }
vt_debug ("%s:%d", __func__, __LINE__);
        cfg_free (new_cfg);
        new_cfg = NULL;

        new_stats = vt_stats_create (new_ctx->dicts, new_ctx->ndicts, &err);
        vt_stats_thread (stats);

        warg.context = new_ctx;
        warg.stats = new_stats;

        new_pool = vt_thread_pool_create ((void *)&warg,
          new_ctx->max_threads, &vt_worker, &err);
vt_debug ("%s:%d", __func__, __LINE__);
        if (! new_pool) {
          vt_error ("%s: could not create workers: reload aborted", __func__);
          goto failure_reload;
        }
        vt_thread_pool_set_max_idle_threads (new_pool, new_ctx->max_idle_threads);
        vt_thread_pool_set_max_queued (new_pool, new_ctx->max_tasks);
vt_debug ("%s:%d", __func__, __LINE__);
        vt_stats_destroy (stats, NULL);
        vt_cleanup (pool, ctx, 1);
        ctx = new_ctx;
        pool = new_pool;
        stats = new_stats;
        break;
failure_reload:
vt_error ("%s:%d", __func__, __LINE__);
        if (new_cfg)
          cfg_free (new_cfg);
        if (new_ctx)
          (void)vt_context_destroy (new_ctx, NULL);
        if (new_pool)
          (void)vt_thread_pool_destroy (new_pool, NULL);
        break;
    }

    if (ret < 0) {
      if (errno != EINTR)
        vt_fatal ("%s: select: %s", __func__, strerror (errno));
      /* ignore */
    } else if (ret > 0) {
      if ((conn = accept (sock, (struct sockaddr *)&their_addr, &addr_size)) < 0)
        vt_fatal ("%s: accept: %s", __func__, strerror (errno));
      if (! (task = calloc (1, sizeof (int))))
        vt_fatal ("%s: calloc: %s", __func__, strerror (errno));
      *task = conn;
      vt_thread_pool_push (pool, (void *)task, &err);
      // FIXME: handle more errors etc!
    }
  }

  // terminate
  (void)close (sock); // probably needs to be done differently!
  //
  vt_cleanup (pool, ctx, 0);
  // 0. close socket
  //    if there is one... etc!
  // 1. kill work force
  // 2. kill context
  // 3. exit
  /* cleanup workers and context */
  //vt_thread_pool_destroy (workers, &err);

  /* remove pid file */
  // IMPLEMENT

  return EXIT_SUCCESS;
}
