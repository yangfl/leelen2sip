#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "utils/macro.h"


__attribute__((warn_unused_result, nonnull, access(read_only, 3)))
inline int amemcat (
    void * restrict *dest, size_t * restrict len, const void * restrict src,
    size_t num) {
  char *ret = realloc(*dest, *len + num);
  return_if_fail (ret != NULL) -1;
  memcpy(ret + *len, src, num);
  *len += num;
  *dest = ret;
  return 0;
}

#define amemcats(dest, len, str) amemcat(dest, len, str, strlen(str))

__attribute__((warn_unused_result, nonnull, access(read_only, 3),
               format(printf, 3, 4)))
int astrcatprintf (
  char * restrict *dest, size_t * restrict len, const char * restrict format,
  ...);

#define stricmp(s1, s2) strncmp(s1, s2, sizeof(s2) - 1)

__attribute__((warn_unused_result, access(read_only, 1), access(read_only, 2)))
inline int strcmp0 (const char *s1, const char *s2) {
  return s1 == NULL && s2 == NULL ? 0 : s1 == NULL || s2 == NULL ? 2 :
         strcmp(s1, s2);
}


#ifdef __cplusplus
}
#endif

#endif /* UTILS_STRING_H */
