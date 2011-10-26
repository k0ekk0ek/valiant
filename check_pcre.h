#ifndef VT_CHECK_PCRE_H_INCLUDED
#define VT_CHECK_PCRE_H_INCLUDED

/*
 * pcre <name> {
 *   attribute = <attribute>
 *   negate = <true|false>
 *   nocase = <true|false>
 *   pattern = <pcre>
 *   weight = <float>
 * }
*/

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "check.h"
#include "type.h"

vt_type_t *vt_pcre_type (void);
int vt_pcre_init (cfg_t *);
int vt_pcre_create (vt_check_t **, const cfg_t *);

#endif

