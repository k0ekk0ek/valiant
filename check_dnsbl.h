#ifndef _CHECK_DNSBL_H_INCLUDED_
#define _CHECK_DNSBL_H_INCLUDED_

/* valiant includes */
#include "check.h"

check_t *check_dnsbl_create (int, int, const char *);
check_t *cfg_to_check_dnsbl (cfg_t *);

#endif
