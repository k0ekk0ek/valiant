/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* prefix includes */
#include "consts.h"
#include "request.h"
#include "utils.h"

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
vt_request_mbrbyid (char **mbr, const vt_request_t *request, vt_request_mbr_t id)
{
  if (id == HELO_NAME)
    *mbr = request->helo_name;
  else if (id == SENDER)
    *mbr = request->sender;
  else if (id == SENDER_DOMAIN)
    *mbr = request->sender_domain;
  else if (id == RECIPIENT)
    *mbr = request->recipient;
  else if (id == RECIPIENT_DOMAIN)
    *mbr = request->recipient_domain;
  else if (id == CLIENT_ADDRESS)
    *mbr = request->client_address;
  else if (id == CLIENT_NAME)
    *mbr = request->client_name;
  else if (id == REV_CLIENT_NAME)
    *mbr = request->reverse_client_name;
  else
    return VT_ERR_BADMBR;

  return VT_SUCCESS;
}

int
vt_request_mbrbyname (char **mbr, const vt_request_t *request, const char *str)
{
  if (strncasecmp (str, "helo_name", 9) == 0)
    *mbr = request->helo_name;
  else if (strncasecmp (str, "sender", 6) == 0)
    *mbr = request->sender;
  else if (strncasecmp (str, "sender_domain", 13) == 0)
    *mbr = request->sender_domain;
  else if (strncasecmp (str, "recipient", 9) == 0)
    *mbr = request->recipient;
  else if (strncasecmp (str, "recipient_domain", 16) == 0)
    *mbr = request->recipient_domain;
  else if (strncasecmp (str, "client_address", 14) == 0)
    *mbr = request->client_address;
  else if (strncasecmp (str, "client_name", 11) == 0)
    *mbr = request->client_name;
  else if (strncasecmp (str, "reverse_client_name", 19) == 0)
    *mbr = request->reverse_client_name;
  else
    return VT_ERR_BADMBR;

  return VT_SUCCESS;
}

int
vt_request_mbrbynamen (char **mbr, const vt_request_t *request, const char *str,
  size_t len)
{
  if (len == 9  && ! strncmp ("helo_name", str, len))
    *mbr = request->helo_name;
  else if (len == 6  && ! strncmp ("sender", str, len))
    *mbr = request->sender;
  else if (len == 13 && ! strncmp ("sender_domain", str, len))
    *mbr = request->sender_domain;
  else if (len == 9  && ! strncmp ("recipient", str, len))
    *mbr = request->recipient;
  else if (len == 16 && ! strncmp ("recipient_domain", str, len))
    *mbr = request->recipient_domain;
  else if (len == 14 && ! strncmp ("client_address", str, len))
    *mbr = request->client_address;
  else if (len == 11 && ! strncmp ("client_name", str, len))
    *mbr = request->client_name;
  else if (len == 19 && ! strncmp ("reverse_client_name", str, len))
    *mbr = request->reverse_client_name;
  else
    return VT_ERR_BADMBR;

  return VT_SUCCESS;
}
