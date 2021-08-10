#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include <inet46i/inet46.h>

#include "utils/macro.h"
#include "host.h"


extern inline void LeelenHost_destroy (struct LeelenHost *self);


int LeelenHost_init (struct LeelenHost *self, const char *report) {
  int ret = 0;

  // addr
  char *msg_addr_sep = strchr(report, '?');
  {
    char saddr[INET6_ADDRSTRLEN];
    unsigned int addrlen = msg_addr_sep != NULL ?
      (unsigned int) (msg_addr_sep - report) : strnlen(report, sizeof(saddr));
    if (addrlen >= sizeof(saddr)) {
      ret |= 1;
    } else {
      memcpy(saddr, report, addrlen);
      saddr[addrlen] = '\0';
      should (inet_aton64(saddr, &self->addr6) != 0) otherwise {
        ret |= 1;
      }
    }
  }
  return_if_fail (msg_addr_sep != NULL) ret | 2 | 4;

  // type
  char *msg_type_end;
  self->type = strtol(msg_addr_sep + 1, &msg_type_end, 10);
  should (*msg_type_end == '*') otherwise {
    ret |= 2;
    msg_type_end = strchr(msg_type_end, '*');
  }

  // desc
  if (msg_type_end == NULL || *msg_type_end == '\0') {
    ret |= 4;
  } else {
    self->desc = strdup(msg_type_end + 1);
    should (self->desc != NULL) otherwise {
      ret |= 4;
    }
  }

  return ret;
}
