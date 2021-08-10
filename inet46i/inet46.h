#ifndef INET46I_INET46_H
#define INET46I_INET46_H

#include <string.h>
#include <net/if.h>
#include <netinet/in.h>

#include "in46.h"


__attribute__((nonnull, access(write_only, 1), access(read_only, 2)))
inline char *ifncpy (char * restrict dst, const char * restrict src) {
  memcpy(dst, src, IFNAMSIZ);
  dst[IFNAMSIZ - 1] = '\0';
  return dst;
}

__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int inet_ifton (const char * restrict ifname, struct in_addr * restrict dst);
__attribute__((nonnull(1), access(read_only, 1), access(write_only, 2),
               access(write_only, 3), access(write_only, 4)))
int inet_atonz_p (
  const char *src, struct in_addr *dst4, struct in6_addr *dst6,
  uint32_t *scope_id);
__attribute__((nonnull(2, 3), access(read_only, 2),
               access(write_only, 3), access(write_only, 4)))
int inet_ptonz (
  int af, const char * restrict src, void * restrict dst, uint32_t *scope_id);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int inet_aton46 (const char * restrict src, void * restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int inet_aton46i (const char * restrict src, void * restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int inet_aton64 (const char * restrict src, void * restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int inet_atonz64 (const char * restrict src, struct in64_addr * restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
int inet_atonz64i (const char * restrict src, struct in64_addr * restrict dst);


#endif /* INET46I_INET46_H */
