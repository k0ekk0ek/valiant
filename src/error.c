/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <unistd.h>

/* valiant includes */
#include <valiant/error.h>
#include <valiant/string.h>



static pthread_key_t error_key;
static pthread_once_t error_once = PTHREAD_ONCE_INIT;

#define ERROR_MIN_LEN 64
#define ERROR_MAX_LEN 1024

static const char error_unkown[] = "Unkown error";
static const char error_bad_request[] = "Invalid request";



static void
error_key_create()
{
    (void)pthread_key_create (&error_key, &free);
}

const char *
strferrno (int err)
{
  size_t len;
  size_t tot;
  char *str = NULL;
  char *ptr = NULL;

  if (err >= 0) {
    (void)pthread_once (&error_once, &error_key_create);
    str = pthread_getspecific (error_key);
    if (str == NULL) {
      len = ERROR_MIN_LEN;
      tot = sizeof (len) + len + 1;
      str = calloc (1, tot);
      if (str != NULL) {
        memcpy (str, &len, sizeof (len));
        if (pthread_setspecific (error_key, str) != 0) {
          free (str);
          str = NULL;
        }
      }
    }

    for (ptr = str; ptr != NULL; ) {
      memcpy (&len, ptr, sizeof (len));
      memset (ptr + sizeof (len), '\0', len);

      if (strerror_r (err, ptr + sizeof (len), len) != 0) {
        if (len < ERROR_MAX_LEN) {
          tot = sizeof (len) + (len * 2) + 1;
          ptr = calloc (1, tot);
          if (ptr != NULL) {
            memcpy (ptr + sizeof (len), str + sizeof (len), len);
            len *= 2;
            memcpy (ptr, &len, sizeof (len));
            if (pthread_setspecific (error_key, str) != 0) {
              free (ptr);
              ptr = NULL;
            } else {
              free (str);
              str = ptr;
            }
          }
        } else {
          ptr = NULL;
        }
      } else {
        ptr = NULL;
      }
    }

    if (str != NULL) {
      str += sizeof (len);
      if (*str == '\0') {
        str = NULL;
      }
    }

  } else {
    switch (err) {
      case ERROR_BAD_REQUEST:
        str = error_bad_request;
        break;
      default:
        break;
    }
  }

  if (str == NULL) {
    str = error_unknown;
  }

  return str;
}

