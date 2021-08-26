#include <errno.h>
#include <netinet/in.h>

#include "private/macro.h"
#include "route.h"


extern inline in_addr_t inet_tomask (int prefix);
extern inline bool inet_isnetwork4 (
  struct in_addr network, int prefix);
extern inline bool inet_isnetwork6 (
  struct in6_addr network, int prefix);
extern inline bool inet_innetwork4 (
  struct in_addr addr, struct in_addr network, int prefix);
extern inline bool inet_innetwork6 (
  struct in6_addr addr, struct in6_addr network, int prefix);


bool inet_isnetwork (int af, const void *network, int prefix) {
  switch (af) {
    case AF_INET:
      return inet_isnetwork4(*(const struct in_addr *) network, prefix);
    case AF_INET6:
      return inet_isnetwork6(*(const struct in6_addr *) network, prefix);
    default:
      errno = EAFNOSUPPORT;
      return false;
  }
}


bool inet_innetwork (
    int af, const void *addr, const void *network, int prefix) {
  switch (af) {
    case AF_INET:
      return inet_innetwork4(
        *(const struct in_addr *) addr, *(const struct in_addr *) network,
        prefix);
    case AF_INET6:
      return inet_innetwork6(
        *(const struct in6_addr *) addr, *(const struct in6_addr *) network,
        prefix);
    default:
      errno = EAFNOSUPPORT;
      return false;
  }
}
