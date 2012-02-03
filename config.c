/* system includes */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "config.h"
#include "lexer.h"

typedef enum _vt_config_state vt_config_state_t;

enum _vt_config_state {
  VT_CONFIG_STATE_NONE,
  VT_CONFIG_STATE_SECTION,
  VT_CONFIG_STATE_OPTION,
  // ... FIXME: add more as needed
};

/* prototypes */

int
vt_config_parse_sec (vt_config_t *sec,
                     vt_config_def_t *def,
                     vt_lexer_t *lxr,
                     vt_error_t *err)
{
  int loop, title;
  vt_config_def_t *defs;
  vt_lexer_t *lxr;
  vt_lexer_scope_def_t *lxr_def;
  vt_error_t *err;

  assert (sec);
  assert (def);
  assert (lxr);

  // well the section might have weird characters in it's title!

  for (loop = 1; loop;) {
    tok = vt_lexer_get_token (lxr, err);

    switch (tok) {
      case VT_LEXER_TOKEN_EOF:
        // FIXME: IMPLEMENT
        break;
      case VT_LEXER_TOKEN_ERROR:
        // FIXME: IMPLEMENT
        break;
      case VT_LEXER_TOKEN_COMMENT_SINGLE:
      case VT_LEXER_TOKEN_COMMENT_MULTI:
        /* always ignore comments */
        break;
      case VT_LEXER_TOKEN_LEFT_CURLY:
        val = vt_lexer_get_value (lxr);

        /* verify we have a title if configured */
        if (def->flags & VT_CONFIG_FLAG_TITLE && ! strlen (sec->data.sec.title)) {
          vt_set_error (err, VT_ERR_...);
          vt_error ("unexpected curly bracket on line %u, column %u",
            val->lineno, val->colno);
          goto failure;
        }

        return vt_config_parse (sec, def->data.sec.opts, lxr, err);

      case VT_LEXER_TOKEN_IDENT:
        // ok... wel we musn't have a title yet...
        // and we must demand a title!
        break;
      default:
        /* all other tokens are illegal by default */
    }
  }

  return 0;
failure:
  /* cleanup */
  return -1;

  // FIXME: IMPLEMENT
        //if (state == VT_CONFIG_STATE_TITLE) {
        //
        //} else if (state == VT_CONFIG
        //  if (next_def->flags & VT_CONFIG_FLAG_TITLE) {
        //    vt_set_error (err, VT_ERR_UNEXP_TITLE);
        //    vt_error ("unexpected title %s on line %u, column %u",
        //      value->subopt.string, value->lineno, value->colno);
        //    goto failure;
        //  }
        //
        //} else {
        //  // its an identifier... perhaps even an option?!?!
        //}
}

int
vt_config_parse_opt (vt_config_t *cfg,
                     vt_config_def_t *def,
                     vt_lexer_t *lxr,
                     vt_error_t *err)
{
  // FIXME: IMPLEMENT
  int loop;
  //vt_config_t *
}

int
vt_config_parse (vt_config_t *sec,
                 vt_config_def_t *defs,
                 vt_lexer_t *lxr,
                 vt_error_t *err)
{
  int loop;
  vt_config_t *opt;
  vt_config_def_t *def;
  vt_lexer_value_t *val;
  vt_lexer_token_t *tok;

  assert (sec);
  assert (defs);
  assert (lxr);

  for (loop = 1; loop;) {
    tok = vt_lexer_get_token (lxr, def, err);

    switch (tok) {
      case VT_LEXER_TOKEN_EOF:
        // FIXME: IMPLEMENT
        break;
      case VT_LEXER_TOKEN_ERROR:
        // FIXME: IMPLEMENT
        break;
      case VT_LEXER_TOKEN_COMMENT_SINGLE:
      case VT_LEXER_TOKEN_COMMENT_MULTI:
        /* always ignore comments */
        break;
      case VT_LEXER_TOKEN_LEFT_CURLY:
        loop = 0;
        break;
      case VT_LEXER_TOKEN_RIGHT_CURLY:
        // do we even have a title for the current
        // or an identifier... if yes... recurse
        break;
      case VT_LEXER_TOKEN_IDENT:
        val = vt_lexer_get_value (lxr);

        /* option or section name must be known in config defs */
        for (def = defs; def->type != VT_CONFIG_TYPE_NONE; def++) {
          if (def->name && strcmp (val->somechld.string, def->name) == 0)
            break;
        }

        if (def->type == VT_CONFIG_TYPE_NONE) {
          vt_set_error (err, VT_ERR_INVAL);
          vt_error ("unknown option %s on line %u, column %u",
            val->somechld.string, val->lineno, val->colno);
          goto failure;
        }

        if (! (opt = vt_config_create (parent, err))) {
          goto failure;
        }

        (void) strncpy (opt->name, val->somechld.string, VT_CONFIG_NAME_MAX);
        if (def->type == VT_CONFIG_TYPE_SEC)
          if (vt_config_parse_sec (opt, def, lxr, err) < 0)
            goto failure;
        else
          if (vt_config_parse_opt (opt, def, lxr, err) < 0)
            goto failure;

        break;
      default:
        // FIXME: IMPLEMENT
    }



  }

  return 0;
failure:
  /* cleanup */
  return -1;
}


vt_config_t *
vt_config_parse_str (vt_config_def_t *def,
                     const char *str,
                     size_t len,
                     vt_error_t *err)
{
  vt_config_def_t *root_def, *prev_def, *next_def, *itr;
  vt_config_state_t state;
  vt_lexer_t *lxr;
  vt_lexer_def lxr_def;
  vt_lexer_scope_def lxr_scope_def;
  vt_token_t tok;

  assert (def);
  assert (str);

  // 0. populate lexer def, which is used for global defaults!

  // 1. create shiny new lexer

  if (! (vt_lexer_create (&lxr_def, err)))
    return NULL;

  // 2. loop and get all tokens... create little state machine!

  root_def = def;
  prnt_def = def;

  for (;;) {
    tok = vt_lexer_get_token (lxr, lxr_scope_def, err);

    if (tok == VT_TOKEN_COMMENT_SINGLE || tok == VT_TOKEN_COMMENT_MULTI)
      continue; /* always ignore comments */

    if (state == VT_CONFIG_STATE_NONE) {
      if (tok == VT_TOKEN_IDENT) {
        /* if we're in a section, check if the option is known */
        if (def->type == VT_CONFIG_TYPE_SEC) {
          next_def = NULL;
          if (! def->opts)
            goto unexp_opt;

          for (itr = def->opts; itr->type != VT_CONFIG_TYPE_NONE; itr++) {
            if (itr->name && strcmp (lxr->value. itr->name) == 0) {
              next_def = itr;
              break;
            }
          }

          if (! next_def)
            goto unexp_opt;

        /* COMMENT */
        } else {
          if (strcmp (lex
        }
      } else {
        // ERROR!
      }

      // 3. expect option or section... comments can be safely ignored all other
      //    tokens will generate error!

    }
  }

//  char *value;
//  vt_config_t *cfg;
//  vt_lexer_t *lxr;
//  vt_lexer_token_t tok;
//
//  vt_lexer_opts_t lxropts;
//
//  lxropts.class = "abcdefghijklmnopqrstuvwxyz";
//  lxropts.delim = "/\"'";
//  lxropts.sep = ",";
//
//  if (! (lxr = vt_lexer_create (str, len, err))) {
    //goto failure;
  //}
//vt_error ("start parsing!\n");
//  // now we can start parsing this stuff!
//  for (; (tok = vt_lexer_get_token (lxr, &lxropts, err)) != VT_TOKEN_EOF
//   && tok != VT_TOKEN_ERROR;) {
//    //
//    // just try something for know?!?!
//    //
//    value = vt_lexer_gets (lxr);
//    vt_error ("token: %d, value: %s", tok, value);
//  }
//
//  if (tok == VT_TOKEN_ERROR) {
//    vt_error ("ERROR!: %d", *err);
//  }
//
//failure:
//  return NULL;
}


vt_config_t *
vt_config_parse_file (vt_config_def_t *def, const char *path, vt_error_t *err)
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
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: open: %s", __func__, strerror (errno));
    goto failure;
  }
  if (vt_buf_init (&buf, BUFLEN) < 0) {
    vt_set_error (err, VT_ERR_NOMEM);
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
        vt_set_error (err, VT_ERR_NOMEM);
        vt_error ("%s: %s", __func__, strerror (errno));
        goto failure;
      }
    }
    if (nrd < BUFLEN) {
      if (errno) {
        if (errno == EINTR)
          goto again;
        vt_set_error (err, VT_ERR_BADCFG);
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

  vt_error ("conf: %s\n", str);

  if (! (cfg = vt_config_parse_buffer (def, str, len, err)))
    goto failure;

  return cfg;
failure:
  // clean up
  return NULL;
#undef BUFLEN
}
