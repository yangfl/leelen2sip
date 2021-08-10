#ifndef LEELEN_HOST_H
#define LEELEN_HOST_H

#include <stdlib.h>
#include <sys/types.h>

#include <inet46i/in46.h>


struct LeelenHost {
  struct in64_addr;
  char *desc;
  unsigned char type;
};

__attribute__((nonnull))
inline void LeelenHost_destroy (struct LeelenHost *self) {
  free(self->desc);
}

__attribute__((nonnull, warn_unused_result,
               access(write_only, 1), access(read_only, 2)))
int LeelenHost_init (struct LeelenHost *self, const char *report);


#endif /* LEELEN_HOST_H */
