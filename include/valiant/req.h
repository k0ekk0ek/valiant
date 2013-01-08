#ifndef VT_REQUEST_H_INCLUDED
#define VT_REQUEST_H_INCLUDED 1

/* valiant includes */
#include "buf.h"
#include "error.h"

typedef enum _vt_request_member vt_request_member_t;

enum _vt_request_member {
  VT_REQUEST_MEMBER_NONE,
  VT_REQUEST_MEMBER_HELO_NAME,
  VT_REQUEST_MEMBER_SENDER,
  VT_REQUEST_MEMBER_SENDER_DOMAIN,
  VT_REQUEST_MEMBER_RECIPIENT,
  VT_REQUEST_MEMBER_RECIPIENT_DOMAIN,
  VT_REQUEST_MEMBER_CLIENT_ADDRESS,
  VT_REQUEST_MEMBER_CLIENT_NAME,
  VT_REQUEST_MEMBER_REV_CLIENT_NAME
};

/* By using vt_buf_t buffers instead of char pointers we hopefully cut down the
   number of mallocs significantly. */
typedef struct _vt_request vt_request_t;

struct _vt_request {
  vt_buf_t helo_name;
  vt_buf_t sender;
  vt_buf_t sender_domain;
  vt_buf_t recipient;
  vt_buf_t recipient_domain;
  vt_buf_t client_address;
  vt_buf_t client_name;
  vt_buf_t rev_client_name;
};

vt_request_t *vt_request_create (vt_error_t *);
void vt_request_destroy (vt_request_t *);
vt_request_t *vt_request_parse (vt_request_t *, int, vt_error_t *);
vt_request_member_t vt_request_mbrtoid (const char *);
char *vt_request_mbrbyid (const vt_request_t *, vt_request_member_t);
char *vt_request_mbrbyname (const vt_request_t *, const char *);
char *vt_request_mbrbynamen (const vt_request_t *, const char *, size_t);

#endif
