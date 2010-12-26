#ifndef _REQUEST_H_INCLUDED_
#define _REQUEST_H_INCLUDED_

#include <stdlib.h>
#include <string.h>

#define HELO_NAME       (1)
#define SENDER          (2)
#define SENDER_DOMAIN   (3)
#define CLIENT_ADDRESS  (4)
#define CLIENT_NAME     (5)
#define REV_CLIENT_NAME (6)


typedef struct request_struct request_t;

struct request_struct {
  char *helo_name;
  char *sender;
  char *sender_domain;
  char *client_address;
  char *client_name;
  char *reverse_client_name;
};

request_t *request_from_socket (int);
void request_free (request_t *);
char *request_member (request_t *, int);
int request_member_id (char *);
int request_member_id_len (char *, size_t);

#endif
