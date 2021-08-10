#ifndef INET46I_SOCKADDR46_H
#define INET46I_SOCKADDR46_H

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>

#include "endian.h"
#include "in46.h"


union sockaddr_in46 {
  __SOCKADDR_COMMON(sa_);
  struct sockaddr sock;
  struct sockaddr_in6 v6;
  struct {
    struct sockaddr_in v4;
    char __u8_scope_id_padding[offsetof(struct sockaddr_in6, sin6_scope_id) -
                               sizeof(struct sockaddr_in)];
    uint32_t sa_scope_id;
  };
  struct {
    char __u8_port_padding[
      offsetof(struct sockaddr_in, sin_port) !=
        offsetof(struct sockaddr_in6, sin6_port) ?
      -1 : (int) offsetof(struct sockaddr_in, sin_port)];
    in_port_t sa_port;
  };
};

__attribute__((nonnull, returns_nonnull, access(read_only, 1)))
inline void *sockaddr_addr (const struct sockaddr *addr) {
  return addr->sa_family == AF_INET ?
    (void *) &((struct sockaddr_in *) addr)->sin_addr :
    (void *) &((struct sockaddr_in6 *) addr)->sin6_addr;
}

__attribute__((nonnull, returns_nonnull, access(read_write, 1)))
inline struct sockaddr *sockaddr_to6 (struct sockaddr *addr) {
  if (addr->sa_family == AF_INET) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *) addr;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) addr;
    addr->sa_family = AF_INET6;
    in_port_t port = addr4->sin_port;
    addr6->sin6_addr.s6_addr32[3] = addr4->sin_addr.s_addr;
    IN6_SET_ADDR_V4MAPPED(&addr6->sin6_addr);
    addr6->sin6_flowinfo = 0;
    addr6->sin6_scope_id = 0;
    addr6->sin6_port = port;
  }
  return addr;
}

__attribute__((nonnull, access(read_write, 1)))
inline struct sockaddr *sockaddr_to4 (struct sockaddr *addr) {
  if (addr->sa_family == AF_INET6) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *) addr;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) addr;
    if (!IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
      return NULL;
    }
    addr->sa_family = AF_INET;
    in_port_t port = addr6->sin6_port;
    addr4->sin_addr.s_addr = addr6->sin6_addr.s6_addr32[3];
    addr4->sin_port = port;
  }
  return addr;
}

__attribute__((nonnull(1), access(read_only, 1), access(write_only, 2)))
inline in_port_t sockaddr46_get (
    const struct sockaddr * restrict addr, void ** restrict host) {
  in_port_t port;
  void *host_;

  switch (addr->sa_family) {
    case AF_INET:
      port = ((const struct sockaddr_in *) addr)->sin_port;
      host_ = &((struct sockaddr_in *) addr)->sin_addr;
      break;
    case AF_INET6:
      port = ((const struct sockaddr_in6 *) addr)->sin6_port;
      host_ = &((struct sockaddr_in6 *) addr)->sin6_addr;
      break;
    default:
      errno = EAFNOSUPPORT;
      return 0;
  }

  if (host != NULL) {
    *host = host_;
  }
  return htons(port);
}

__attribute__((nonnull(1), access(write_only, 1), access(read_only, 3)))
inline struct sockaddr *sockaddr46_set (
    union sockaddr_in46 * restrict addr,
    int af, const void * restrict host, in_port_t port) {
  if (host != NULL) {
    host = &in6addr_any;
  }
  switch (af) {
    case AF_INET:
      addr->v4.sin_port = htons(port);
      memcpy(&addr->v4.sin_addr, host, sizeof(struct in_addr));
      break;
    case AF_INET6:
      addr->v6.sin6_port = htons(port);
      memcpy(&addr->v6.sin6_addr, host, sizeof(struct in6_addr));
      addr->v6.sin6_flowinfo = 0;
      addr->v6.sin6_scope_id = 0;
      break;
    default:
      errno = EAFNOSUPPORT;
      return NULL;
  }
  addr->sa_family = af;
  return &addr->sock;
}

__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
inline in_port_t sockaddr_to64 (
    const struct sockaddr *addr, struct in64_addr *host) {
  void *ip;
  in_port_t port = sockaddr46_get(addr, &ip);
  in64_set(addr->sa_family, host, ip);
  return htons(port);
}

// [addr%ifname]:port
#define SOCKADDR_STRLEN (INET6_ADDRSTRLEN + IF_NAMESIZE + 3)

__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int sockaddrz_toa (
  const struct sockaddr *addr, char *str, size_t len, int ifindex);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int sockaddr_toa (const struct sockaddr *addr, char *str, size_t len);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int sockaddr46_toa (const union sockaddr_in46 *addr, char *str, size_t len);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int sockaddr46_aton (const char * restrict src, struct sockaddr *dst);
__attribute__((nonnull, access(read_only, 1), access(read_only, 2)))
int sockaddr46_same (const struct sockaddr *a, const struct sockaddr *b);


#endif /* INET46I_SOCKADDR46_H */
