#ifndef VT_CONF_H_INCLUDED
#define VT_CONF_H_INCLUDED 1

/* system includes */
#include <confuse.h>

/* defaults */
#define VT_CFG_CONFIG_FILE "/etc/valiant/valiant.conf"
#define VT_CFG_PID_FILE "/var/run/valiant.pid"
#define VT_CFG_BIND_ADDR "localhost"
#define VT_CFG_PORT "10025"
#define VT_CFG_SYSLOG_IDENT "valiant"
#define VT_CFG_SYSLOG_FACILITY "mail"
#define VT_CFG_SYSLOG_PRIO "info"
#define VT_CFG_ALLOW_RESP "DUNNO"
#define VT_CFG_BLOCK_RESP "521 Bad reputation"
#define VT_CFG_BLOCK_THRESHOLD (5.0)
#define VT_CFG_DELAY_RESP "421 Questionable reputation, please try again later"
#define VT_CFG_DELAY_THRESHOLD (2.0)
#define VT_CFG_ERROR_RESP "451 Server error, please try again later"

cfg_opt_t *vt_cfg_getoptnum (cfg_t *, unsigned int);
char *vt_cfg_getstr_dup (cfg_t *, const char *);
char *vt_cfg_title_dup (cfg_t *);
int vt_cfg_opt_isset (cfg_opt_t *);
cfg_t *vt_cfg_parse (const char *);

#endif
