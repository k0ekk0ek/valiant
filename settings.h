#ifndef VT_SETTINGS_H_INCLUDED
#define VT_SETTINGS_H_INCLUDED

/* system includes */
#include <confuse.h>

unsigned int vt_cfg_size_opts (cfg_t *);
cfg_opt_t *vt_cfg_getnopt (cfg_t *, unsigned int);
unsigned int vt_cfg_opt_parsed (cfg_opt_t *);
cfg_t *vt_cfg_parse (const char *);

#endif
