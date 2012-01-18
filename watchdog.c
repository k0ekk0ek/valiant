/* system includes */
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "error.h"
#include "watchdog.h"

/* prototypes */
void *vt_watchdog (void *);

int _signal = 0;

void
vt_watchdog_init (void)
{
  sigset_t sigset;
  pthread_t sig_thread;

  sigfillset (&sigset);
  pthread_sigmask (SIG_BLOCK, &sigset, NULL);

  if (pthread_create (&sig_thread, NULL, &vt_watchdog, NULL) != 0)
    vt_fatal ("%s: pthread_create: %s", __func__, strerror (errno));
}

int
vt_watchdog_signal (void)
{
  int sig;

  return __sync_lock_test_and_set (&_signal, 0);
}

void *
vt_watchdog (void *arg)
{
  int dead;
  int prev_sig, sig;
  sigset_t sigset;

  for (dead = 0; ! dead; ) {
    sigfillset (&sigset);
    sigwait (&sigset, &sig);

    switch (sig) {
      case SIGINT:
      case SIGTERM:
        /* terminate */
        __sync_lock_test_and_set (&_signal, SIGTERM);
        vt_info ("caught SIGTERM, will terminate");
        dead = 1;
        break;
      case SIGHUP:
        /* reload */
        if (__sync_bool_compare_and_swap (&_signal, 0, SIGHUP))
          vt_info ("caught SIGHUP, will reload");
        else
          vt_info ("caught SIGHUP, ignored");
        break;
      default:
        /* do nothing */
        break;
    }
  }

  return NULL;
}
