/* system includes */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "vt_buf.h"
#include "vt_config.h"
#include "vt_lexer.h"

/* prototypes */
static int vt_config_defaults (vt_config_t *, vt_config_def_t *,
  int *);
static int vt_config_parse_sec (vt_config_t *, vt_config_def_t *, vt_lexer_t *,
  int *);
static vt_token_t vt_config_parse_opt (vt_config_t *, vt_config_def_t *,
  vt_lexer_t *, int *);
static int vt_config_parse (vt_config_t *, vt_config_def_t *, vt_lexer_t *,
  int *);
static int vt_config_validate (vt_config_t *,
                               vt_config_t *,
                               vt_config_def_t *,
                               int *);

/* generic option interface functions */
static int vt_config_getn_cmn (vt_config_t *, vt_value_t *, int, int, int *);
static int vt_config_setn_cmn (vt_config_t *, vt_value_t *, int, int, int *);
static int vt_config_unsetn_cmn (vt_config_t *, vt_value_t *, int, int, int *);

/* generic file/section interface functions */
static int vt_config_sec_getn_cmn (vt_config_t *, vt_config_t *, int, int,
  int *);
static int vt_config_sec_setn_cmn (vt_config_t *, vt_config_t *, int, int,
  int *);
static int vt_config_sec_unsetn_cmn (vt_config_t *, vt_config_t *, int, int,
  int *);
static int vt_config_sec_setnopt (vt_config_t *, const char *, int, int, int,
  int *);

#define vt_config_isopt(sec) \
  ((sec)->type == VT_CONFIG_TYPE_OPT)
#define vt_config_issec(sec) \
  ((sec)->type == VT_CONFIG_TYPE_FILE || (sec)->type == VT_CONFIG_TYPE_SEC)
#define vt_config_sec_opts(sec) \
  ((sec)->type == VT_CONFIG_TYPE_FILE \
    ? (sec)->data.file.opts : (sec)->data.sec.opts)
#define vt_config_sec_nopts(sec) \
  ((sec)->type == VT_CONFIG_TYPE_FILE \
    ? (sec)->data.file.nopts : (sec)->data.sec.nopts)

vt_config_t *
vt_config_create (const char *name, int flags, int *err)
{
  vt_config_t *opt;

  if (! (opt = calloc (1, sizeof (vt_config_t)))) {
    vt_set_errno (err, errno);
    vt_error ("calloc: %s", vt_strerror (errno));
    return NULL;
  }

  if (name)
    (void)strncpy (opt->name, name, VT_CONFIG_NAME_MAX - 1);
  opt->flags = flags;

  return opt;
}

vt_config_t *
vt_config_opt_create (const char *name, int flags, vt_value_type_t type,
  int *err)
{
  vt_config_t *opt;

  if (! (opt = vt_config_create (name, flags, err)))
    return NULL;

  opt->type = VT_CONFIG_TYPE_OPT;
  opt->data.opt.type = type;

  return opt;
}

vt_config_t *
vt_config_sec_create (const char *name, int flags, const char *title,
  int *err)
{
  vt_config_t *sec;

  if (! (sec = vt_config_create (name, flags, err)))
    return NULL;

  sec->type = VT_CONFIG_TYPE_SEC;
  if (title) {
    (void)strncpy (sec->data.sec.title, title, VT_CONFIG_TITLE_MAX);
    sec->flags |= VT_CONFIG_FLAG_TITLE;
  }

  return sec;
}

int
vt_config_destroy (vt_config_t *cfg, int *err)
{
  int i;
  unsigned int nopts;
  vt_config_t **opts;
  vt_value_t *val;

  if (cfg) {
    if (cfg->type == VT_CONFIG_TYPE_FILE) {
      opts = cfg->data.file.opts;
      nopts = cfg->data.file.nopts;
    } else if (cfg->type == VT_CONFIG_TYPE_SEC) {
      opts = cfg->data.sec.opts;
      nopts = cfg->data.sec.nopts;
    } else if (cfg->type == VT_CONFIG_TYPE_OPT) {
      for (i = 0; i < cfg->data.opt.nvals; i++) {
        if ((val = cfg->data.opt.vals[i])) {
          if (val->type == VT_VALUE_TYPE_STR && val->data.str)
            free (val->data.str);
          free (val);
        }
      }
      if (cfg->data.opt.vals)
        free (cfg->data.opt.vals);

      opts = NULL;
      nopts = 0;
    }

    if (nopts) {
      for (i = 0; i < nopts; i++) {
        if (opts[i] && vt_config_destroy (opts[i], err) < 0)
          return -1;
      }
      if (opts)
        free (opts);
    }

    free (cfg);
  }

  return 0;
}

static int
vt_config_parse_sec (vt_config_t *sec, vt_config_def_t *def, vt_lexer_t *lxr,
  int *err)
{
  int loop, title;
  vt_config_def_t *defs;
  vt_lexer_def_t lxr_def;
  vt_token_t token;

  assert (sec);
  assert (def);
  assert (lxr);

  if (def->flags & VT_CONFIG_FLAG_TITLE)
    title = 1;
  else
    title = 0;

  memset (&lxr_def, 0, sizeof (vt_lexer_def_t));
  lxr_def.space = VT_CHRS_SPACE;
  lxr_def.char_as_token = 1;
  if (def->data.sec.title) {
    lxr_def.str_first = def->data.sec.title;
    lxr_def.str = def->data.sec.title;
  } else {
    lxr_def.str_first = VT_CHRS_STR_FIRST;
    lxr_def.str = VT_CHRS_STR;
  }

  for (;;) {
    token = vt_lexer_get_next_token (lxr, &lxr_def, err);

    switch (token) {
      case VT_TOKEN_EOF:
        vt_set_errno (err, EINVAL);
        vt_error ("unexpected end of file");
      case VT_TOKEN_ERROR:
        goto error;
      case VT_TOKEN_COMMENT_SINGLE:
      case VT_TOKEN_COMMENT_MULTI:
        /* always ignore comments */
        break;
      case VT_TOKEN_LEFT_CURLY:
        /* verify title is set */
        if (title && strlen (sec->data.sec.title) == 0) {
          vt_set_errno (err, EINVAL);
          vt_error ("unexpected curly bracket on line %u, column %u",
            lxr->line, lxr->column);
          goto error;  
        }

        return vt_config_parse (sec, def->data.sec.opts, lxr, err);
        break; /* never reached */
      case VT_TOKEN_STR:
        /* verify section is supposed to have a title */
        if (! title || strlen (sec->data.sec.title) != 0) {
          vt_set_errno (err, EINVAL);
          vt_error ("unexpected title on line %u, column %u",
            lxr->line, lxr->column);
          goto error;
        }

        sec->flags |= VT_CONFIG_FLAG_TITLE;
        strncpy (sec->data.sec.title, lxr->value.data.str, VT_CONFIG_TITLE_MAX);
        break;
      default:
        vt_set_errno (err, EINVAL);
        if (token < VT_TOKEN_NONE)
          vt_error ("unexpected character %c on line %u, column %u",
            lxr->value.data.chr, lxr->line, lxr->column);
        else
          vt_error ("unexpected token %d on line %u, column %u",
            token, lxr->line, lxr->column);
        goto error;
    }
  }

  return 0;
error:
  return -1;
}

static vt_token_t
vt_config_parse_opt (vt_config_t *opt, vt_config_def_t *def, vt_lexer_t *lxr,
  int *err)
{
  char *spec;
  int ret;
  vt_lexer_def_t lxr_def;
  vt_token_t expect, token;

  assert (opt && vt_config_isopt (opt));
  assert (def && def->type == VT_CONFIG_TYPE_OPT);
  assert (lxr);

  opt->data.opt.type = def->data.opt.type;

  memset (&lxr_def, 0, sizeof (vt_lexer_def_t));
  lxr_def.space = VT_CHRS_SPACE;
  lxr_def.str_first = VT_CHRS_STR_FIRST;
  lxr_def.str = VT_CHRS_STR;
  lxr_def.char_as_token = 1;
  switch (def->data.opt.type) {
    case VT_VALUE_TYPE_BOOL:
      lxr_def.scan_bool = 1;
      break;
    case VT_VALUE_TYPE_FLOAT:
      lxr_def.int_as_float = 1;
    case VT_VALUE_TYPE_INT:
      lxr_def.scan_binary = 1;
      lxr_def.scan_octal = 1;
      lxr_def.scan_hex = 1;
      lxr_def.scan_hex_dollar = 1;
      lxr_def.numbers_as_int = 1;
      break;
    case VT_VALUE_TYPE_STR:
      if (def->data.opt.dquot)
        lxr_def.dquot = def->data.opt.dquot;
      else
        lxr_def.squot = VT_CHRS_DQUOT;
      if (def->data.opt.squot)
        lxr_def.squot = def->data.opt.squot;
      else
        lxr_def.squot = VT_CHRS_SQUOT;
      break;
    default:
      break;
  }

  expect = VT_TOKEN_EQUAL_SIGN;
  token = VT_TOKEN_ERROR;
  for (;;) {
    token = vt_lexer_get_next_token (lxr, &lxr_def, err);

    switch (token) {
      case VT_TOKEN_EOF:
        vt_set_errno (err, EINVAL);
        vt_error ("unexpected end of file");
      case VT_TOKEN_ERROR:
        goto error;
      case VT_TOKEN_EQUAL_SIGN:
        if (expect != VT_TOKEN_EQUAL_SIGN)
          goto unexpected;
        expect = VT_TOKEN_NONE;
        break;
      case VT_TOKEN_COMMA:
        if (expect != VT_TOKEN_COMMA)
          goto unexpected;
        expect = VT_TOKEN_NONE;
        break;
      case VT_TOKEN_BOOL:
      case VT_TOKEN_INT:
      case VT_TOKEN_FLOAT:
      case VT_TOKEN_STR:
        if (expect == VT_TOKEN_COMMA && token != VT_TOKEN_COMMA)
          goto done;
        if (def->data.opt.type != lxr->value.type)
          goto unexpected;

        if (lxr->value.type == VT_VALUE_TYPE_BOOL)
          ret = vt_config_setnbool (opt, lxr->value.data.bln, -1, VT_CONFIG_INST_APPEND, err);
        else if (lxr->value.type == VT_VALUE_TYPE_INT)
          ret = vt_config_setnint (opt, lxr->value.data.lng, -1, VT_CONFIG_INST_APPEND, err);
        else if (lxr->value.type == VT_VALUE_TYPE_FLOAT)
          ret = vt_config_setnfloat (opt, lxr->value.data.dbl, -1, VT_CONFIG_INST_APPEND, err);
        else if (lxr->value.type == VT_VALUE_TYPE_STR)
          ret = vt_config_setnstr (opt, lxr->value.data.str, -1, VT_CONFIG_INST_APPEND, err);

        if (! (def->flags & VT_CONFIG_FLAG_LIST))
          goto done;
        expect = VT_TOKEN_COMMA;
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
        goto error;
    }
  }

done:
  return token;
error:
  return VT_TOKEN_ERROR;
}

static int
vt_config_parse (vt_config_t *sec, vt_config_def_t *defs, vt_lexer_t *lxr,
  int *err)
{
  char *name, *spec;
  int flags, inst, loop, nopts, optno;
  int (*strcmp_func)(const char *, const char *);
  vt_config_t *opt, **opts;
  vt_config_def_t *def;
  vt_lexer_def_t lxr_def;
  vt_token_t token;

  assert (sec);
  assert (defs);
  assert (lxr);

  memset (&lxr_def, 0, sizeof (vt_lexer_def_t));
  lxr_def.space = VT_CHRS_SPACE;
  lxr_def.str_first = VT_CHRS_STR_FIRST;
  lxr_def.str = VT_CHRS_STR;
  lxr_def.char_as_token = 1;

  inst = VT_CONFIG_INST_APPEND | VT_CONFIG_INST_COMPARE;

  for (loop = 1; loop;) {
    token = vt_lexer_get_next_token (lxr, &lxr_def, err);
inspect:
    switch (token) {
      case VT_TOKEN_EOF:
        if (sec->type == VT_CONFIG_TYPE_SEC) {
          vt_set_errno (err, EINVAL);
          vt_error ("unexpected end of file");
          goto error;
        }
        loop = 0;
        break;
      case VT_TOKEN_ERROR:
        goto error;
      case VT_TOKEN_COMMENT_SINGLE:
      case VT_TOKEN_COMMENT_MULTI:
        /* always ignore comments */
        break;
      case VT_TOKEN_RIGHT_CURLY:
        if (sec->type == VT_CONFIG_TYPE_FILE)
          goto unexpected;
        loop = 0;
        break;
      case VT_TOKEN_STR:
        name = lxr->value.data.str;
        for (def = defs; def->type != VT_CONFIG_TYPE_NONE; def++) {
          if (def->flags & VT_CONFIG_FLAG_NOCASE)
            strcmp_func = &strcasecmp;
          else
            strcmp_func = &strcmp;
          if (def->name && strcmp_func (def->name, name) == 0)
            break;
        }

        switch (def->type) {
          case VT_CONFIG_TYPE_SEC:
            /* section allowed to exist if multi flag set */
            if (def->flags & VT_CONFIG_FLAG_MULTI)
              break;
            /* fall through */
          case VT_CONFIG_TYPE_OPT:
            opts = vt_config_sec_opts (sec);
            nopts = vt_config_sec_nopts (sec);
            for (optno = 0; optno < nopts; optno++) {
              opt = opts[optno];
              if (strcmp_func (def->name, opt->name) == 0) {
                vt_set_errno (err, EINVAL);
                vt_error ("earlier declaration of %s %s on line %u, column %u",
                  def->type == VT_CONFIG_TYPE_SEC ? "section" : "option",
                  opt->name, opt->line, opt->column);
                goto error;
              }
            }
            break;
          default:
            vt_set_errno (err, EINVAL);
            vt_error ("unknown option %s on line %u, column %u",
              lxr->value.data.str, lxr->line, lxr->column);
            goto error;
        }

        flags = def->flags | VT_CONFIG_FLAG_NODEFAULT;

        if (def->type == VT_CONFIG_TYPE_SEC) {
          optno = vt_config_sec_setnsec (sec, name, flags, -1, inst, err);
          if (optno < 0) {
            vt_error ("could not create section %s: %s", name, vt_strerror (*err));
            goto error;
          }

          opts = vt_config_sec_opts (sec);
          opt = opts[optno];
          opt->line = lxr->line;
          opt->column = lxr->column;
          if (vt_config_parse_sec (opt, def, lxr, err) < 0)
            goto error;
        } else {
          optno = vt_config_sec_setnopt (sec, name, flags, -1, inst, err);
          if (optno < 0) {
            vt_error ("could not create option %s: %s", name, vt_strerror (*err));
            goto error;
          }

          opts = vt_config_sec_opts (sec);
          opt = opts[optno];
          opt->line = lxr->line;
          opt->column = lxr->column;
          if ((token = vt_config_parse_opt (opt, def, lxr, err)) == VT_TOKEN_ERROR)
            goto error;
          if (def->flags & VT_CONFIG_FLAG_LIST)
            goto inspect;
        }
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
        goto error;
    }
  }

  return 0;
error:
  return -1;
}



vt_config_t *
vt_config_parse_str (vt_config_def_t *defs, const char *str, size_t len,
  int *err)
{
  vt_config_t *root;
  vt_lexer_t lxr;

  assert (defs);
  assert (str && len);

  vt_lexer_init (&lxr, str, len);

  if (! (root = vt_config_create (NULL, VT_CONFIG_FLAG_NONE, err)))
    goto error;

  root->type = VT_CONFIG_TYPE_FILE;

  /* parse buffer */
  if (vt_config_parse (root, defs, &lxr, err) != 0)
    goto error;
  /* provide defaults for missing options */
  if (vt_config_defaults (root, defs, err) != 0)
    goto error;
  /* validate options */
  if (vt_config_validate (root, root, defs, err) != 0)
    goto error;

  return (root);
error:
  if (root)
    (void)vt_config_destroy (root, NULL);
  return (NULL);
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

  if (! (cfg = vt_config_parse_str (def, str, len, err)))
    goto failure;

vt_buf_deinit (&buf);

  return cfg;
failure:
  // clean up
  return NULL;
#undef BUFLEN
}

int
vt_config_defaults (vt_config_t *sec, vt_config_def_t *defs, int *err)
{
  char *name;
  int flags, inst;
  int nopts, optno, pos;
  int (*strcmp_func)(const char *, const char *);
  vt_config_t *opt, **opts;
  vt_config_def_t *def;
  vt_value_type_t type;

  assert (sec && vt_config_issec (sec));
  assert (defs);

  inst = VT_CONFIG_INST_SET    |
         VT_CONFIG_INST_APPEND |
         VT_CONFIG_INST_RECYCLE;
  opt = NULL;
  opts = vt_config_sec_opts (sec);
  nopts = vt_config_sec_nopts (sec);

  for (def = defs; def->type != VT_CONFIG_TYPE_NONE; def++) {
    opt = NULL;
    if (def->flags & VT_CONFIG_FLAG_NOCASE)
      strcmp_func = &strcasecmp;
    else
      strcmp_func = &strcmp;

    for (optno = 0; ! opt && optno < nopts; optno++) {
      if (strcmp_func (opts[optno]->name, def->name) == 0)
        opt = opts[optno];
    }

    if (opt) {
      if (vt_config_issec (opt) && vt_config_defaults (opt, def->data.sec.opts, err) < 0)
        goto error;
    } else {
      if (vt_config_issec (def)) {
        name = def->name;
        flags = def->flags & ~( VT_CONFIG_FLAG_LIST  |
                                VT_CONFIG_FLAG_NODEFAULT );
        if (! (opt = vt_config_sec_create (name, flags, NULL, err)))
          goto error;
      } else {
        name = def->name;
        flags = def->flags & ~( VT_CONFIG_FLAG_LIST  |
                                VT_CONFIG_FLAG_TITLE |
                                VT_CONFIG_FLAG_NODEFAULT );
        type = def->data.opt.type;
        if (! (opt = vt_config_opt_create (name, flags, type, err)))
          goto error;
        if (def->flags & VT_CONFIG_FLAG_NODEFAULT)
          type = VT_VALUE_TYPE_NONE;

        switch (type) {
          case VT_VALUE_TYPE_BOOL:
            pos = vt_config_setbool (opt, def->data.opt.val.data.bln, err);
            break;
          case VT_VALUE_TYPE_INT:
            pos = vt_config_setint (opt, def->data.opt.val.data.lng, err);
            break;
          case VT_VALUE_TYPE_FLOAT:
            pos = vt_config_setfloat (opt, def->data.opt.val.data.dbl, err);
            break;
          case VT_VALUE_TYPE_STR:
            pos = vt_config_setstr (opt, def->data.opt.val.data.str, err);
            break;
          default:
            pos = 0;
            break;
        }

        if (pos < 0)
          goto error;
      }

      if (vt_config_issec (opt) && vt_config_defaults (opt, def->data.sec.opts, err) < 0)
        goto error;
      if (vt_config_sec_setn_cmn (sec, opt, -1, inst, err) < 0)
      goto error;
    }
  }

  return 0;
error:
  if (opt)
    (void)vt_config_destroy (opt, NULL);
  return -1;
}

static int
vt_config_validate (vt_config_t *root,
                    vt_config_t *sec,
                    vt_config_def_t *defs,
                    int *err)
{
  int cur, next, nopts, optno;
  int(*strcmp_func)(const char *, const char *);
  vt_config_t **opts;
  vt_config_def_t *def;

  assert (root && vt_config_issec (root));
  assert (sec && vt_config_issec (sec));

  nopts = vt_config_sec_nopts (sec);
  opts = vt_config_sec_opts (sec);

  for (def = defs; def->type != VT_CONFIG_TYPE_NONE; def++) {
    if (def->flags & VT_CONFIG_FLAG_NOCASE)
      strcmp_func = &strcasecmp;
    else
      strcmp_func = &strcmp;

    /* verify no duplicate (by title) sections exist */
    if (def->flags & VT_CONFIG_FLAG_TITLE &&
        def->flags & VT_CONFIG_FLAG_NODUPES)
    {
      for (optno = 0; optno < nopts; ) {
        cur = optno;

        if (vt_config_issec (opts[cur]) &&
            strcmp_func (opts[cur]->name, def->name) == 0)
        {
          for (next = cur + 1; next < nopts; next++) {
            if (vt_config_issec (opts[next]) &&
                strcmp_func (opts[next]->name, def->name) == 0)
            {
              if (cur == optno)
                optno = next;
              if (strcmp_func (opts[cur]->data.sec.title, opts[next]->data.sec.title) == 0) {
                vt_set_errno (err, EINVAL);
                vt_error ("earlier declaration of section %s with title %s on line %u, column %u",
                  opts[cur]->name, opts[cur]->data.sec.title,
                  opts[cur]->line, opts[cur]->column);
                return (-1);
              }
            }
          }

          if (cur == optno)
            optno = nopts;
        }
        else
        {
          optno++;
        }
      }
    }

    if (def->validate_func) {
      for (optno = 0; optno < nopts; optno++) {
        if (opts[optno] && strcmp_func (opts[optno]->name, def->name) == 0) {
          if (def->validate_func (root, opts[optno], err) != 0)
            return (-1);
        }
      }
    }
  }

  return (0);
}

static int
vt_config_getn_cmn (vt_config_t *opt, vt_value_t *val, int idx, int inst,
  int *err)
{
  int i, n, pos;

  assert (opt && vt_config_isopt (opt));
  if (inst & VT_CONFIG_INST_COMPARE)
    assert (val && opt->data.opt.type == val->type);

  /* we need to count the number of values with the same value if the
     compare flag was specified */
  if (inst & VT_CONFIG_INST_COMPARE) {
    n = 0;
    for (i = 0; i < opt->data.opt.nvals; i++) {
      if (vt_value_cmp (opt->data.opt.vals[i], val, NULL) == 0)
        n++;
    }
  } else {
    n = opt->data.opt.nvals;
  }

  if (n) {
    if (idx < 0) {
      idx = (idx * -1) - 1;
      if (idx > n) {
        n = 1;
      } else {
        n -= idx;
      }
    } else if (idx < n) {
      n = idx + 1;
    }
  } else {
    if (inst & VT_CONFIG_INST_APPEND)
      return opt->data.opt.nvals;
    if (inst & (VT_CONFIG_INST_PREPEND | VT_CONFIG_INST_SET))
      return 0;
    vt_set_errno (err, EINVAL);
    return -1;
  }

  for (i = 0, pos = 0; pos < n && i < opt->data.opt.nvals; i++) {
    if (inst & VT_CONFIG_INST_COMPARE) {
      if (vt_value_cmp (opt->data.opt.vals[i], val, NULL) == 0)
        pos++;
    } else {
      pos++;
    }
  }

  if (! (inst & VT_CONFIG_INST_APPEND))
    i--;

  return i;
}

static int
vt_config_setn_cmn (vt_config_t *opt, vt_value_t *val, int idx, int inst,
  int *err)
{
  int new_nvals, nvals, pos;
  size_t size;
  vt_value_t **new_vals, **vals;

  vals = NULL;
  new_vals = NULL;
  nvals = 0;
  new_nvals = 0;

  assert (opt && vt_config_isopt (opt));
  if (inst & VT_CONFIG_INST_COMPARE)
    assert (val && opt->data.opt.type == val->type);

  if ((pos = vt_config_getn_cmn (opt, val, idx, inst, err)) < 0)
    return -1;

  if (val) {
    if (! (inst & VT_CONFIG_INST_RECYCLE) && ! (val = vt_value_dup (val))) {
      vt_set_errno (err, errno);
      return -1;
    }
  } else if (! (val = calloc (1, sizeof (vt_value_t)))) {
    vt_set_errno (err, errno);
    return -1;
  }

  vals = opt->data.opt.vals;
  nvals = opt->data.opt.nvals;
  if (nvals > 0) {
    if (inst & (VT_CONFIG_INST_PREPEND | VT_CONFIG_INST_APPEND))
      new_nvals = nvals + 1;
    else
      new_nvals = nvals;
  } else {
    new_nvals = 1;
  }

  if (new_nvals > nvals) {
    size = new_nvals * sizeof (vt_value_t *);
    if (! (new_vals = realloc (vals, size))) {
      vt_set_errno (err, errno);
      goto error;
    }
    memset (new_vals + nvals, 0, 1 * sizeof (vt_value_t *));
    vals = new_vals;
    nvals = new_nvals;
  }

  if (inst & (VT_CONFIG_INST_PREPEND | VT_CONFIG_INST_APPEND)) {
    if (pos < (nvals - 1)) {
      size = (nvals - pos) * sizeof (vt_value_t *);
      memmove (vals + pos, vals + (pos + 1), size);
    }
  } else {
    (void)vt_value_destroy (vals[pos]);
  }

  vals[pos] = val;

  opt->data.opt.vals = vals;
  opt->data.opt.nvals = nvals;

  return pos;
error:
  (void)vt_value_destroy (val);
  return -1;
}

static int
vt_config_unsetn_cmn (vt_config_t *opt, vt_value_t *val, int idx, int inst,
  int *err)
{
  int nvals, pos;
  size_t size;
  vt_value_t **vals;

  assert (opt && vt_config_isopt (opt));
  if (inst & VT_CONFIG_INST_COMPARE)
    assert (val && opt->data.opt.type == val->type);

  if ((pos = vt_config_getn_cmn (opt, val, idx, inst, err)) < 0)
    return pos;

  val = opt->data.opt.vals[pos];
  vals = opt->data.opt.vals;
  nvals = opt->data.opt.nvals;

  vt_value_destroy (val);

  if (nvals > 1) {
    size = (nvals - pos) - 1;
    if (size)
      memmove (vals + (pos + 1), vals + pos, size);
    memset (vals + (nvals - 1), 0, sizeof (vt_value_t *));
    opt->data.opt.nvals--;
  } else {
    free (vals);
    opt->data.opt.nvals = 0;
  }

  return pos;
}

char *
vt_config_getname (vt_config_t *opt)
{
  assert (opt && (vt_config_isopt (opt) || vt_config_issec (opt)));

  return opt->name;
}

int
vt_config_getnvals (vt_config_t *opt)
{
  assert (opt && vt_config_isopt (opt));

  return opt->data.opt.nvals;
}

bool
vt_config_getnbool (vt_config_t *opt, int idx, int *err)
{
  int pos;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_BOOL);

  if ((pos = vt_config_getn_cmn (opt, NULL, idx, 0, err)) < 0)
    return false;
  return opt->data.opt.vals[pos]->data.bln;
}

int
vt_config_getbooln (vt_config_t *opt, bool bln, int idx, int inst, int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_BOOL);

  val.type = VT_VALUE_TYPE_BOOL;
  val.data.bln = bln;

  /* make sure set, prepend, and append flags are off */
  inst &= ~(VT_CONFIG_INST_SET     |
            VT_CONFIG_INST_PREPEND |
            VT_CONFIG_INST_APPEND);

  return vt_config_getn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_setnbool (vt_config_t *opt, bool bln, int idx, int inst, int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_BOOL);

  val.type = VT_VALUE_TYPE_BOOL;
  val.data.bln = bln;

  /* make sure set flag is on */
  inst |= VT_CONFIG_INST_SET;
  /* make sure compare flag is off */
  inst &= ~VT_CONFIG_INST_COMPARE;

  return vt_config_setn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_unsetnbool (vt_config_t *opt, int idx, int *err)
{
  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_BOOL);

  return vt_config_unsetn_cmn (opt, NULL, idx, 0, err);
}

long
vt_config_getnint (vt_config_t *opt, int idx, int *err)
{
  int pos;
  vt_value_t *val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_INT);

  if ((pos = vt_config_getn_cmn (opt, NULL, idx, 0, err)) < 0)
    return 0;
  return opt->data.opt.vals[pos]->data.lng;
}

int
vt_config_getintn (vt_config_t *opt, long lng, int idx, int inst, int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_INT);

  val.type = VT_VALUE_TYPE_INT;
  val.data.lng = lng;

  /* make sure set, prepend and append flags are off */
  inst &= ~(VT_CONFIG_INST_SET     |
            VT_CONFIG_INST_PREPEND |
            VT_CONFIG_INST_APPEND);

  return vt_config_getn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_setnint (vt_config_t *opt, long lng, int idx, int inst, int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_INT);

  val.type = VT_VALUE_TYPE_INT;
  val.data.lng = lng;

  /* make sure set flag is on */
  inst |= VT_CONFIG_INST_SET;
  /* make sure compare flag is off */
  inst &= ~VT_CONFIG_INST_COMPARE;

  return vt_config_setn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_unsetnint (vt_config_t *opt, int idx, int *err)
{
  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_INT);

  return vt_config_unsetn_cmn (opt, NULL, idx, 0, err);
}

double
vt_config_getnfloat (vt_config_t *opt, int idx, int *err)
{
  int pos;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_FLOAT);

  if ((pos = vt_config_getn_cmn (opt, NULL, idx, 0, err)) < 0)
    return 0.0;
  return opt->data.opt.vals[pos]->data.dbl;
}

int
vt_config_getfloatn (vt_config_t *opt, double dbl, int idx, int inst, int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_FLOAT);

  val.type = VT_VALUE_TYPE_FLOAT;
  val.data.dbl = dbl;

  /* make sure set, prepend and append flags are off */
  inst &= ~(VT_CONFIG_INST_SET     |
            VT_CONFIG_INST_PREPEND |
            VT_CONFIG_INST_APPEND);

  return vt_config_getn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_setnfloat (vt_config_t *opt, double dbl, int idx, int inst, int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_FLOAT);

  val.type = VT_VALUE_TYPE_FLOAT;
  val.data.dbl = dbl;

  /* make sure set flag is on */
  inst |= VT_CONFIG_INST_SET;
  /* make sure compare flag is off */
  inst &= ~VT_CONFIG_INST_COMPARE;

  return vt_config_setn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_unsetnfloat (vt_config_t *opt, int idx, int *err)
{
  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_FLOAT);

  return vt_config_unsetn_cmn (opt, NULL, idx, 0, err);
}

char *
vt_config_getnstr (vt_config_t *opt, int idx, int *err)
{
  int pos;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_STR);

  if ((pos = vt_config_getn_cmn (opt, NULL, idx, 0, err)) < 0)
    return NULL;
  return opt->data.opt.vals[pos]->data.str;
}

char *
vt_config_getnstr_dup (vt_config_t *opt, int idx, int *err)
{
  char *str;

  if ((str = vt_config_getnstr (opt, idx, err))) {
    if (! (str = strdup (str)))
      vt_set_errno (err, errno);
  }

  return str;
}

int
vt_config_getstrn (vt_config_t *opt, const char *str, int idx, int inst,
  int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_STR);

  val.type = VT_VALUE_TYPE_STR;
  val.data.str = (char *)str;

  /* make sure set, prepend and append flags are off */
  inst &= ~(VT_CONFIG_INST_SET     |
            VT_CONFIG_INST_PREPEND |
            VT_CONFIG_INST_APPEND);

  return vt_config_getn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_setnstr (vt_config_t *opt, const char *str, int idx, int inst,
  int *err)
{
  vt_value_t val;

  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_STR);

  val.type = VT_VALUE_TYPE_STR;
  val.data.str = (char *)str;

  /* make sure set flag is on */
  inst |= VT_CONFIG_INST_SET;
  /* make sure compare flag is off */
  inst &= ~VT_CONFIG_INST_COMPARE;

  return vt_config_setn_cmn (opt, &val, idx, inst, err);
}

int
vt_config_unsetnstr (vt_config_t *opt, int idx, int *err)
{
  assert (opt && vt_config_isopt (opt));
  assert (opt->data.opt.type == VT_VALUE_TYPE_STR);

  return vt_config_unsetn_cmn (opt, NULL, idx, 0, err);
}

static int
vt_config_sec_getn_cmn (vt_config_t *sec, vt_config_t *opt, int idx, int inst,
  int *err)
{
  int cnt, i, n, nopts, pos, set;
  size_t sz;
  vt_config_t **opts;
  vt_config_type_t type = VT_CONFIG_TYPE_NONE;

  assert (sec);
  assert (vt_config_issec (sec));

  sz = strlen (opt->name);
  opts = vt_config_sec_opts (sec);
  nopts = vt_config_sec_nopts (sec);

  n = 0;
  for (i = 0; i < nopts; i++) {
    if (strcasecmp (opts[i]->name, opt->name) == 0) {
      if (type == VT_CONFIG_TYPE_NONE)
        type = opts[i]->type;
      /* types must match */
      if (type != opts[i]->type) {
        vt_set_errno (err, EINVAL);
        vt_error ("different types for option %s", opt->name);
        return -1;
      }

      if (opt->type == VT_CONFIG_TYPE_NONE) {
        n++;
      } else if (opt->type != opts[i]->type) {
        vt_set_errno (err, EINVAL);
        vt_error ("option %s registered as different type", opt->name);
        return -1;
      } else {
        n++;
      }
    } else if (inst & VT_CONFIG_INST_COMPARE) {
      if (sz)
        continue;
      if (opt->type == VT_CONFIG_TYPE_NONE || opt->type == opts[i]->type)
        n++;
    } else {
      n++;
    }
  }

  if (n) {
    set = inst & (VT_CONFIG_INST_APPEND  |
                  VT_CONFIG_INST_PREPEND |
                  VT_CONFIG_INST_SET);

    /* option name must be unique */
    if (vt_config_isopt (opt) && inst & VT_CONFIG_INST_COMPARE && set) {
      vt_set_errno (err, EINVAL);
      vt_error ("%s: option %s already exists", __func__, opt->name);
      return -1;
    }

    if (idx < 0) {
      idx = (idx * -1) - 1;
      if (idx > n)
        n = 1;
      else
        n -= idx;
    } else if (idx < n) {
      n = idx + 1;
    }
  } else {
    if (inst & VT_CONFIG_INST_APPEND)
      return nopts;
    if (inst & (VT_CONFIG_INST_PREPEND | VT_CONFIG_INST_SET))
      return 0;
    vt_set_errno (err, EINVAL);
    return -1;
  }

  pos = 0;
  for (i = 0; pos < n && i < nopts; i++) {
    if (inst & VT_CONFIG_INST_COMPARE) {
      cnt = 0;
      if (sz && strcasecmp (opts[i]->name, opt->name) == 0)
        cnt++;
      if (opt->type == VT_CONFIG_TYPE_NONE || opt->type == opts[i]->type)
        cnt++;
      if (cnt == 2)
        pos++;
    } else {
      pos++;
    }
  }

  if (! (inst & VT_CONFIG_INST_APPEND))
    i--;
  return i;
}

static int
vt_config_sec_setn_cmn (vt_config_t *sec, vt_config_t *opt, int idx, int inst,
  int *err)
{
  int new_nopts, nopts, pos;
  size_t nbytes;
  vt_config_t *new_opt, **new_opts, **opts;

  assert (sec && vt_config_issec (sec));
  assert (opt);

  /* user must provide *_PREPEND or *_APPEND flag himself, *_SET is turned on
     automatically */
  inst |= VT_CONFIG_INST_SET;

  if ((pos = vt_config_sec_getn_cmn (sec, opt, idx, inst, err)) < 0)
    return pos;

  opts = vt_config_sec_opts (sec);
  nopts = vt_config_sec_nopts (sec);

  if (inst & VT_CONFIG_INST_RECYCLE) {
    new_opt = opt;
  } else {
    if (! (new_opt = calloc (1, sizeof (vt_config_t)))) {
      vt_set_errno (err, errno);
      goto error;
    }
    /* clone option */
    (void)strcpy (new_opt->name, opt->name);
    new_opt->flags = opt->flags;
    new_opt->type = opt->type;
    if (vt_config_isopt (opt))
      new_opt->data.opt.type = opt->data.opt.type;
  }

  if (nopts > 0) {
    if (inst & (VT_CONFIG_INST_PREPEND | VT_CONFIG_INST_APPEND))
      new_nopts = nopts + 1;
    else
      new_nopts = nopts;
  } else {
    new_nopts = 1;
  }

  if (new_nopts > nopts) {
    nbytes = new_nopts * sizeof (vt_config_t *);
    if (! (new_opts = realloc (opts, nbytes))) {
      vt_set_errno (err, errno);
      goto error;
    }
    nbytes = (nopts - pos) * sizeof (vt_config_t *);
    if (nbytes)
      memmove (new_opts + pos, new_opts + (pos + 1), nbytes);
  } else {
    int i;
    switch (opts[pos]->type) {
      case VT_CONFIG_TYPE_FILE:
        for (i = 0; i < opts[pos]->data.file.nopts; i++) {
          vt_config_destroy (opts[pos]->data.file.opts[i], NULL);
        }
        free (opts[pos]->data.file.opts);
        break;
      case VT_CONFIG_TYPE_SEC:
        for (i = 0; i < opts[pos]->data.sec.nopts; i++) {
          vt_config_destroy (opts[pos]->data.sec.opts[i], NULL);
        }
        free (opts[pos]->data.sec.opts);
        break;
      case VT_CONFIG_TYPE_OPT:
        // FIXME:
        //if (opt->data.opt.type == VT_VALUE_TYPE_STR)
        //  free (opt->data.opt.vals.str);
        break;
    }
    free (opts[pos]);
    new_opts = opts;
  }

  new_opts[pos] = new_opt;

  if (sec->type == VT_CONFIG_TYPE_FILE) {
    sec->data.file.opts = new_opts;
    sec->data.file.nopts = new_nopts;
  } else {
    sec->data.sec.opts = new_opts;
    sec->data.sec.nopts = new_nopts;
  }

  return pos;
error:
  if (new_opt && new_opt != opt)
    vt_config_destroy (new_opt, NULL);
  return -1;
}

int
vt_config_sec_unsetn_cmn (vt_config_t *sec, vt_config_t *opt, int idx, int inst,
  int *err)
{
  int nopts, pos;
  size_t nbytes;
  vt_config_t **opts;

  assert (sec && vt_config_issec (sec));
  if (inst & VT_CONFIG_INST_COMPARE)
    assert (opt);

  /* make sure set, prepend, and append flags are off */
  inst &= ~(VT_CONFIG_INST_SET     |
            VT_CONFIG_INST_PREPEND |
            VT_CONFIG_INST_APPEND);
  if ((pos = vt_config_sec_getn_cmn (sec, opt, idx, inst, err)) < 0)
    return -1;

  opts = vt_config_sec_opts (sec);
  nopts = vt_config_sec_nopts (sec);

  if (vt_config_destroy (opts[pos], err) < 0)
    return -1;

  if (pos < --nopts) {
    nbytes = (nopts - pos) * sizeof (vt_config_t *);
    memmove (opts + (pos + 1), opts + pos, nbytes);
  }
  memset (opts + nopts, 0, sizeof (vt_config_t *));

  if (sec->type == VT_CONFIG_TYPE_FILE) {
    sec->data.file.opts = opts;
    sec->data.file.nopts = nopts;
  } else {
    sec->data.sec.opts = opts;
    sec->data.sec.nopts = nopts;
  }

  return 0;
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

bool
vt_config_sec_getnbool (vt_config_t *sec, const char *name, int idx, int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_getnbool (opt, idx, err);
  return false;
}

int
vt_config_sec_setnbool (vt_config_t *sec, const char *name, bool bln,
  int idx, int inst, int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_setnbool (opt, bln, idx, inst, err);
  return -1;
}

long
vt_config_sec_getnint (vt_config_t *sec, const char *name, int idx, int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_getnint (opt, idx, err);
  return 0;
}

int
vt_config_sec_setnint (vt_config_t *sec, const char *name, long lng,
  int idx, int inst, int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_setnint (opt, lng, idx, inst, err);
  return -1;
}

double
vt_config_sec_getnfloat (vt_config_t *sec, const char *name, int idx,
  int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_getnfloat (opt, idx, err);
  return 0.0;
}

int
vt_config_sec_setnfloat (vt_config_t *sec, const char *name, double dbl,
  int idx, int inst, int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_setnfloat (opt, dbl, idx, inst, err);
  return -1;
}

char *
vt_config_sec_getnstr (vt_config_t *sec, const char *name, int idx,
  int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_getnstr (opt, idx, err);
  return NULL;
}

char *
vt_config_sec_getnstr_dup (vt_config_t *sec, const char *name, int idx,
  int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_getnstr_dup (opt, idx, err);
  return NULL;
}

int
vt_config_sec_setnstr (vt_config_t *sec, const char *name, const char *str,
  int idx, int inst, int *err)
{
  vt_config_t *opt;

  opt = vt_config_sec_getnopt (sec, name, 0, VT_CONFIG_INST_COMPARE, err);
  if (opt)
    return vt_config_setnstr (opt, str, idx, inst, err);
  return -1;
}

int
vt_config_sec_getnopts (vt_config_t *sec)
{
  assert (sec && vt_config_issec (sec));

  return vt_config_sec_nopts (sec);
}

int
vt_config_sec_getnsecs (vt_config_t *sec, const char *name, int inst, int *err)
{
  int (*strcmp_func)(const char *, const char *);
  int cnt, cmp, nopts, optno;
  vt_config_t **opts;

  cmp = inst & VT_CONFIG_INST_COMPARE;

  assert (sec && vt_config_issec (sec));
  if (cmp)
    assert (name);

  cnt = 0;
  opts = vt_config_sec_opts (sec);
  nopts = vt_config_sec_nopts (sec);

  for (optno = 0; optno < nopts; optno++) {
    if (opts[optno]->type == VT_CONFIG_TYPE_SEC) {
      if (opts[optno]->flags & VT_CONFIG_FLAG_NOCASE)
        strcmp_func = &strcasecmp;
      else
        strcmp_func = &strcmp;

      if (cmp) {
        if (strcmp_func (opts[optno]->name, name) == 0)
          cnt++;
      } else {
        cnt++;
      }
    }
  }

  return cnt;
}

vt_config_t *
vt_config_sec_getnopt (vt_config_t *sec, const char *name, int idx, int inst,
  int *err)
{
  int pos;
  vt_config_t opt, **opts;

  assert (sec && vt_config_issec (sec));

  memset (&opt, 0, sizeof (vt_config_t));
  opt.type = VT_CONFIG_TYPE_OPT;
  if (inst & VT_CONFIG_INST_COMPARE && name)
    (void)strncpy (opt.name, name, VT_CONFIG_NAME_MAX);

  inst |= VT_CONFIG_INST_COMPARE;
  if ((pos = vt_config_sec_getn_cmn (sec, &opt, idx, inst, err)) < 0)
    return NULL;

  opts = vt_config_sec_opts (sec);
  return opts[pos];
}

static int
vt_config_sec_setnopt (vt_config_t *sec, const char *name, int flags,
  int idx, int inst, int *err)
{
  int pos;
  vt_config_t *opt;

  if (! (opt = vt_config_create (name, flags, err)))
    goto error;
  opt->type = VT_CONFIG_TYPE_OPT;
  inst |= VT_CONFIG_INST_RECYCLE;
  if ((pos = vt_config_sec_setn_cmn (sec, opt, idx, inst, err)) < 0)
    goto error;

  return pos;
error:
  if (opt)
    vt_config_destroy (opt, NULL);
  return -1;
}

int
vt_config_sec_setnopt_bool (vt_config_t *sec, const char *name, int flags,
  bool bln, int idx, int inst, int *err)
{
  int pos;
  vt_config_t *opt;

  if (! (opt = vt_config_opt_create (name, flags, VT_VALUE_TYPE_BOOL, err)))
    goto error;
  if (vt_config_setnbool (opt, bln, 0, 0, err) < 0)
    goto error;
  inst |= VT_CONFIG_INST_RECYCLE;
  if ((pos = vt_config_sec_setn_cmn (sec, opt, idx, inst, err)) < 0)
    goto error;

  return pos;
error:
  if (opt)
    vt_config_destroy (opt, NULL);
  return -1;
}

int
vt_config_sec_setnopt_int (vt_config_t *sec, const char *name, int flags,
  long lng, int idx, int inst, int *err)
{
  int pos;
  vt_config_t *opt;

  if (! (opt = vt_config_opt_create (name, flags, VT_VALUE_TYPE_INT, err)))
    goto error;
  if (vt_config_setnint (opt, lng, 0, 0, err) < 0)
    goto error;
  inst |= VT_CONFIG_INST_RECYCLE;
  if ((pos = vt_config_sec_setn_cmn (sec, opt, idx, inst, err)) < 0)
    goto error;

  return pos;
error:
  if (opt)
    vt_config_destroy (opt, NULL);
  return -1;
}

int
vt_config_sec_setnopt_float (vt_config_t *sec, const char *name, int flags,
  double dbl, int idx, int inst, int *err)
{
  int pos;
  vt_config_t *opt;

  if (! (opt = vt_config_opt_create (name, flags, VT_VALUE_TYPE_FLOAT, err)))
    goto error;
  if (vt_config_setnfloat (opt, dbl, 0, 0, err) < 0)
    goto error;

  inst |= VT_CONFIG_INST_RECYCLE;
  if ((pos = vt_config_sec_setn_cmn (sec, opt, idx, inst, err)) < 0)
    goto error;

  return pos;
error:
  if (opt)
    vt_config_destroy (opt, NULL);
  return -1;
}

int
vt_config_sec_setnopt_str (vt_config_t *sec, const char *name, int flags,
  const char *str, int idx, int inst, int *err)
{
  int pos;
  vt_config_t *opt;

  if (! (opt = vt_config_opt_create (name, flags, VT_VALUE_TYPE_STR, err)))
    goto error;
  if (vt_config_setnstr (opt, str, 0, 0, err) < 0)
    goto error;
  inst |= VT_CONFIG_INST_RECYCLE;
  if ((pos = vt_config_sec_setn_cmn (sec, opt, idx, inst, err)) < 0)
    goto error;

  return pos;
error:
  if (opt)
    vt_config_destroy (opt, NULL);
  return -1;
}

int
vt_config_sec_unsetnopt (vt_config_t *sec, const char *name, int idx, int inst,
  int *err)
{
  vt_config_t opt;

  assert (sec && vt_config_issec (sec));

  memset (&opt, 0, sizeof (vt_config_t));
  opt.type = VT_CONFIG_TYPE_OPT;
  if (inst & VT_CONFIG_INST_COMPARE && name)
    (void)strncpy (opt.name, name, VT_CONFIG_NAME_MAX);

  inst |= VT_CONFIG_INST_COMPARE;
  return vt_config_sec_unsetn_cmn (sec, &opt, idx, inst, err);
}

vt_config_t *
vt_config_sec_getnsec (vt_config_t *sec, const char *name, int idx, int inst,
  int *err)
{
  int pos;
  vt_config_t opt, **opts;

  assert (sec && vt_config_issec (sec));

  memset (&opt, 0, sizeof (vt_config_t));
  opt.type = VT_CONFIG_TYPE_SEC;
  if (inst & VT_CONFIG_INST_COMPARE && name)
    (void)strncpy (opt.name, name, VT_CONFIG_NAME_MAX);

  inst = VT_CONFIG_INST_COMPARE;
  if ((pos = vt_config_sec_getn_cmn (sec, &opt, idx, inst, err)) < 0)
    return NULL;

  opts = vt_config_sec_opts (sec);
  return opts[pos];
}

int
vt_config_sec_setnsec (vt_config_t *sec, const char *name, int flags, int idx,
  int inst, int *err)
{
  int pos;
  vt_config_t *opt;

  assert (sec && vt_config_issec (sec));
  assert (name);

  if (! (opt = vt_config_sec_create (name, flags, NULL, err)))
    goto error;
  inst |= VT_CONFIG_INST_RECYCLE;
  if ((pos = vt_config_sec_setn_cmn (sec, opt, idx, inst, err)) < 0)
    goto error;

  return pos;
error:
  if (opt)
    vt_config_destroy (opt, NULL);
  return -1;
}

int
vt_config_sec_unsetnsec (vt_config_t *sec, const char *name, int flags, int idx,
  int inst, int *err)
{
  vt_config_t opt;

  assert (sec && vt_config_issec (sec));

  memset (&opt, 0, sizeof (vt_config_t));
  opt.type = VT_CONFIG_TYPE_SEC;
  if (inst & VT_CONFIG_INST_COMPARE && name)
    (void)strncpy (opt.name, name, VT_CONFIG_NAME_MAX);

  inst |= VT_CONFIG_INST_COMPARE;
  return vt_config_sec_unsetn_cmn (sec, &opt, idx, inst, err);
}

