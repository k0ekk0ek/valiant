/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* prefix includes */
#include "request.h"
#include "utils.h"


#define BUFSIZE (1024)

// vt_request_t *
// vt_request_read (int fd)
// {
  // char buf[BUFSIZE], *attr;
  // int ret;
  // vt_request_t *request;

  // if ((request = malloc0 (sizeof (vt_request_t))) == NULL)
    // return NULL;

  // for (;;) {
    // ret = readline (fd, buf, BUFSIZE);

    // if (ret == EMPTY)
      // break;
    // if (ret == ERROR) {
      // request_free (request);
      // return NULL; /* errno set by readline */
    // }

    // if (request->helo_name == NULL && strncmp (buf, "helo_name", 10) == 0)
      // attr = request->helo_name = strdup (buf+10);
    // else if (request->sender == NULL && strncmp (buf, "sender=", 7) == 0)
      // attr = request->sender = strdup (buf+7);
    // else if (request->client_address == NULL && strncmp (buf, "client_address=", 15) == 0)
      // attr = request->client_address = strdup (buf+15);
    // else if (request->client_name == NULL && strncmp (buf, "client_name=", 12) == 0)
      // attr = request->client_name = strdup (buf+12);
    // else if (request->reverse_client_name == NULL && strncmp (buf, "reverse_client_name", 19) == 0)
      // attr = request->reverse_client_name = strdup (buf+19);
    // else if (strlen (buf) == 0)
      // break; /* our work here is done */
    // else
      // continue; /* skip unwanted attributes */

    // if (! attr) {
      // vt_request_free (request);
      // return NULL; /* errno set by strdup */
    // }
  // }

  // // FIXME: validate request attributes

  // return request;
// }

#undef BUFSIZE

void
vt_request_free (vt_request_t *request)
{
  if (request) {
    if (request->helo_name)
      free (request->helo_name);
    if (request->sender)
      free (request->sender);
    if (request->client_address)
      free (request->client_address);
    if (request->client_name)
      free (request->client_name);
    if (request->reverse_client_name)
      free (request->reverse_client_name);
    free (request);
  }

  return;
}

int
vt_request_attrtoid (char *str)
{
  if (strncasecmp (str, "helo_name", 9) == 0)
    return HELO_NAME;
  if (strncasecmp (str, "sender", 6) == 0)
    return SENDER;
  if (strncasecmp (str, "sender_domain", 13) == 0)
    return SENDER_DOMAIN;
  if (strncasecmp (str, "client_address", 14) == 0)
    return CLIENT_ADDRESS;
  if (strncasecmp (str, "client_name", 11) == 0)
    return CLIENT_NAME;
  if (strncasecmp (str, "reverse_client_name", 19) == 0)
    return REV_CLIENT_NAME;

  return -1;
}

char *
vt_request_attrbyid (vt_request_t *request, int id)
{
  switch (id) {
    case HELO_NAME:
      return request->helo_name;
    case SENDER:
      return request->sender;
    case CLIENT_ADDRESS:
      return request->client_address;
    case CLIENT_NAME:
      return request->client_name;
    case REV_CLIENT_NAME:
      return request->reverse_client_name;
  }

  return NULL;
}

char *
vt_request_attrbyname (vt_request_t *request, char *str)
{
  if (strncasecmp (str, "helo_name", 9) == 0)
    return request->helo_name;
  if (strncasecmp (str, "sender", 6) == 0)
    return request->sender;
  if (strncasecmp (str, "client_address", 14) == 0)
    return request->client_address;
  if (strncasecmp (str, "client_name", 11) == 0)
    return request->client_name;
  if (strncasecmp (str, "reverse_client_name", 19) == 0)
    return request->reverse_client_name;

  return NULL;
}

char *
vt_request_attrbynamen (vt_request_t *request, char *str, size_t len)
{
  if (len == 9  && ! strncmp ("helo_name", str, len))
    return request->helo_name;
  if (len == 6  && ! strncmp ("sender", str, len))
    return request->sender;
  if (len == 14 && ! strncmp ("client_address", str, len))
    return request->client_address;
  if (len == 11 && ! strncmp ("client_name", str, len))
    return request->client_name;
  if (len == 19 && ! strncmp ("reverse_client_name", str, len))
    return request->reverse_client_name;

  return NULL;
}

