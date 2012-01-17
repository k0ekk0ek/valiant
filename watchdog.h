#ifndef VT_WATCHDOG_H_INCLUDED
#define VT_WATCHDOG_H_INCLUDED 1

/* Watchdog implementation inspired by the examples in the article
   "Thread-Specific Data and Signal Handling in Multi-Threaded Applications"
   in Linux Journal. http://www.linuxjournal.com/article/2121. */

void vt_watchdog_init (void);
int vt_watchdog_signal (void);

#endif
