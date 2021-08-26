#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/macro.h"
#include "string.h"


extern inline int amemcat (
  void * restrict *dest, size_t * restrict len, const void * restrict src,
  size_t num);
extern inline int strcmp0 (const char *s1, const char *s2);

int astrcatprintf (
    char * restrict *dest, size_t * restrict len, const char * restrict format,
    ...) {
  int ret = 0;
  int bufsize = 256;
  int printlen;

  va_list args;
  va_start(args, format);
  while (1) {
    // expand
    {
      char *newstr = realloc(*dest, *len + bufsize);
      should (newstr != NULL) otherwise {
        ret = -1;
        goto fail;
      }
      *dest = newstr;
    }
    // print
    printlen = vsnprintf(*dest + *len, bufsize, format, args);
    should (printlen >= 0) otherwise {
      ret = -2;
      goto fail;
    }
    // check
    break_if (printlen < bufsize);
    bufsize = printlen + 1;
  }
  *len += printlen;

fail:
  va_end(args);
  return ret;
}
