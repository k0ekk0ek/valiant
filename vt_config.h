#ifndef VT_CONFIG_H_INCLUDED
#define VT_CONFIG_H_INCLUDED

/* system includes */
#include <stdbool.h>

#include "vt_error.h"

#define VT_CONFIG_NAME_MAX (32)

typedef union _vt_config_value vt_config_value_t;

union _vt_config_value {
  unsigned long lng; /* integer value */
  double dbl; /* floating point value */
  bool bln; /* boolean value */
  char *str; /* string value */
};

typedef enum _vt_config_option_type vt_config_option_type_t;

enum _vt_config_option_type {
  VT_CONFIG_OPT_TYPE_INT,
  VT_CONFIG_OPT_TYPE_FLOAT,
  VT_CONFIG_OPT_TYPE_BOOL,
  VT_CONFIG_OPT_TYPE_STR
};

typedef struct _vt_config_def vt_config_def_t;

typedef struct _vt_config_def_section vt_config_def_section_t;

struct _vt_config_def_section {
  vt_config_def_t *opts;
};

typedef struct _vt_config_def_option vt_config_def_option_t;

struct _vt_config_def_option {
  vt_config_option_type_t type;
  char *delim;
  char *sep;
  vt_config_value_t val;
};

typedef union _vt_config_def_member vt_config_def_member_t;

union _vt_config_def_member {
  vt_config_def_option_t opt;
  vt_config_def_section_t sec;
};


typedef enum _vt_config_type vt_config_type_t;

enum _vt_config_type {
  VT_CONFIG_TYPE_NONE,
  VT_CONFIG_TYPE_FILE,
  VT_CONFIG_TYPE_SEC,
  VT_CONFIG_TYPE_OPT,
};


struct _vt_config_def {
  char *name;
  vt_config_type_t type;
  //vt_config_flags_t flags;
  vt_config_def_member_t data;
  // validate func
  //
};

typedef struct _vt_config vt_config_t;

struct _vt_config {
  /* file, section, or option */
  vt_config_type_t type; // replace by flags?!?!
  char name[VT_CONFIG_NAME_MAX + 1];
  int colno;
  int lineno;
  /* member of type vt_config_file_t, vt_config_section_t, or
     vt_config_option_t that is used for storing the actual data. */
  //vt_config_data_t data;
};

#define VT_CONFIG_DEF_INT(name, def, flags) \
  { (name), VT_CONFIG_TYPE_OPT, {.opt = {VT_CONFIG_OPT_TYPE_INT, NULL, ",", {.lng = (def) }}}}
#define VT_CONFIG_DEF_FLOAT(name, def, flags) \
  { (name), VT_CONFIG_TYPE_DBL, {.opt = { VT_CONFIG_OPT_TYPE_FLOAT, NULL, ",", {.dbl = (def) }}}}
#define VT_CONFIG_DEF_BOOL(name, def, flags) \
  { (name), VT_CONFIG_TYPE_OPT, {.opt = { VT_CONFIG_OPT_TYPE_BOOL, NULL, NULL, {.bln = (def) ? true : false }}}}
#define VT_CONFIG_DEF_STR(name, def, flags) \
  { (name), VT_CONFIG_TYPE_OPT, {.opt = { VT_CONFIG_OPT_TYPE_STR, NULL, ",", {.str = (def) }}}}
#define VT_CONFIG_DEF_SEC(name, arr, flags) \
  { (name), VT_CONFIG_TYPE_SEC, {.sec = { (opts) }}}
#define VT_CONFIG_DEF_END() \
  { NULL, VT_CONFIG_TYPE_NONE, {.opt = {VT_CONFIG_OPT_TYPE_INT, NULL, NULL, {.lng = 0 }}}}
#endif

vt_config_t *vt_config_parse_buffer (vt_config_def_t *def,
                        const char *str,
                        size_t len,
                        vt_error_t *err);
vt_config_t *vt_config_parse_file (vt_config_def_t *def, const char *path, vt_error_t *err);

















