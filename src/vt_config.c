/* system includes */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "vt_buf.h"
#include "vt_config_priv.h"
#include "vt_lexer.h"

vt_config_t *
vt_config_create (vt_config_t *cfg, int *err)
{
  unsigned int nopts;
  vt_config_t *opt, **opts;

  if (! (opt = calloc (1, sizeof (vt_config_t)))) {
    vt_set_errno (err, errno);
    vt_error ("%s: calloc: %s", __func__, vt_strerror (errno));
    goto failure;
  }

  if (cfg) {
    if (cfg->type == VT_CONFIG_TYPE_FILE) {
      nopts = cfg->data.file.nopts + 1;
      opts = realloc (cfg->data.file.opts, nopts * sizeof (vt_config_t));
    } else if (cfg->type == VT_CONFIG_TYPE_SEC) {
      nopts = cfg->data.sec.nopts + 1;
      opts = realloc (cfg->data.sec.opts, nopts * sizeof (vt_config_t));
    } else {
      vt_panic ("%s: config_t not of type file, or section", __func__);
    }

    if (! opts) {
      vt_set_errno (err, errno);
      vt_error ("%s: realloc: %s", __func__, vt_strerror (errno));
      goto failure;
    }

    opts[nopts - 1] = opt;

    if (cfg->type == VT_CONFIG_TYPE_FILE) {
      cfg->data.file.opts = opts;
      cfg->data.file.nopts = nopts;
    } else {
      cfg->data.sec.opts = opts;
      cfg->data.sec.nopts = nopts;
    }
  }

  return opt;
failure:
  if (opt)
    free (opt);
  return NULL;
}

vt_value_t *
vt_config_create_value (vt_config_t *cfg, int *err)
{
  unsigned int nvals;
  vt_value_t *val, **vals;

  if (! (val = calloc (1, sizeof (vt_value_t)))) {
    vt_set_errno (err, ENOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    return NULL;
  }

  if (cfg) {
    switch (cfg->type) {
      case VT_CONFIG_TYPE_BOOL:
        val->type = VT_VALUE_TYPE_BOOL;
        break;
      case VT_CONFIG_TYPE_INT:
        val->type = VT_VALUE_TYPE_INT;
        break;
      case VT_CONFIG_TYPE_FLOAT:
        val->type = VT_VALUE_TYPE_FLOAT;
        break;
      case VT_CONFIG_TYPE_STR:
        val->type = VT_VALUE_TYPE_STR;
        break;
      default:
        vt_panic ("%s: option has unsupported value type", __func__);
    }

    nvals = cfg->data.opt.nvals + 1;
    if (! (vals = realloc (cfg->data.opt.vals, nvals * sizeof (vt_value_t)))) {
      vt_set_errno (err, ENOMEM);
      vt_error ("%s: realloc: %s", __func__, strerror (errno));
      goto failure;
    }

    vals[nvals - 1] = val;

    cfg->data.opt.vals = vals;
    cfg->data.opt.nvals = nvals;
  }

  return val;
failure:
  if (val)
    free (val);
  return NULL;
}


void
vt_config_init_file (vt_config_t *sec)
{
  assert (sec);
  sec->type = VT_CONFIG_TYPE_FILE;
  memset (sec->data.file.path, '\0', VT_CONFIG_PATH_MAX);
  sec->data.file.opts = NULL;
  sec->data.file.nopts = 0;
}

void
vt_config_init_sec (vt_config_t *sec)
{
  assert (sec);
  sec->type = VT_CONFIG_TYPE_SEC;
  memset (sec->data.sec.title, '\0', VT_CONFIG_TITLE_MAX);
  sec->data.sec.opts = NULL;
  sec->data.sec.nopts = 0;
}

void
vt_config_init_opt (vt_config_t *opt, vt_config_def_t *def)
{
  assert (opt);
  opt->type = def->type;
  opt->data.opt.vals = NULL;
  opt->data.opt.nvals = 0;
}

void
vt_config_prep_lexer_def (vt_lexer_def_t *lxr_def, vt_config_def_t *def)
{
  assert (lxr_def);
  assert (def);

  vt_lexer_def_reset (lxr_def);

  switch (def->type) {
    case VT_CONFIG_TYPE_SEC:
      if (def->data.sec.title)
        lxr_def->str = def->data.sec.title;
      else
        lxr_def->str = VT_CHRS_STR;
      break;
    case VT_CONFIG_TYPE_BOOL:
      lxr_def->scan_bool = 1;
      lxr_def->scan_binary = 0;
      lxr_def->scan_octal = 0;
      lxr_def->scan_float = 0;
      lxr_def->scan_int = 0;
      lxr_def->scan_hex = 0;
      lxr_def->scan_hex_dollar = 0;
      lxr_def->scan_str_dquot = 0;
      lxr_def->scan_str_squot = 0;
      break;
    case VT_CONFIG_TYPE_INT:
      lxr_def->scan_bool = 0;
      lxr_def->scan_float = 0;
      lxr_def->scan_str_dquot = 0;
      lxr_def->scan_str_squot = 0;
      lxr_def->numbers_as_int = 1;
      lxr_def->int_as_float = 0;
      break;
    case VT_CONFIG_TYPE_FLOAT:
      lxr_def->scan_bool = 0;
      lxr_def->scan_str_dquot = 0;
      lxr_def->scan_str_squot = 0;
      lxr_def->numbers_as_int = 1;
      lxr_def->int_as_float = 1;
      break;
    case VT_CONFIG_TYPE_STR:
      lxr_def->scan_bool = 0;
      lxr_def->scan_binary = 0;
      lxr_def->scan_octal = 0;
      lxr_def->scan_float = 0;
      lxr_def->scan_int = 0;
      lxr_def->scan_hex = 0;
      lxr_def->scan_hex_dollar = 0;
      lxr_def->dquot = def->data.opt.dquot;
      lxr_def->squot = def->data.opt.squot;
      break;
  }
}

int
vt_config_parse_sec (vt_config_t *sec,
                     vt_config_def_t *def,
                     vt_lexer_t *lxr,
                     int *err)
{
  int loop, title;
  vt_config_def_t *defs;
  vt_lexer_def_t lxr_def;
  vt_token_t token;

  assert (sec);
  assert (def);
  assert (lxr);

  vt_config_prep_lexer_def (&lxr_def, def);

  for (loop = 1; loop;) {
    token = vt_lexer_get_token (lxr, &lxr_def, err);

    switch (token) {
      case VT_TOKEN_EOF:
        vt_set_errno (err, EINVAL);
        vt_error ("unexpected end of file");
      case VT_TOKEN_ERROR:
        goto failure;
      case VT_TOKEN_COMMENT_SINGLE:
      case VT_TOKEN_COMMENT_MULTI:
        /* always ignore comments */
        break;
      case VT_TOKEN_LEFT_CURLY:
        /* verify we have a title if configured */
        if (def->flags & VT_CONFIG_FLAG_TITLE && ! strlen (sec->data.sec.title)) {
          vt_set_errno (err, EINVAL);
          vt_error ("unexpected curly bracket on line %u, column %u",
            lxr->line, lxr->column);
          goto failure;
        }

        return vt_config_parse (sec, def->data.sec.opts, lxr, err);
      case VT_TOKEN_STR:
        /* verify section is supposed to have a title */
        if (! (def->flags & VT_CONFIG_FLAG_TITLE) || strlen (sec->data.sec.title)) {
          vt_set_errno (err, EINVAL);
          vt_error ("unexpected title on line %u, column %u",
            lxr->line, lxr->column);
          goto failure;
        } else {
          sec->flags |= VT_CONFIG_FLAG_TITLE;
          (void)strncpy (sec->data.sec.title, lxr->value.data.str, VT_CONFIG_TITLE_MAX);
        }
        break;
      default:
        /* all other tokens are illegal by default */
        vt_set_errno (err, EINVAL);
        if (token < VT_TOKEN_NONE)
          vt_error ("unexpected character %c on line %u, column %u",
            lxr->value.data.chr, lxr->line, lxr->column);
        else
          vt_error ("unexpected token on line %u, column %u",
            lxr->line, lxr->column);
        goto failure;
    }
  }

  return 0;
failure:
  /* cleanup */
  return -1;
}

int
vt_config_parse_opt (vt_config_t *opt,
                     vt_config_def_t *def,
                     vt_lexer_t *lxr,
                     int *err)
{
  char *spec;
  int loop, state;
  vt_lexer_def_t lxr_def;
  vt_token_t token;
  vt_value_t *val;

  vt_config_prep_lexer_def (&lxr_def, def);

  for (loop = 1, state = 0; loop;) {
    token = vt_lexer_get_next_token (lxr, &lxr_def, err);
    switch (token) {
    case VT_TOKEN_EOF:
      if (state == 0)
        vt_error ("unexpected end of file");
      return 0;
    case VT_TOKEN_ERROR:
      goto failure;
    case VT_TOKEN_EQUAL_SIGN:
vt_error ("equal sign");
      if (state != 0)
        goto unexpected;
      state = 1;
      break;
    case VT_TOKEN_COMMA:
      if (state != 2)
        goto unexpected;
      state = 1;
      break;
    case VT_TOKEN_BOOL:
    case VT_TOKEN_INT:
    case VT_TOKEN_FLOAT:
    case VT_TOKEN_STR:
      if (state == 2 && token == VT_TOKEN_STR)
        return 0;
      if (state != 1 || (def->type == VT_CONFIG_TYPE_BOOL  && lxr->value.type != VT_VALUE_TYPE_BOOL)   ||
                        (def->type == VT_CONFIG_TYPE_INT   && lxr->value.type != VT_VALUE_TYPE_INT)    ||
                        (def->type == VT_CONFIG_TYPE_FLOAT && lxr->value.type != VT_VALUE_TYPE_FLOAT)  ||
                        (def->type == VT_CONFIG_TYPE_STR   && lxr->value.type != VT_VALUE_TYPE_STR))
        goto unexpected;
      if (! (val = vt_config_create_value (opt, err)))
        goto failure;
      vt_error ("value, token: %d", token);
      //vt_error ("def flags2: %u", def->flags);
      /* populate value */
      val->type = lxr->value.type;
        //vt_error ("value type: %d", val->type);
      if (lxr->value.type == VT_VALUE_TYPE_BOOL) {
        //vt_error ("boolean value: %s", val->data.bln ? "true" : "false");
        val->data.bln = lxr->value.data.bln;
      } else if (lxr->value.type == VT_VALUE_TYPE_INT) {
        //vt_error ("value: %d", lxr->value.data.lng);
        val->data.lng = lxr->value.data.lng;
      } else if (lxr->value.type == VT_VALUE_TYPE_FLOAT) {
        val->data.dbl = lxr->value.data.dbl;
      } else if (lxr->value.type == VT_VALUE_TYPE_STR) {
        val->data.str = strdup (lxr->value.data.str);
        if (! val->data.str) {
          vt_set_errno (err, ENOMEM);
          vt_error ("%s: strdup: %s", __func__, strerror (errno));
          goto failure;
        }
      }
      //vt_error ("def flags3: %u", def->flags);
      if (! (def->flags & VT_CONFIG_FLAG_LIST))
        return 0;
      //vt_error ("wtf");
      state = 2;
      break;
    default:
unexpected:
      if (token < VT_TOKEN_NONE) {
        vt_error ("unexpected character %c on line line %u, column %u",
          token, lxr->line, lxr->column);
      } else {
        if (token == VT_TOKEN_BOOL)
          spec = "boolean value";
        else if (token == VT_TOKEN_INT)
          spec = "integer value";
        else if (token == VT_TOKEN_FLOAT)
          spec = "floating point value";
        else if (token == VT_TOKEN_STR)
          spec = "string value";
        else
          spec = "value";
        vt_error ("unexpected %s on line %u, column %u",
          spec, lxr->line, lxr->column);
      }
      vt_set_errno (err, EINVAL);
      goto failure;
    }
  }

  return 0;
failure:
  /* cleanup */
  return -1;
}

int
vt_config_parse (vt_config_t *sec,
                 vt_config_def_t *defs,
                 vt_lexer_t *lxr,
                 int *err)
{
  int loop;
  vt_config_t *opt;
  vt_config_def_t *def;
  vt_lexer_def_t lxr_def;
  vt_token_t token;

  assert (sec);
  assert (defs);
  assert (lxr);

  vt_lexer_def_reset (&lxr_def);

  for (loop = 1; loop;) {
    token = vt_lexer_get_next_token (lxr, &lxr_def, err);
inspect:
    switch (token) {
      case VT_TOKEN_EOF:
        // FIXME: IMPLEMENT
        // if we're in a section... that's an error
        loop = 0;
        break;
      case VT_TOKEN_ERROR:
        // FIXME: IMPLEMENT
        loop = 0;
        break;
      case VT_TOKEN_COMMENT_SINGLE:
      case VT_TOKEN_COMMENT_MULTI:
        /* always ignore comments */
        break;
      case VT_TOKEN_RIGHT_CURLY:
        if (sec->type == VT_CONFIG_TYPE_FILE)
          goto unexpected;
        return 0;
        break;
      case VT_TOKEN_STR:
        /* option or section name must be known in config defs */
        for (def = defs; def->type != VT_CONFIG_TYPE_NONE; def++) {
          vt_error ("def->name: %s", def->name);
          if (def->name && strcmp (lxr->value.data.str, def->name) == 0)
            break;
        }

        if (def->type == VT_CONFIG_TYPE_NONE) {
          vt_set_errno (err, EINVAL);
          vt_error ("unknown option %s on line %u, column %u",
            lxr->value.data.str, lxr->line, lxr->column);
          goto failure;
        }

        if (! (opt = vt_config_create (sec, err)))
          goto failure;

        (void) strncpy (opt->name, lxr->value.data.str, VT_CONFIG_NAME_MAX);
        if (def->type == VT_CONFIG_TYPE_SEC) {
          vt_config_init_sec (opt);
          if (vt_config_parse_sec (opt, def, lxr, err) < 0)
            goto failure;
        } else {
vt_error ("option");
          vt_config_init_opt (opt, def);
          if (vt_config_parse_opt (opt, def, lxr, err) < 0)
            goto failure;
vt_error ("option done");
          /* option parser is always one token ahead, except for end of file */
          if (def->flags & VT_CONFIG_FLAG_LIST)
            goto inspect;
        }
        break;
      default:
unexpected:
        // FIXME: IMPLEMENT
        vt_error ("%s: unexpected, FIX ERROR MESSAGE ;)", __func__);
        goto failure;
    }



  }

  return 0;
failure:
  /* cleanup */
  return -1;
}


vt_config_t *
vt_config_parse_str (vt_config_def_t *defs,
                     const char *str,
                     size_t len,
                     int *err)
{
  vt_config_def_t *root_def, *prev_def, *next_def, *itr;
  vt_lexer_t lxr;
  vt_token_t tok;

  assert (defs);
  assert (str);

  // 0. populate lexer def, which is used for global defaults!

  // 1. create shiny new lexer
  vt_config_t *cfg;
  memset (&lxr, 0, sizeof (vt_lexer_t));
  lxr.str = (char *)str;
  lxr.len = len;
  lxr.lines = 1;
  lxr.columns = 1;
  //if (! (cfg = vt_lexer_create (NULL, err)))
  //  return NULL;

  //root_def = def;
  //prnt_def = def;

  if (! (cfg = vt_config_create (NULL, err)))
    goto failure;
  vt_config_init_file (cfg);
  if (vt_config_parse (cfg, defs, &lxr, err) < 0)
    goto failure;
  return cfg;
failure:
  /* FIXME: IMPLEMENT: CLEANUP */
  return NULL;
}


vt_config_t *
vt_config_parse_file (vt_config_def_t *def, const char *path, int *err)
{
#define BUFLEN (4096)
  char rdbuf[BUFLEN];
  char *str;
  int fd;
  int orig_errno;
  size_t len;
  ssize_t nrd;
  vt_buf_t buf;
  vt_config_t *cfg;

  assert (def);
  assert (path);

  if ((fd = open (path, O_RDONLY)) < 0) {
    vt_set_errno (err, errno);
    vt_error ("%s: open: %s", __func__, strerror (errno));
    goto failure;
  }
  if (vt_buf_init (&buf, BUFLEN) < 0) {
    vt_set_errno (err, ENOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto failure;
  }

  orig_errno = errno;
  errno = 0;
  for (;;) {
again:
    nrd = read (fd, rdbuf, BUFLEN);

    if (nrd > 0) {
      if (vt_buf_ncat (&buf, rdbuf, nrd) < 0) {
        vt_set_errno (err, ENOMEM);
        vt_error ("%s: %s", __func__, strerror (errno));
        goto failure;
      }
    }
    if (nrd < BUFLEN) {
      if (errno) {
        if (errno == EINTR)
          goto again;
        vt_set_errno (err, errno);
        vt_error ("%s: read: %s", __func__, strerror (errno));
        goto failure;
      }
      break; /* end of file */
    }
  }

  (void)close (fd);
  errno = orig_errno;

  str = vt_buf_str (&buf);
  len = vt_buf_len (&buf);

  //vt_error ("conf: %s\n", str);

  if (! (cfg = vt_config_parse_str (def, str, len, err)))
    goto failure;

  return cfg;
failure:
  // clean up
  return NULL;
#undef BUFLEN
}

/**
 ** INTERFACE FUNCTIONS
 **/

char *
vt_config_getname (vt_config_t *cfg)
{
  assert (cfg);

  return cfg->name;
}

vt_value_t *
vt_config_getnval (vt_config_t *opt, unsigned int idx, int *err)
{
  assert (opt);
  assert (opt->type == VT_CONFIG_TYPE_BOOL  || opt->type == VT_CONFIG_TYPE_INT
       || opt->type == VT_CONFIG_TYPE_FLOAT || opt->type == VT_CONFIG_TYPE_STR);

  if (idx <= opt->data.opt.nvals)
    return opt->data.opt.vals[idx];
  vt_set_errno (err, EINVAL);
  return NULL;
}

vt_value_t *
vt_config_getnval_priv (vt_config_t *opt, vt_value_type_t type,
  unsigned int idx, int *err)
{
  vt_value_t *val;

  assert (opt);

  if (! (val = vt_config_getnval (opt, idx, err)))
    return NULL;
vt_error ("opt: %s, %d:%d", opt->name, val->type, type);
  assert (val->type == type);

  return val;
}

unsigned int
vt_config_getnvals (vt_config_t *opt)
{
  assert (opt);
  assert (opt->type == VT_CONFIG_TYPE_BOOL  || opt->type == VT_CONFIG_TYPE_INT
       || opt->type == VT_CONFIG_TYPE_FLOAT || opt->type == VT_CONFIG_TYPE_STR);

  return opt->data.opt.nvals;
}

bool
vt_config_getnbool (vt_config_t *opt, unsigned int idx, int *err)
{
  vt_value_t *val;

  assert (opt);
  assert (opt->type == VT_CONFIG_TYPE_BOOL);

  if (! (val = vt_config_getnval_priv (opt, VT_VALUE_TYPE_BOOL, idx, err)))
    return false;
  return val->data.bln;
}

long
vt_config_getnint (vt_config_t *opt, unsigned int idx, int *err)
{
  vt_value_t *val;

  assert (opt);
  assert (opt->type == VT_CONFIG_TYPE_INT);

  if (! (val = vt_config_getnval_priv (opt, VT_VALUE_TYPE_INT, idx, err)))
    return 0;
  return val->data.lng;
}

double
vt_config_getnfloat (vt_config_t *opt, unsigned int idx, int *err)
{
  vt_value_t *val;

  assert (opt);
  assert (opt->type == VT_CONFIG_TYPE_FLOAT);

  if (! (val = vt_config_getnval_priv (opt, VT_VALUE_TYPE_FLOAT, idx, err)))
    return 0.0;
  return val->data.dbl;
}

char *
vt_config_getnstr (vt_config_t *opt, unsigned int idx, int *err)
{
  vt_value_t *val;

  assert (opt);
  assert (opt->type == VT_CONFIG_TYPE_STR);

  if (! (val = vt_config_getnval_priv (opt, VT_VALUE_TYPE_STR, idx, err)))
    return NULL;
  return val->data.str;
}

char *
vt_config_sec_gettitle (vt_config_t *sec)
{
  assert (sec);
  assert (sec->type == VT_CONFIG_TYPE_SEC);

  if (sec->flags & VT_CONFIG_FLAG_TITLE)
    return sec->data.sec.title;
  return NULL;
}

vt_config_t *
vt_config_sec_getnopt (vt_config_t *sec, const char *name, unsigned int idx,
  int *err)
{
  vt_config_t **opts;
  unsigned int i, j, nopts;

  assert (sec);
  assert (sec->type == VT_CONFIG_TYPE_FILE || sec->type == VT_CONFIG_TYPE_SEC);
  /* name can be NULL to get option based on index only */

  if (sec->type == VT_CONFIG_TYPE_FILE) {
    opts = sec->data.file.opts;
    nopts = sec->data.file.nopts;
  } else {
    opts = sec->data.sec.opts;
    nopts = sec->data.sec.nopts;
  }

  for (i = 0, j = 0; i < nopts; i++) {
    if (name) {
      if (strcmp (opts[i]->name, name) == 0)
        j++;
    } else if (! name) {
      j++;
    }

    if ((j > 0) && (j - 1) == idx)
      break;
  }

  if ((j - 1) == idx)
    return opts[i];
  vt_set_errno (err, EINVAL);
  return NULL;
}

vt_config_t *
vt_config_sec_getnopt_priv (vt_config_t *sec, vt_value_type_t type,
  const char *name, unsigned int idx, int *err)
{
  vt_config_t *opt;

  assert (sec);
  assert (sec->type == VT_CONFIG_TYPE_SEC || sec->type == VT_CONFIG_TYPE_FILE);
  assert (name);

  if (! (opt = vt_config_sec_getnopt (sec, name, 0, err)))
    return NULL;

  assert (opt->type = type);

  return opt;
}

unsigned int
vt_config_sec_getnopts (vt_config_t *sec)
{
  assert (sec);
  assert (sec->type == VT_CONFIG_TYPE_SEC || sec->type == VT_CONFIG_TYPE_FILE);

  return sec->data.sec.nopts;
}

bool
vt_config_sec_getnbool (vt_config_t *sec, const char *name, unsigned int idx,
  int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt_priv (sec, VT_CONFIG_TYPE_BOOL, name, idx, err);
  return opt ? vt_config_getnbool (opt, idx, err) : false;
}

long
vt_config_sec_getnint (vt_config_t *sec, const char *name, unsigned int idx,
  int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt_priv (sec, VT_CONFIG_TYPE_INT, name, 0, err);
  return opt ? vt_config_getnint (opt, idx, err) : 0;
}

double
vt_config_sec_getnfloat (vt_config_t *sec, const char *name, unsigned int idx,
  int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt_priv (sec, VT_CONFIG_TYPE_FLOAT, name, idx, err);
  return opt ? vt_config_getnfloat (opt, idx, err) : 0.0;
}

char *
vt_config_sec_getnstr (vt_config_t *sec, const char *name, unsigned int idx,
  int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt_priv (sec, VT_CONFIG_TYPE_STR, name, idx, err);
  return opt ? vt_config_getnstr (opt, idx, err) : NULL;
}

