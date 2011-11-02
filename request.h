#ifndef VT_REQUEST_H_INCLUDED
#define VT_REQUEST_H_INCLUDED

/* system includes */
#include <sys/types.h>

typedef enum vt_request_mbr_enum vt_request_mbr_t;

enum vt_request_mbr_enum {
  HELO_NAME,
  SENDER,
  SENDER_DOMAIN,
  RECIPIENT,
  RECIPIENT_DOMAIN,
  CLIENT_ADDRESS,
  CLIENT_NAME,
  REV_CLIENT_NAME
};

typedef struct vt_request_struct vt_request_t;

struct vt_request_struct {
  char *helo_name;
  char *sender;
  char *sender_domain; /* pointer to first char after @ in sender */
  char *recipient;
  char *recipient_domain; /* pointer to first char after @ in recipient */
  char *client_address;
  char *client_name;
  char *reverse_client_name;
};

int vt_request_mbrtoid (vt_request_mbr_t *, const char *);
int vt_request_mbrbyid (char **, vt_request_t *, vt_request_mbr_t *);
int vt_request_mbrbyname (char **, vt_request_t *, char *);
int vt_request_mbrbynamen (char **, vt_request_t *, char *, size_t);

#endif
