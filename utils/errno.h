#ifndef UTILS_ERRNO_H
#define UTILS_ERRNO_H

#include <errno.h>
#include <stddef.h>


__attribute__((access(write_only, 1)))
inline int saveerrno (int *errnum) {
  if (errnum != NULL) {
    *errnum = errno;
  }
  return errno;
}


#endif /* UTILS_ERRNO_H */
