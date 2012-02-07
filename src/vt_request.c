/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* prefix includes */
#include "buf.h"
#include "error.h"
#include "request.h"

vt_request_t *
vt_request_parse_line (vt_request_t *req, const char *str, size_t len,
  vt_error_t *err)
{
  vt_buf_t *mbr;
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
    mbr = &(req->rev_client_name);
  } else {
    mbr = NULL;
  }

  if (mbr) {
    if (! vt_buf_ncpy (mbr, str+pos, len-pos)) {
      vt_set_error (err, VT_ERR_NOMEM);
      vt_error ("%s: %s", __func__, strerror (ENOMEM));
      return NULL;
    }

    if (mbr == &(req->sender)) {
      mbr = &(req->sender_domain);
    } else if (mbr == &(req->recipient)) {
      mbr = &(req->recipient_domain);
    } else {
      mbr = NULL;
    }

    if (mbr) {
      for (; pos < len && *(str+pos) != '@'; pos++)
        ;
      if (*(str+pos) == '@' && ++pos < len) {
        if (! vt_buf_ncpy (mbr, str+pos, len-pos)) {
          vt_set_error (err, VT_ERR_NOMEM);
          vt_error ("%s: %s", __func__, strerror (ENOMEM));
          return NULL;
        }
      }
    }
  }

  return req;
}

#define BUFLEN (1024)
#define isterm(c) ((c) == '\n' || (c) == '\r')

vt_request_t *
vt_request_parse (vt_request_t *req, int fildes, vt_error_t *err)
{
  char buf[BUFLEN+1];
  size_t i, j, n, nlft;
  ssize_t nrd;

  for (n = 0;;) {
    if ((nlft = BUFLEN - n) == 0) {
      vt_set_error (err, VT_ERR_NOBUFS);
      vt_error ("%s: not enough space in line buffer", __func__);
      return NULL;
    }
again:
    nrd = read (fildes, buf+n, nlft);
    if (nrd < 0) {
      if (errno == EINTR)
        goto again;

      vt_set_error (err, VT_ERR_NORETRY);
      vt_error ("%s: read: %s", __func__, strerror (errno));
      return NULL;

    } else if (nrd > 0) {
      n += nrd;

      for (i=0; i < n; ) {
        for (j=i; j < n && !isterm (*(buf+j)); j++)
          ; /* find line terminator */

        if (j < n) {
          if (! vt_request_parse_line (req, buf+i, (j-i), err))
            return NULL;

          for (i=j; i < n &&  isterm (*(buf+i)); i++)
            ; /* forget about line terminator(s) */
        } else {
          break; /* no line terminator(s) */
        }
      }

      if (i) {
        if ((n - i)) {
          memmove (buf, buf+i, (n - 1));
          n -= i;
        } else if (n < BUFLEN) {
          goto done; /* end of request */
        }
      }
    } else {
      break; /* end of file */
    }
  }
done:
  return req;
}

#undef isterm
#undef BUFLEN

vt_request_t *
vt_request_create (vt_error_t *err)
{
  vt_request_t *req;

  if (! (req = calloc (1, sizeof (vt_request_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
  }
  return req;
}

void
vt_request_destroy (vt_request_t *req)
{
  vt_buf_deinit (&req->helo_name);
  vt_buf_deinit (&req->sender);
  vt_buf_deinit (&req->sender_domain);
  vt_buf_deinit (&req->recipient);
  vt_buf_deinit (&req->recipient_domain);
  vt_buf_deinit (&req->client_address);
  vt_buf_deinit (&req->client_name);
  vt_buf_deinit (&req->rev_client_name);
  free (req);
}

vt_request_member_t
vt_request_mbrtoid (const char *mbr)
{
  if (! mbr)
    return VT_REQUEST_MEMBER_NONE;
  else if (strncmp (mbr, "helo_name", 9) == 0)
    return VT_REQUEST_MEMBER_HELO_NAME;
  else if (strncmp (mbr, "sender", 6) == 0)
    return VT_REQUEST_MEMBER_SENDER;
  else if (strncmp (mbr, "sender_domain", 13) == 0)
    return VT_REQUEST_MEMBER_SENDER_DOMAIN;
  else if (strncmp (mbr, "recipient", 9) == 0)
    return VT_REQUEST_MEMBER_RECIPIENT;
  else if (strncmp (mbr, "recipient_domain", 16) == 0)
    return VT_REQUEST_MEMBER_RECIPIENT_DOMAIN;
  else if (strncmp (mbr, "client_address", 14) == 0)
    return VT_REQUEST_MEMBER_CLIENT_ADDRESS;
  else if (strncmp (mbr, "client_name", 11) == 0)
    return VT_REQUEST_MEMBER_CLIENT_NAME;
  else if (strncmp (mbr, "reverse_client_name", 19) == 0)
    return VT_REQUEST_MEMBER_REV_CLIENT_NAME;

  return VT_REQUEST_MEMBER_NONE;
}

char *
vt_request_mbrbyid (const vt_request_t *req, vt_request_member_t mbrid)
{
  if (mbrid == VT_REQUEST_MEMBER_HELO_NAME)
    return vt_buf_str (&(req->helo_name));
  else if (mbrid == VT_REQUEST_MEMBER_SENDER)
    return vt_buf_str (&(req->sender));
  else if (mbrid == VT_REQUEST_MEMBER_SENDER_DOMAIN)
    return vt_buf_str (&(req->sender_domain));
  else if (mbrid == VT_REQUEST_MEMBER_RECIPIENT)
    return vt_buf_str (&(req->recipient));
  else if (mbrid == VT_REQUEST_MEMBER_RECIPIENT_DOMAIN)
    return vt_buf_str (&(req->recipient_domain));
  else if (mbrid == VT_REQUEST_MEMBER_CLIENT_ADDRESS)
    return vt_buf_str (&(req->client_address));
  else if (mbrid == VT_REQUEST_MEMBER_CLIENT_NAME)
    return vt_buf_str (&(req->client_name));
  else if (mbrid == VT_REQUEST_MEMBER_REV_CLIENT_NAME)
    return vt_buf_str (&(req->rev_client_name));

  return NULL;
}

char *
vt_request_mbrbyname (const vt_request_t *req, const char *str)
{
  if (strncmp (str, "helo_name", 9) == 0)
    return vt_buf_str (&(req->helo_name));
  else if (strncmp (str, "sender", 6) == 0)
    return vt_buf_str (&(req->sender));
  else if (strncmp (str, "sender_domain", 13) == 0)
    return vt_buf_str (&(req->sender_domain));
  else if (strncmp (str, "recipient", 9) == 0)
    return vt_buf_str (&(req->recipient));
  else if (strncmp (str, "recipient_domain", 16) == 0)
    return vt_buf_str (&(req->recipient_domain));
  else if (strncmp (str, "client_address", 14) == 0)
    return vt_buf_str (&(req->client_address));
  else if (strncmp (str, "client_name", 11) == 0)
    return vt_buf_str (&(req->client_name));
  else if (strncmp (str, "reverse_client_name", 19) == 0)
    return vt_buf_str (&(req->rev_client_name));

  return NULL;
}

char *
vt_request_mbrbynamen (const vt_request_t *req, const char *str, size_t len)
{
  if (len == 9  && ! strncmp ("helo_name", str, len))
    return vt_buf_str (&(req->helo_name));
  else if (len == 6  && ! strncmp ("sender", str, len))
    return vt_buf_str (&(req->sender));
  else if (len == 13 && ! strncmp ("sender_domain", str, len))
    return vt_buf_str (&(req->sender_domain));
  else if (len == 9  && ! strncmp ("recipient", str, len))
    return vt_buf_str (&(req->recipient));
  else if (len == 16 && ! strncmp ("recipient_domain", str, len))
    return vt_buf_str (&(req->recipient_domain));
  else if (len == 14 && ! strncmp ("client_address", str, len))
    return vt_buf_str (&(req->client_address));
  else if (len == 11 && ! strncmp ("client_name", str, len))
    return vt_buf_str (&(req->client_name));
  else if (len == 19 && ! strncmp ("reverse_client_name", str, len))
    return vt_buf_str (&(req->rev_client_name));

  return NULL;
}
