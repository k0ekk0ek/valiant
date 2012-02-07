#ifndef VT_CONFIG_PRIV_H_INCLUDED
#define VT_CONFIG_PRIV_H_INCLUDED

/* valiant includes */
#include "vt_config.h"

int vt_config_parse_opt (vt_config_t *, vt_config_def_t *, vt_lexer_t *, int *);
int vt_config_parse_sec (vt_config_t *, vt_config_def_t *, vt_lexer_t *, int *);
int vt_config_parse (vt_config_t *, vt_config_def_t *, vt_lexer_t *, int *);

vt_value_t *vt_config_getnval_priv (vt_config_t *, vt_value_type_t,
  unsigned int, int *);
vt_config_t *vt_config_getnopt_priv (vt_config_t *, vt_value_type_t,
  unsigned int, int *);
#endif
