/* system includes */

#include <glib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* prefix includes */
#include "request.h"
#include "utils.h"

request_t *
request_from_socket (gint socket)
{
  request_t *request;

  gchar *member;
  gchar *line;
  gsize  len;

  if ((request = g_new0 (request_t, 1))) {
    // do ... while
    //
    line = g_malloc0 (1024);
    // handle errors!
    //
    //readline
    gint bla;

    for (;;) {
      g_printf ("reading line\n");
    bla = readline (socket, line, 1024);
    // handle errors!
    //gint

    if (bla > 0) {
      if (! request->helo_name
       && ! strncmp (line, "helo_name=", 10))
      {
        member = request->helo_name = strdup (line+10);

      } else if (! request->sender 
              && ! strncmp (line, "sender=", 7))
      {
        member = request->sender = strdup (line+7);

      } else if (! request->client_address
              && ! strncmp (line, "client_address=", 15))
      {
        member = request->client_address = strdup (line+15);

      } else if (! request->client_name
              && ! strncmp (line, "client_name=", 12))
      {
        member = request->client_name = strdup (line+12);

      } else if (! request->reverse_client_name
              && ! strncmp (line, "reverse_client_name", 19))
      {
        member = request->reverse_client_name = strdup (line+19);
      } else if (strlen (line) == 0) {
        g_printf ("empty line... breaking\n");
        break;
      } else {
        continue; //skip unwanted attributes
      }

      if (! member) {
        g_warning ("%s: %s", __func__, strerror (errno));
        // actually this is not acceptable... it could cause a memory leak!
        return NULL;
      }
    } else if (bla == 0) { // empty
      break;
    }
    }


  } else {
    g_warning ("%s: some error", __func__);
  }

  return request;
}


void
request_free (request_t *request)
{
  //if (request)
  //  free (request);
  // XXX: this of course won't do!

  return;
}


char *
request_member (request_t *request, int id)
{
  char *member = NULL;

  switch (id) {
    case HELO_NAME:
      member = request->helo_name;
      break;
    case SENDER:
      member = request->sender;
      break;
    case SENDER_DOMAIN:
      member = request->sender_domain;
      break;
    case CLIENT_ADDRESS:
      member = request->client_address;
      break;
    case CLIENT_NAME:
      member = request->client_name;
      break;
    case REV_CLIENT_NAME:
      member = request->reverse_client_name;
      break;
  }

  return member;
}


int
request_member_id (char *str)
{
  return request_member_id_len (str, strlen (str));
}


int
request_member_id_len (char *str, size_t len)
{
  if (len == 9  && ! strncmp ("helo_name", str, len))
    return HELO_NAME;
  if (len == 6  && ! strncmp ("sender", str, len))
    return SENDER;
  if (len == 13 && ! strncmp ("sender_domain", str, len))
    return SENDER_DOMAIN;
  if (len == 14 && ! strncmp ("client_address", str, len))
    return CLIENT_ADDRESS;
  if (len == 11 && ! strncmp ("client_name", str, len))
    return CLIENT_NAME;
  if (len == 19 && ! strncmp ("reverse_client_name", str, len))
    return REV_CLIENT_NAME;

  return -1;
}
