#ifndef VT_CHECK_DNSBL_H_INCLUDED
#define VT_CHECK_DNSBL_H_INCLUDED

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "check.h"

vt_check_type_t *vt_check_dnsbl_type (void);
int vt_check_dnsbl_init (const cfg_t *);
int vt_check_dnsbl_create (vt_check_t **, const cfg_t *);

#endif

