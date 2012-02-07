/* system includes */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* valiant includes */
#include "error.h"
#include "signal.h"

/* prototypes */
void *vt_signal_worker (void *);

int _signal = 0;

void
vt_signal_thread (void)
{
  int fds[2];
  pthread_t thread;
  sigset_t sigset;

  sigfillset (&sigset);
  pthread_sigmask (SIG_BLOCK, &sigset, NULL);

  /* create self-pipe here to notify the main thread of any received signals */
  if (pipe (fds) < 0)
    vt_fatal ("%s: pipe: %s", __func__, strerror (errno));

  // set to non-blocking
  //fcntl(F_SETFL) to mark both ends non-blocking (O_NONBLOCK). 

  if (pthread_create (&sig_thread, NULL, &vt_signal_worker, NULL) != 0)
    vt_fatal ("%s: pthread_create: %s", __func__, strerror (errno));
}

void *
vt_signal_worker (void *arg)
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
