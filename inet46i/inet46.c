#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "private/macro.h"
#include "in46.h"
#include "inet46.h"


extern inline char *ifncpy (char * __restrict dst, const char * __restrict src);


int inet_atonz_p (
    const char * __restrict src, struct in_addr *dst4, struct in6_addr *dst6,
    uint32_t * __restrict scope_id) {
  if (src[0] == '[') {
    src++;
  }

  const char *src_end = src + strlen(src);
  const char *addr_end = strchr(src, '%');
  // quick path, no copy
  if (scope_id != NULL) {
    *scope_id = 0;
  }
  if (src_end[-1] != ']' && addr_end == NULL) {
    if (dst4 != NULL) {
      return_if (inet_pton(AF_INET, src, dst4) == 1) AF_INET;
    }
    if (dst6 != NULL) {
      return_if (inet_pton(AF_INET6, src, dst6) == 1) AF_INET6;
    }
    return AF_UNSPEC;
  }

  // do copy
  if (src_end[-1] == ']') {
    src_end--;
  }
  if (addr_end == NULL) {
    addr_end = src_end;
  } else if (scope_id != NULL) {
    const char *ifname_start = addr_end + 1;
    if (0 < src_end - ifname_start && src_end - ifname_start < IF_NAMESIZE) {
      char ifname[IF_NAMESIZE];
      memcpy(ifname, ifname_start, src_end - ifname_start);
      ifname[src_end - ifname_start] = '\0';
      if ('0' <= ifname[0] && ifname[0] <= '9') {
        char *ifname_end;
        *scope_id = strtol(ifname, &ifname_end, 10);
        if (*ifname_end != '\0') {
          *scope_id = 0;
        }
      } else {
        *scope_id = if_nametoindex(ifname);
      }
    }
  }
  return_if_fail (addr_end - src < INET6_ADDRSTRLEN) 0;
  char src_[INET6_ADDRSTRLEN];
  memcpy(src_, src, addr_end - src);
  src_[addr_end - src] = '\0';
  if (dst4 != NULL) {
    return_if (inet_pton(AF_INET, src_, dst4) == 1) AF_INET;
  }
  if (dst6 != NULL) {
    return_if (inet_pton(AF_INET6, src_, dst6) == 1) AF_INET6;
  }
  return AF_UNSPEC;
}


int inet_ptonz (
    int af, const char * __restrict src, void * __restrict dst,
    uint32_t * __restrict scope_id) {
  switch (af) {
    case AF_INET:
      return_if (inet_atonz_p(src, dst, NULL, scope_id) != 0) 1;
      break;
    case AF_INET6:
      return_if (inet_atonz_p(src, NULL, dst, scope_id) != 0) 1;
      break;
    default:
      errno = EAFNOSUPPORT;
  }
  return 0;
}


int inet_aton46 (const char * __restrict src, void * __restrict dst) {
  if (inet_pton(AF_INET, src, dst) == 1) {
    return AF_INET;
  }
  if (inet_pton(AF_INET6, src, dst) == 1) {
    return AF_INET6;
  }
  return AF_UNSPEC;
}


int inet_aton46i (const char * __restrict src, void * __restrict dst) {
  unsigned int ifindex = if_nametoindex(src);
  if (ifindex != 0) {
    *(unsigned int *) dst = ifindex;
    return AF_LOCAL;
  }
  return inet_aton46(src, dst);
}


int inet_aton64 (const char * __restrict src, void * __restrict dst) {
  if (inet_pton(AF_INET, src, &((struct in64_addr *) dst)->addr) == 1) {
    IN6_SET_ADDR_V4MAPPED(dst);
    return AF_INET;
  }
  if (inet_pton(AF_INET6, src, dst) == 1) {
    return AF_INET6;
  }
  return AF_UNSPEC;
}


int inet_atonz64 (
    const char * __restrict src, struct in64_addr * __restrict dst) {
  int domain = inet_atonz_p(src, &dst->addr, &dst->addr6, &dst->scope_id);
  if (domain == AF_INET) {
    IN6_SET_ADDR_V4MAPPED(dst);
  }
  return domain;
}


int inet_atonz64i (
    const char * __restrict src, struct in64_addr * __restrict dst) {
  dst->scope_id = if_nametoindex(src);
  if (dst->scope_id != 0) {
    memset(&dst->addr6, 0, sizeof(struct in6_addr));
    return AF_LOCAL;
  }
  return inet_atonz64(src, dst);
}
