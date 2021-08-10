#ifndef INET46I_ROUTE_H
#define INET46I_ROUTE_H

#include <netinet/in.h>


__attribute__((const, warn_unused_result))
inline in_addr_t inet_tomask (int prefix) {
  return prefix < 0 ? 0xffffffff : htonl(~((1u << (32 - prefix)) - 1u));
}


__attribute__((const, warn_unused_result))
inline int inet_isnetwork4 (struct in_addr network, int prefix) {
  return (network.s_addr & ~inet_tomask(prefix)) == 0;
}

__attribute__((const, warn_unused_result))
inline int inet_isnetwork6 (struct in6_addr network, int prefix) {
  int prefix32 = 4;
  int prefix32rem = 0;
  if (prefix >= 0) {
    prefix32 = prefix / 32;
    prefix32rem = prefix % 32;
  }
  for (int i = 3; i >= prefix32 + !!prefix32rem; i--) {
    if (network.s6_addr32[i] != 0) {
      return 0;
    }
  }
  return prefix32rem == 0 ? 1 : inet_isnetwork4(
    *(struct in_addr *) &network.s6_addr32[prefix32], prefix32rem);
}

__attribute__((pure, warn_unused_result, access(read_only, 2)))
int inet_isnetwork (int af, const void *network, int prefix);


__attribute__((const, warn_unused_result))
inline int inet_innetwork4 (
    struct in_addr addr, struct in_addr network, int prefix) {
  return prefix == 0 ||
    (addr.s_addr & inet_tomask(prefix)) ==
      (network.s_addr & inet_tomask(prefix));
}

__attribute__((const, warn_unused_result))
inline int inet_innetwork6 (
    struct in6_addr addr, struct in6_addr network, int prefix) {
  if (prefix == 0) {
    return 1;
  }
  int prefix32 = 4;
  int prefix32rem = 0;
  if (prefix >= 0) {
    prefix32 = prefix / 32;
    prefix32rem = prefix % 32;
  }
  for (int i = 3; i >= prefix32 + !!prefix32rem; i--) {
    if (addr.s6_addr32[i] != network.s6_addr32[i]) {
      return 0;
    }
  }
  return prefix32rem == 0 ? 1 : inet_innetwork4(
    *(struct in_addr *) &addr.s6_addr32[prefix32],
    *(struct in_addr *) &network.s6_addr32[prefix32], prefix32rem);
}

__attribute__((pure, warn_unused_result,
               access(read_only, 2), access(read_only, 3)))
int inet_innetwork (
  int af, const void *addr, const void *network, int prefix);


#endif /* INET46I_ROUTE_H */
