/* valiant - a postfix policy daemon */

/* system includes */
#include <confuse.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

/* valiant includes */
#include "error.h"
#include "dict_dnsbl.h"
#include "dict_spf.h"
#include "dict_str.h"
#include "dict_hash.h"
#include "dict_pcre.h"

int
main (int argc, char *argv[])
{
  if (argc < 2)
    return 1;

  cfg_opt_t dict_in_opts[] = {
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t dict_spf_opts[] = {
    CFG_FLOAT ("weight", 0.0, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t dict_opts[] = {
    CFG_SEC ("in", dict_in_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("pass", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("fail", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("soft_fail", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("neutral", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("none", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("perm_error", dict_spf_opts, CFGF_NONE),
    CFG_SEC ("temp_error", dict_spf_opts, CFGF_NONE),
    CFG_STR ("path", 0, CFGF_NODEFAULT),
    CFG_STR ("member", 0, CFGF_NODEFAULT),
    CFG_STR ("pattern", 0, CFGF_NODEFAULT),
    CFG_BOOL ("invert", cfg_false, CFGF_NONE),
    CFG_BOOL ("nocase", cfg_false, CFGF_NONE),
    CFG_STR ("type", 0, CFGF_NODEFAULT),
    CFG_STR ("zone", 0, CFGF_NODEFAULT),
    CFG_BOOL ("ipv4", cfg_true, CFGF_NONE),
    CFG_BOOL ("ipv6", cfg_false, CFGF_NONE),
    CFG_FLOAT ("weight", 1.0, CFGF_NONE),
    CFG_INT ("max_idle_threads", 10, CFGF_NONE),
    CFG_INT ("max_queued", 200, CFGF_NONE),
    CFG_INT ("max_threads", 100, CFGF_NONE),
    CFG_INT ("min_threads", 10, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t type_opts[] = {
    CFG_INT ("max_idle_threads", 10, CFGF_NONE),
    CFG_INT ("max_queued", 200, CFGF_NONE),
    CFG_INT ("max_threads", 100, CFGF_NONE),
    CFG_INT ("min_threads", 10, CFGF_NONE),
    CFG_END ()    
  };

  cfg_opt_t opts[] = {
    CFG_SEC ("dict", dict_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC ("type", type_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_INT ("max_idle_threads", 10, CFGF_NONE),
    CFG_INT ("max_queued", 200, CFGF_NONE),
    CFG_INT ("max_threads", 100, CFGF_NONE),
    CFG_INT ("min_threads", 10, CFGF_NONE),
    CFG_END ()
  };

  cfg_t *cfg = cfg_init (opts, CFGF_NONE);
  cfg_t *dnsbl_sec, *dnsbl_type_sec, *str_sec, *spf_sec, *spf_type_sec, *hash_sec;
  cfg_t *pcre_sec, *pcre_type_sec;

  switch(cfg_parse(cfg, argv[1])) {
    case CFG_FILE_ERROR:
    case CFG_PARSE_ERROR:
      return 1;
  }

  vt_dict_t *dnsbl, str;

  int i, n;
  cfg_t *sec;
  char *type;
  dnsbl_sec = NULL;
  dnsbl_type_sec = NULL;
  spf_sec = NULL;
  spf_type_sec = NULL;
  str_sec = NULL;
  hash_sec = NULL;
  pcre_sec = NULL;
  pcre_type_sec = NULL;

  for (i = 0, n = cfg_size (cfg, "type"); i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "type", i)) &&
        (type = (char *)cfg_title (sec)))
    {
      if (strcmp (type, "dnsbl") == 0)
        dnsbl_type_sec = sec;
      else if (strcmp (type, "spf") == 0)
        spf_type_sec = sec;
      break;
    }
  }

  for (i = 0, n = cfg_size (cfg, "dict"); i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "dict", i)) &&
        (type = cfg_getstr (sec, "type")))
    {
      if (! dnsbl_sec && strcmp (type, "dnsbl") == 0)
        dnsbl_sec = sec;
      else if (! str_sec && strcmp (type, "str") == 0)
        str_sec = sec;
      else if (! spf_sec && strcmp (type, "spf") == 0)
        spf_sec = sec;
      else if (! hash_sec && strcmp (type, "hash") == 0)
        hash_sec = sec;
      else if (! pcre_sec && strcmp (type, "pcre") == 0)
        pcre_sec = sec;
    }
  }

  vt_request_t *req = vt_request_create (NULL);
  int fd = open ("request.txt", O_RDONLY);

  vt_request_parse (req, fd, NULL);
vt_error ("%s:%d: helo_name: %s", __func__, __LINE__, vt_request_mbrbyid (req, VT_REQUEST_MEMBER_HELO_NAME));
  // IMPLEMENT
  // - The idea now is to create to checks, one for dnsbl, and one for str
  //   and verify the dnsbl check is running in a thread pool.

  vt_dict_type_t *dnsbl_type = vt_dict_dnsbl_type ();
  vt_dict_type_t *str_type = vt_dict_str_type ();
  vt_dict_type_t *spf_type = vt_dict_spf_type ();
  vt_dict_type_t *hash_type = vt_dict_hash_type ();
  vt_dict_type_t *pcre_type = vt_dict_pcre_type ();

  vt_error_t err;
  vt_dict_t *str_dict, *spf_dict, *dnsbl_dict, *hash_dict, *pcre_dict;
  str_dict = spf_dict = dnsbl_dict = hash_dict = pcre_dict = NULL;
vt_error ("%s:%d:", __func__, __LINE__);
  if (str_sec)
    str_dict = vt_dict_create (str_type, NULL, str_sec, &err);
vt_error ("%s:%d:", __func__, __LINE__);
  if (dnsbl_sec)
    dnsbl_dict = vt_dict_create (dnsbl_type, dnsbl_type_sec, dnsbl_sec, &err);
vt_error ("%s:%d:", __func__, __LINE__);
  if (spf_sec)
    spf_dict = vt_dict_create (spf_type, spf_type_sec, spf_sec, &err);
vt_error ("%s:%d:", __func__, __LINE__);
  if (hash_sec)
    hash_dict = vt_dict_create (hash_type, NULL, hash_sec, &err);
vt_error ("%s:%d:", __func__, __LINE__);
  if (pcre_sec)
    pcre_dict = vt_dict_create (pcre_type, NULL, pcre_sec, &err);
vt_error ("%s:%d:", __func__, __LINE__);
  vt_result_t *res = vt_result_create (NULL);

  if (dnsbl_dict)
    dnsbl_dict->check_func (dnsbl_dict, req, res, NULL);
  if (spf_dict)
    spf_dict->check_func (spf_dict, req, res, NULL);
  if (str_dict)
    str_dict->check_func (str_dict, req, res, NULL);
  if (hash_dict)
    hash_dict->check_func (hash_dict, req, res, NULL);
  if (pcre_dict)
    pcre_dict->check_func (pcre_dict, req, res, NULL);

  vt_result_wait (res);
vt_error ("%s:%d", __func__, __LINE__);
  return 0;
}
