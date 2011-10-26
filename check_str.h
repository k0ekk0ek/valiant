#ifndef VT_CHECK_STR_H_INCLUDED
#define VT_CHECK_STR_H_INCLUDED

/*
 * str <name> {
 *   attribute = <attribute>
 *   negate = <true|false>
 *   nocase = <true|false>
 *   pattern = <string>
 *   weight = <float>
 * }
*/

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "check.h"
#include "type.h"

vt_type_t *vt_str_type (void);
int vt_str_init (const cfg_t *);
int vt_str_create (vt_check_t **, const cfg_t *);

#endif

