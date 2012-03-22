#ifndef VT_CONFIG_H_INCLUDED
#define VT_CONFIG_H_INCLUDED

/* The interface is strongly inspired by libConfuse, which was replaced because
   it didn't have all the options valiant required. */

/* system includes */
#include <limits.h>
#include <stdbool.h>

/* valiant includes */
#include "vt_lexer.h"
#include "vt_value.h"

#define VT_CONFIG_NAME_MAX (32)
#define VT_CONFIG_PATH_MAX (PATH_MAX)
#define VT_CONFIG_TITLE_MAX (32)

typedef enum _vt_config_type vt_config_type_t;

enum _vt_config_type {
  VT_CONFIG_TYPE_NONE = 0,
  VT_CONFIG_TYPE_FILE,
  VT_CONFIG_TYPE_SEC,
  VT_CONFIG_TYPE_OPT
};

#define VT_CONFIG_FLAG_NONE (0)
#define VT_CONFIG_FLAG_MULTI (1<<1)
#define VT_CONFIG_FLAG_LIST (1<<2)
#define VT_CONFIG_FLAG_NOCASE (1<<3)
#define VT_CONFIG_FLAG_TITLE (1<<4)
#define VT_CONFIG_FLAG_NODEFAULT (1<<5)
#define VT_CONFIG_FLAG_NODUPES (1<<6)

#define VT_CONFIG_INST_NONE (0)
#define VT_CONFIG_INST_SET (1<<1)
#define VT_CONFIG_INST_PREPEND (1<<2) /* implies VT_CONFIG_INST_SET */
#define VT_CONFIG_INST_APPEND (1<<3) /* implies VT_CONFIG_INST_SET */
#define VT_CONFIG_INST_COMPARE (1<<4)
#define VT_CONFIG_INST_RECYCLE (1<<5)

typedef struct _vt_config vt_config_t;
typedef struct _vt_config_file vt_config_file_t;
typedef struct _vt_config_sec vt_config_sec_t;
typedef struct _vt_config_opt vt_config_opt_t;

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
  vt_value_type_t type;
  vt_value_t **vals;
  unsigned int nvals;
};

struct _vt_config {
  char name[VT_CONFIG_NAME_MAX + 1];
  vt_config_type_t type;
  unsigned int flags;
  int line;
  int column;
  union {
    vt_config_file_t file;
    vt_config_sec_t sec;
    vt_config_opt_t opt;
  } data;
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
  vt_value_type_t type;
  vt_value_t val; /* default value */
};

typedef int(*vt_config_validate_func_t)(vt_config_t *, int *);

struct _vt_config_def {
  char *name;
  vt_config_type_t type;
  unsigned int flags;
  union {
    vt_config_def_sec_t sec;
    vt_config_def_opt_t opt;
  } data;
  vt_config_validate_func_t validate_func;
};

vt_config_t *vt_config_create (int *)
int vt_config_init (vt_config_t *, vt_config_def_t *);
int vt_config_destroy (vt_config_t *, int *);
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
#define VT_CONFIG_STR(name, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {"\"", "\'", {VT_VALUE_TYPE_STR, {.str = (def) }}}}}
#define VT_CONFIG_STR_LIST(name, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, ((flags) | VT_CONFIG_FLAG_LIST), \
    {.opt = {"\"", "\'", {VT_VALUE_TYPE_STR {.str = (def) }}}}}
#define VT_CONFIG_STR_QUOT(name, dquot, squot, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {(dquot), (squot), {VT_VALUE_TYPE_STR, {.str = (def) }}}}}
#define VT_CONFIG_STR_DQUOT(name, dquot, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {(dquot), "\'", {VT_VALUE_TYPE_STR, {.str = (def) }}}}}
#define VT_CONFIG_STR_SQUOT(name, squot, def, flags) \
  { (name), VT_CONFIG_TYPE_STR, (flags), \
    {.opt = {"\"", (squot), {VT_VALUE_TYPE_STR, {.str = (def) }}}}}
#define VT_CONFIG_SEC(name, opts, flags) \
  { (name), VT_CONFIG_TYPE_SEC, (flags), \
    {.sec = {VT_CHRS_STR, (opts) }}}
#define VT_CONFIG_SEC_TITLE(name, chrs, opts, flags) \
  { (name), VT_CONFIG_TYPE_SEC, ((flags) | VT_CONFIG_FLAG_TITLE), \
    {.sec = {(chrs), (opts) }}}
#define VT_CONFIG_END() \
  { NULL, VT_CONFIG_TYPE_NONE, 0, \
    {.opt = {NULL, NULL, {VT_VALUE_TYPE_INT, {.lng = 0 }}}}}

char *vt_config_getname (vt_config_t *);

unsigned int vt_config_getnvals (vt_config_t *);

bool vt_config_getnbool (vt_config_t *, int, int *);
#define vt_config_getbool(opt, err) \
  (vt_config_getnbool ((opt), 0, (err)))
int vt_config_getbooln (vt_config_t *, bool, int, int, int *);
int vt_config_setnbool (vt_config_t *, bool, int, int, int *);
#define vt_config_setbool(opt, bln, err) \
  (vt_config_setnbool ((opt), (bln), -1, 0, (err)))
int vt_config_unsetnbool (vt_config_t *, int, int *);

long vt_config_getnint (vt_config_t *, int, int *);
#define vt_config_getint(opt, err) \
  (vt_config_getnint ((opt), 0, (err)))
int vt_config_getintn (vt_config_t *, long, int, int, int *);
int vt_config_setnint (vt_config_t *, long, int, int, int *);
#define vt_config_setint(opt, lng, err) \
  (vt_config_setnint ((opt), (lng), -1, 0, (err)))
int vt_config_unsetnint (vt_config_t *, int, int *);

double vt_config_getnfloat (vt_config_t *, int, int *);
#define vt_config_getfloat(opt, err) \
  (vt_config_getnfloat ((opt), 0, (err)))
int vt_config_getfloatn (vt_config_t *, double, int, int *);
int vt_config_setnfloat (vt_config_t *, double, int, int *);
#define vt_config_setfloat(opt, val, err) \
  (vt_config_setnfloat ((opt), (val), -1, 0, (err)))
int vt_config_unsetnfloat (vt_config_t *, int, int *);

char *vt_config_getnstr (vt_config_t *, int, int *);
#define vt_config_getstr(opt, err) \
  (vt_config_getnstr ((opt), 0, (err)))
char *vt_config_getnstr_dup (vt_config_t *, int, int *);
int vt_config_getstrn (vt_config_t *, const char *, int, int, int *);
int vt_config_setnstr (vt_config_t *, const char *, int, int, int *);
#define vt_config_setstr(opt, str, err) \
  (vt_config_setnstr ((opt), (str), -1, 0, (err)))
int vt_config_unsetnstr (vt_config_t *, int, int *);

char *vt_config_sec_gettitle (vt_config_t *);

int vt_config_sec_getnopts (vt_config_t *);
int vt_config_sec_getnsecs (vt_config_t *, const char *, int, int *);

bool vt_config_sec_getnbool (vt_config_t *, const char *, int, int *);
#define vt_config_sec_getbool(sec, name, err) \
  (vt_config_sec_getnbool ((sec), (name), 0, (err)))
int vt_config_sec_setnbool (vt_config_t *, const char *, bool, int, int,
  int *);
#define vt_config_set_setbool(sec, name, bln, err) \
  (vt_config_sec_setnbool ((sec), (name), (bln), -1, 0, (err)))

long vt_config_sec_getnint (vt_config_t *, const char *, int, int *);
#define vt_config_sec_getint(sec, name, err) \
  (vt_config_sec_getnint ((sec), (name), 0, (err)))
int vt_config_sec_setnint (vt_config_t *, const char *, long, int, int,
  int *);
#define vt_config_set_setint(sec, name, lng, err) \
  (vt_config_sec_setnint ((sec), (name), (lng), -1, 0, (err)))

double vt_config_sec_getnfloat (vt_config_t *, const char *, int, int *);
#define vt_config_sec_getfloat(sec, name, err) \
  (vt_config_sec_getnfloat ((sec), (name), 0, (err)))
int vt_config_sec_setnfloat (vt_config_t *, const char *, double, int, int,
  int *);
#define vt_config_set_setfloat(sec, name, dbl, err) \
  (vt_config_sec_setnfloat ((sec), (name), (dbl), -1, 0, (err)))

char *vt_config_sec_getnstr (vt_config_t *, const char *, int, int *);
#define vt_config_sec_getstr(sec, name, err) \
  (vt_config_sec_getnstr ((sec), (name), 0, (err)))
char *vt_config_sec_getnstr_dup (vt_config_t *, const char *, int, int *);
int vt_config_sec_setnstr (vt_config_t *, const char *, const char *, int, int,
  int *);
#define vt_config_set_setstr(sec, name, str, err) \
  (vt_config_sec_setnstr ((sec), (name), (str), -1, 0, (err)))

vt_config_t *vt_config_sec_getnopt (vt_config_t *, const char *, int, int *);
#define vt_config_sec_getopt(sec, name, err) \
  (vt_config_sec_getnopt ((sec), (name), 0, (err))
int vt_config_sec_setnopt_bool (vt_config_t *, const char *, int, bool,
  int, int, int *);
int vt_config_sec_setnopt_int (vt_config_t *, const char *, int, long,
  int, int, int *);
int vt_config_sec_setnopt_float (vt_config_t *, const char *, int, double,
  int, int, int *);
int vt_config_sec_setnopt_str (vt_config_t *, const char *, int, const char *,
  int, int, int *);
int vt_config_sec_unsetnopt (vt_config_t *, const char *, int, int, int *);
#define vt_config_sec_unsetopt(sec, name, err) \
  (vt_config_sec_unsetnopt ((sec), (name), -1, VT_CONFIG_INST_COMPARE, (err)))

vt_config_t *vt_config_sec_getnsec (vt_config_t *, const char *, int, int,
  int *);
#define vt_config_sec_getsec(sec, name, err) \
  (vt_config_sec_getnsec ((sec), (name), 0, (err))
int vt_config_sec_setnsec (vt_config_t *, const char *, int, int, int, int *);
#define vt_config_sec_setsec(sec, name, flags, err) \
  (vt_config_sec_setnsec ((sec), (name), (flags), -1, 0, (err)))

#endif
