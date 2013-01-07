#ifndef VT_SIGNAL_H_INCLUDED
#define VT_SIGNAL_H_INCLUDED 1

/* Code inspired by the examples in the article "Thread-Specific Data and
   Signal Handling in Multi-Threaded Applications" in Linux Journal.
   http://www.linuxjournal.com/article/2121. */

/* Using "The self-pipe trick" for signal notification was suggested by a
   colleague. http://cr.yp.to/docs/selfpipe.html. */
int vt_signal_thread (void);

#endif
