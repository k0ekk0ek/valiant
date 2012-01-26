#ifndef VT_BUF_H_INCLUDED
#define VT_BUF_H_INCLUDED 1

/* system includes */
#include <sys/types.h>

typedef struct _vt_buf vt_buf_t;

struct _vt_buf {
  char *buf;
  size_t len;
  size_t cnt;
};

#define vt_buf_str(s) ((s)->buf)
#define vt_buf_len(s) ((s)->cnt)

int vt_buf_init (vt_buf_t *, size_t);
vt_buf_t *vt_buf_ncpy (vt_buf_t *, const char *, size_t);
vt_buf_t *vt_buf_ncat (vt_buf_t *, const char *, size_t);
void vt_buf_deinit (vt_buf_t *);

#endif
