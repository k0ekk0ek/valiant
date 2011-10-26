#ifndef VT_REQUEST_H_INCLUDED
#define VT_REQUEST_H_INCLUDED

/* system includes */
#include <sys/types.h>

#define HELO_NAME       (1)
#define SENDER          (2)
#define SENDER_DOMAIN   (3)
#define CLIENT_ADDRESS  (4)
#define CLIENT_NAME     (5)
#define REV_CLIENT_NAME (6)

typedef struct vt_request_struct vt_request_t;

struct vt_request_struct {
  char *helo_name;
  char *sender;
  char *client_address;
  char *client_name;
  char *reverse_client_name;
};

vt_request_t *vt_request_read (int);
void vt_request_free (vt_request_t *);
int vt_request_attrtoid (char *);
char *vt_request_attrbyid (vt_request_t *, int);
char *vt_request_attrbyname (vt_request_t *, char *);
char *vt_request_attrbynamen (vt_request_t *, char *, size_t);

#endif

