#include <stdio.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include "private/macro.h"
#include "in46.h"
#include "inet46.h"
#include "sockaddr46.h"


extern inline void *sockaddr_addr (const struct sockaddr *addr);
extern inline struct sockaddr *sockaddr_to6 (struct sockaddr *addr);
extern inline struct sockaddr *sockaddr_to4 (struct sockaddr *addr);
extern inline in_port_t sockaddr46_get (
  const struct sockaddr * restrict addr,
  void ** restrict host);
extern inline struct sockaddr *sockaddr46_set (
  union sockaddr_in46 * restrict addr, int af,
  const void * restrict host, in_port_t port);
extern inline in_port_t sockaddr_to64 (
  const struct sockaddr *addr, struct in64_addr *host);


int sockaddrz_toa (
    const struct sockaddr *addr, char *str, size_t len, int ifindex) {
  if (addr->sa_family == AF_INET6) {
    str[0] = '[';
    inet_ntop(
      addr->sa_family, &((const struct sockaddr_in6 *) addr)->sin6_addr,
      str + 1, len - 1);
  } else {
    inet_ntop(
      addr->sa_family, &((const struct sockaddr_in *) addr)->sin_addr,
      str, len);
  }

  if (ifindex != 0) {
    char ifname[IF_NAMESIZE + 1];
    ifname[0] = '%';
    if_indextoname(ifindex, ifname + 1);
    strncat(str, ifname, len);
  }
  if (addr->sa_family == AF_INET6) {
    strncat(str, "]", len);
  }

  in_port_t port = ((const union sockaddr_in46 *) addr)->sa_port;
  if (port != 0) {
    char sport[7];
    sport[0] = ':';
    snprintf(sport + 1, sizeof(sport) - 1, "%d", ntohs(port));
    strncat(str, sport, len);
  }
  return 0;
}


int sockaddr_toa (const struct sockaddr *addr, char *str, size_t len) {
  return sockaddrz_toa(addr, str, len,
                       ((const struct sockaddr_in6 *) addr)->sin6_scope_id);
}


int sockaddr46_toa (const union sockaddr_in46 *addr, char *str, size_t len) {
  return sockaddrz_toa(&addr->sock, str, len, addr->sa_scope_id);
}


int sockaddr46_aton (const char * restrict src, struct sockaddr *dst) {
  struct in64_addr host;
  int domain = inet_atonz64i(src, &host);
  return_if_fail (domain != AF_UNSPEC) AF_UNSPEC;

  union sockaddr_in46 *addr = (void *) dst;
  addr->sa_family = domain;
  addr->sa_scope_id = host.scope_id;
  switch (domain) {
    case AF_LOCAL:
      addr->sa_family = AF_INET6;
      memset(&addr->v6.sin6_addr, 0, sizeof(struct in6_addr));
      break;
    case AF_INET:
      memcpy(&addr->v4.sin_addr, &host.addr, sizeof(struct in_addr));
      break;
    case AF_INET6:
      memcpy(&addr->v6.sin6_addr, &host.addr6, sizeof(struct in6_addr));
      addr->v6.sin6_flowinfo = 0;
      break;
  }
  return domain;
}


int sockaddr46_same (const struct sockaddr *a, const struct sockaddr *b) {
  void *aaddr;
  void *baddr;
  in_port_t aport = sockaddr46_get(a, &aaddr);
  in_port_t bport = sockaddr46_get(b, &baddr);
  return_nonzero (cmp(aport, bport));
  return a->sa_family == AF_INET ?
    memcmp(aaddr, baddr, sizeof(struct in_addr)) :
    memcmp(aaddr, baddr, sizeof(struct in6_addr));
}
