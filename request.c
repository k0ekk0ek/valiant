/* system includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* prefix includes */
#include "consts.h"
#include "request.h"
#include "utils.h"

int
vt_request_parse_line (vt_request_t *req, const char *str, size_t len)
{
  char **mbr, *ptr;
  size_t pos;

  if (len > 10 && strncmp (str, "helo_name=", 10) == 0) {
    pos = 10;
    mbr = &(req->helo_name);
  } else if (len > 7 && strncmp (str, "sender=", 7) == 0) {
    pos = 7;
    mbr = &(req->sender);
  } else if (len > 10 && strncmp (str, "recipient=", 10) == 0) {
    pos = 10;
    mbr = &(req->recipient);
  } else if (len > 15 && strncmp (str, "client_address=", 15) == 0) {
    pos = 15;
    mbr = &(req->client_address);
  } else if (len > 12 && strncmp (str, "client_name=", 12) == 0) {
    pos = 12;
    mbr = &(req->client_name);
  } else if (len > 20 && strncmp (str, "reverse_client_name=", 20) == 0) {
    pos = 20;
    mbr = &(req->reverse_client_name);
  } else {
    return VT_ERR_BADMBR;
  }

  if (! (*mbr = strndup (str+pos, len - pos)))
    return VT_ERR_NOMEM;

  if (*mbr == req->sender) {
    ptr = req->sender;
    mbr = &(req->sender_domain);
  } else if (*mbr == req->recipient) {
    ptr = req->recipient;
    mbr = &(req->recipient_domain);
  } else {
    ptr = NULL;
  }

  if (ptr) {
    for (; *ptr && *ptr != '@'; ptr++)
      ;
    if (*ptr == '@')
      *mbr = ++ptr;
  }

  return VT_SUCCESS;
}

#define BUFLEN (32)
#define isterm(c) ((c) == '\n' || (c) == '\r')

int
vt_request_parse (vt_request_t *req, int fildes)
{
  char buf[BUFLEN+1];
  int err;
  size_t i, j, n, nlft;
  ssize_t nrd;

  for (n = 0;;) {
    if ((nlft = BUFLEN - n) == 0)
      return VT_ERR_NOBUFS;

again:
    nrd = read (fildes, buf+n, nlft);
    if (nrd < 0) {
      if (errno == EINTR)
        goto again;
      return VT_ERR_SYS;

    } else if (nrd > 0) {
      n += nrd;

      for (i=0; i < n; ) {
        for (j=i; j < n && !isterm (*(buf+j)); j++)
          ; /* find line terminator */

        if (j < n) {
          err = vt_request_parse_line (req, buf+i, (j-i));
          switch (err) {
            case VT_SUCCESS:
            case VT_ERR_BADMBR: /* doesn't mean anything in this context */
              break;
            default:
              return err;
          }

          for (i=j; i < n &&  isterm (*(buf+i)); i++)
            ; /* forget about line terminator(s) */
        } else {
          break; /* no line terminator(s) */
        }
      }

      if (i && (n - i)) {
        memmove (buf, buf+i, (n - 1));
        n -= i;
      }
    } else {
      break; /* EOF */
    }
  }

  return VT_SUCCESS;
}

#undef isterm
#undef BUFLEN

int
vt_request_destroy (vt_request_t *req)
{
  if (req) {
    if (req->helo_name)
      free (req->helo_name);
    if (req->sender)
      free (req->sender);
    if (req->recipient)
      free (req->recipient);
    if (req->client_address)
      free (req->client_address);
    if (req->client_name)
      free (req->client_name);
    if (req->reverse_client_name)
      free (req->reverse_client_name);
    free (req);
  }
  return VT_SUCCESS;
}

int
vt_request_mbrtoid (vt_request_mbr_t *id, const char *mbr)
{
  if (mbr) {
    if (strncasecmp (mbr, "helo_name", 9) == 0)
      *id = HELO_NAME;
    else if (strncasecmp (mbr, "sender", 6) == 0)
      *id = SENDER;
    else if (strncasecmp (mbr, "sender_domain", 13) == 0)
      *id = SENDER_DOMAIN;
    else if (strncasecmp (mbr, "recipient", 9) == 0)
      *id = RECIPIENT;
    else if (strncasecmp (mbr, "recipient_domain", 16) == 0)
      *id = RECIPIENT_DOMAIN;
    else if (strncasecmp (mbr, "client_address", 14) == 0)
      *id = CLIENT_ADDRESS;
    else if (strncasecmp (mbr, "client_name", 11) == 0)
      *id = CLIENT_NAME;
    else if (strncasecmp (mbr, "reverse_client_name", 19) == 0)
      *id = REV_CLIENT_NAME;

    return VT_SUCCESS;
  }

  return VT_ERR_BADMBR;
}

int
vt_request_mbrbyid (char **mbr, const vt_request_t *req, vt_request_mbr_t id)
{
  if (id == HELO_NAME)
    *mbr = req->helo_name;
  else if (id == SENDER)
    *mbr = req->sender;
  else if (id == SENDER_DOMAIN)
    *mbr = req->sender_domain;
  else if (id == RECIPIENT)
    *mbr = req->recipient;
  else if (id == RECIPIENT_DOMAIN)
    *mbr = req->recipient_domain;
  else if (id == CLIENT_ADDRESS)
    *mbr = req->client_address;
  else if (id == CLIENT_NAME)
    *mbr = req->client_name;
  else if (id == REV_CLIENT_NAME)
    *mbr = req->reverse_client_name;
  else
    return VT_ERR_BADMBR;

  return VT_SUCCESS;
}

int
vt_request_mbrbyname (char **mbr, const vt_request_t *req, const char *str)
{
  if (strncasecmp (str, "helo_name", 9) == 0)
    *mbr = req->helo_name;
  else if (strncasecmp (str, "sender", 6) == 0)
    *mbr = req->sender;
  else if (strncasecmp (str, "sender_domain", 13) == 0)
    *mbr = req->sender_domain;
  else if (strncasecmp (str, "recipient", 9) == 0)
    *mbr = req->recipient;
  else if (strncasecmp (str, "recipient_domain", 16) == 0)
    *mbr = req->recipient_domain;
  else if (strncasecmp (str, "client_address", 14) == 0)
    *mbr = req->client_address;
  else if (strncasecmp (str, "client_name", 11) == 0)
    *mbr = req->client_name;
  else if (strncasecmp (str, "reverse_client_name", 19) == 0)
    *mbr = req->reverse_client_name;
  else
    return VT_ERR_BADMBR;

  return VT_SUCCESS;
}

int
vt_request_mbrbynamen (char **mbr, const vt_request_t *req, const char *str,
  size_t len)
{
  if (len == 9  && ! strncmp ("helo_name", str, len))
    *mbr = req->helo_name;
  else if (len == 6  && ! strncmp ("sender", str, len))
    *mbr = req->sender;
  else if (len == 13 && ! strncmp ("sender_domain", str, len))
    *mbr = req->sender_domain;
  else if (len == 9  && ! strncmp ("recipient", str, len))
    *mbr = req->recipient;
  else if (len == 16 && ! strncmp ("recipient_domain", str, len))
    *mbr = req->recipient_domain;
  else if (len == 14 && ! strncmp ("client_address", str, len))
    *mbr = req->client_address;
  else if (len == 11 && ! strncmp ("client_name", str, len))
    *mbr = req->client_name;
  else if (len == 19 && ! strncmp ("reverse_client_name", str, len))
    *mbr = req->reverse_client_name;
  else
    return VT_ERR_BADMBR;

  return VT_SUCCESS;
}
