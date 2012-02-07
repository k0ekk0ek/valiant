#ifndef VT_CONFIG_H_INCLUDED
#define VT_CONFIG_H_INCLUDED

/* The interface is strongly inspired by libConfuse, which was replaced because
   it didn't have all the options valiant required. */

/* system includes */
#include <limits.h>
#include <stdbool.h>

/* valiant includes */
#include "lexer.h"

#define VT_CONFIG_NAME_MAX (32)
#define VT_CONFIG_PATH_MAX (PATH_MAX)
#define VT_CONFIG_TITLE_MAX (32)

typedef enum _vt_config_type vt_config_type_t;

enum _vt_config_type {
  VT_CONFIG_TYPE_NONE,
  VT_CONFIG_TYPE_FILE, /* vt_config_t with list of sub options */
  VT_CONFIG_TYPE_SEC, /* vt_config_t with list of sub options */
  VT_CONFIG_TYPE_BOOL, /* vt_config_t with list of boolean value(s) */
  VT_CONFIG_TYPE_INT, /* vt_config_t with list of integer value(s) */
  VT_CONFIG_TYPE_FLOAT, /* vt_config_t with list of floating point value(s) */
  VT_CONFIG_TYPE_STRING /* vt_config_t with list of string value(s) */
};

#define VT_CONFIG_FLAG_NONE (0)
#define VT_CONFIG_FLAG_MULTI (1<<1)
#define VT_CONFIG_FLAG_LIST (1<<2)
#define VT_CONFIG_FLAG_NOCASE (1<<3)
#define VT_CONFIG_FLAG_TITLE (1<<4)
#define VT_CONFIG_FLAG_NODEFAULT (1<<5)
#define VT_CONFIG_FLAG_NODUPES (1<<6)

typedef struct _vt_config vt_config_t;
typedef struct _vt_config_file vt_config_file_t;
typedef struct _vt_config_sec vt_config_sec_t;
typedef struct _vt_config_opt vt_config_opt_t;
typedef union _vt_config_mbr vt_config_mbr_t;

struct _vt_config_file {
  char path[VT_CONFIG_PATH_MAX + 1];
  vt_config_t **opts;
  unsigned int nopts;
};

struct _vt_config_sec {
  char title[VT_CONFIG_TITLE_MAX + 1];
  vt_config_t **opts;
  unsigned int nopts;
};

struct _vt_config_opt {
  vt_value_t **vals;
  unsigned int nvals;
};

union _vt_config_mbr {
  vt_config_file_t file;
  vt_config_sec_t sec;
  vt_config_opt_t opt;
};

struct _vt_config {
  char name[VT_CONFIG_NAME_MAX + 1];
  vt_config_type_t type;
  unsigned int flags;
  int line;
  int column;
  vt_config_mbr_t data;
};

/* *_def_* structs, and unions below are used to define a configuration. This
   allows us to be very strict about what sections and options are allowed, and
   at the same time be very dynamic. For instance we allow the programmer to
   specify custom delimiters and custom seperators per option. */
typedef struct _vt_config_def vt_config_def_t;
typedef struct _vt_config_def_sec vt_config_def_sec_t;
typedef struct _vt_config_def_opt vt_config_def_opt_t;
typedef union _vt_config_def_mbr vt_config_def_mbr_t;

struct  _vt_config_def_sec {
  char *title; /* chars allowed in title */
  vt_config_def_t *opts;
};

struct _vt_config_def_opt {
  char *dquot; /* chars that count as double quote, defaults to "\"" */
  char *squot; /* chars that count as single quote, defaults to "\'" */
  vt_value_t val; /* default value */
};

union _vt_config_def_mbr {
  vt_config_def_sec_t sec;
  vt_config_def_opt_t opt;
};

typedef int(*vt_config_validate_func_t)(vt_config_t *, int *);

struct _vt_config_def {
  char *name;
  vt_config_type_t type;
  unsigned int flags;
  vt_config_def_mbr_t data;
  vt_config_validate_func_t validate_func;
};


vt_config_t *vt_config_parse_str (vt_config_def_t *, const char *, size_t,
  int *);
vt_config_t *vt_config_parse_file (vt_config_def_t *, const char *,
  int *);

#define VT_CONFIG_BOOL(name, def, flags) \
  { (name), VT_CONFIG_TYPE_BOOL, (flags), \
    {.opt = {NULL, NULL, {VT_VALUE_TYPE_BOOL, {.bln = (def) ? true : false }}}}}
#define VT_CONFIG_BOOL_LIST(name, def, flags) \
  { (name), VT_CONFIG_TYPE_BOOL, ((flags) | VT_CONFIG_FLAG_LIST), \
    {.opt = {NULL, NULL, {VT_VALUE_TYPE_BOOL, {.bln = (def) ? true : false }}}}}
#define VT_CONFIG_INT(name, def, flags) \
  { (name), VT_CONFIG_TYPE_INT, (flags), \
    {.opt = {NULL, NULL, {VT_VALUE_TYPE_INT, {.lng = (def) }}}}}
#define VT_CONFIG_INT_LIST(name, def, flags) \
  { (name), VT_CONFIG_TYPE_INT, ((flags) | VT_CONFIG_FLAG_LIST), \
    {.opt = {NULL, NULL, {VT_VALUE_TYPE_INT, {.lng = (def) }}}}}
#define VT_CONFIG_FLOAT(name, def, flags) \
  { (name), VT_CONFIG_TYPE_FLOAT, (flags), \
    {.opt = {NULL, NULL, {VT_VALUE_TYPE_FLOAT, {.dbl = (def) }}}}}
#define VT_CONFIG_FLOAT_LIST(name, def, flags) \
  { (name), VT_CONFIG_TYPE_FLOAT, ((flags) | VT_CONFIG_FLAG_LIST), \
    {.opt = {NULL, NULL, {VT_VALUE_TYPE_FLOAT, {.dbl = (def) }}}}}
#define VT_CONFIG_STRING(name, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {"\"", "\'", {VT_VALUE_TYPE_STR, {.str = (def) }}}}}
#define VT_CONFIG_STRING_LIST(name, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, ((flags) | VT_CONFIG_FLAG_LIST), \
    {.opt = {"\"", "\'", {VT_VALUE_TYPE_STR {.str = (dev) }}}}}
#define VT_CONFIG_STRING_QUOT(name, dquot, squot, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {(dquot), (squot), {VT_VALUE_TYPE_STR, {.str = (dev) }}}}}
#define VT_CONFIG_STR_DQUOT(name, dquot, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {(dquot), "\'", {VT_VALUE_TYPE_STR, {.str = (dev) }}}}}
#define VT_CONFIG_STR_SQUOT(name, squot, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {"\"", (squot), {VT_VALUE_TYPE_STR, {.str = (dev) }}}}}
#define VT_CONFIG_SEC(name, opts, flags) \
  { (name), VT_CONFIG_TYPE_SEC, (flags), \
    {.sec = {VT_CHRS_STR, (opts) }}}
#define VT_CONFIG_SEC_TITLE(name, chrs, opts, flags) \
  { (name), VT_CONFIG_TYPE_SEC, ((flags) | VT_CONFIG_FLAG_TITLE), \
    {.sec = {(chrs), (opts) }}}
#define VT_CONFIG_END() \
  { NULL, VT_CONFIG_TYPE_NONE, 0, \
    {.opt = {NULL, NULL, NULL, {VT_VALUE_TYPE_INT, {.lng = 0 }}}}}

const char *vt_config_getname (vt_config_t *);

int vt_config_isfile (vt_config_t *);
int vt_config_issec (vt_config_t *);
int vt_config_isopt (vt_config_t *);

vt_value_t *vt_config_getnval (vt_config_t *, unsigned int, int *);
unsigned int vt_config_getnvals (vt_config_t *, int *);

long vt_config_getnint (vt_config_t *, unsigned int, int *);
#define vt_config_getint(cfg) (vt_config_getnint ((cfg), 0))

double vt_config_getnfloat (vt_config_t *, unsigned int, int *);
#define vt_config_getfloat(cfg) (vt_config_getnfloat ((cfg), 0))

bool vt_config_getnbool (vt_config_t *, unsigned int, int *);
#define vt_config_getbool(cfg, err) (vt_config_getnbool ((cfg), 0, (err)))

char *vt_config_getnstr (vt_config_t *, unsigned int, int *);
#define vt_config_getstr(cfg, err) (vt_config_getnstr ((cfg), 0, (err)))

/* section interface */
#define vt_config_sec_getname(cfg) (vt_config_getname ((cfg)))
const char *vt_config_sec_gettitle (vt_config_t *, int *);
vt_config_t *vt_config_sec_getnopt (vt_config_t *, const char *, unsigned int,
  int *);
unsigned int vt_config_sec_getnopts (vt_config_t *, int *);

/* specify NULL instead of name in vt_config_get(n)sec(s) functions to operate
   on sections based soley on index */
vt_config_t *vt_config_sec_getnsec (vt_config_t *, const char *, unsigned int,
  int *);
#define vt_config_sec_getsec(cfg, name, err) \
  (vt_config_sec_getnsec ((cfg), (name), 0, (err))
unsigned int vt_config_sec_getnsecs (vt_config_t *, const char *,
  int *);

long vt_config_sec_getnint (vt_config_t *, const char *, unsigned int,
  int *);
#define vt_config_sec_getint(cfg, opt, err) \
  (vt_config_sec_getnint ((cfg), (opt), 0, (err)))

double vt_config_sec_getnfloat (vt_config_t *, const char *, unsigned int,
  int *);
#define vt_config_sec_getfloat(cfg, opt, err) \
  (vt_config_sec_getnfloat ((cfg), (opt), 0, (err)))

bool vt_config_sec_getnbool (vt_config_t *, const char *, unsigned int,
  int *);
#define vt_config_sec_getbool(cfg, opt, err) \
  (vt_config_sec_getnbool ((cfg), (opt), 0, (err)))

char *vt_config_sec_getnstr (vt_config_t *, const char *, unsigned int,
  int *);
#define vt_config_sec_getstr(cfg, opt, err) \
  (vt_config_sec_getnstr ((cfg), (opt), 0, (err))

#endif
