#ifndef INET46I_IN46_H
#define INET46I_IN46_H

#include <errno.h>
#include <stddef.h>
#include <net/if.h>
#include <netinet/in.h>

#include "endian.h"


#define IN6_SET_ADDR_V4MAPPED(a) ( \
  ((struct in6_addr *) (a))->s6_addr32[0] = 0, \
  ((struct in6_addr *) (a))->s6_addr32[1] = 0, \
  ((struct in6_addr *) (a))->s6_addr32[2] = lhtonl(0xffff) \
)
#define IN6_IS_ADDR_V4OK(a) ( \
  ((struct in6_addr *) (a))->s6_addr32[0] == 0 && \
  ((struct in6_addr *) (a))->s6_addr32[1] == 0 && \
  (((struct in6_addr *) (a))->s6_addr32[2] == 0 || \
   ((struct in6_addr *) (a))->s6_addr32[2] == lhtonl(0xffff)) \
)

__attribute__((nonnull))
inline int in6_normalize (struct in6_addr *addr) {
  if (IN6_IS_ADDR_V4COMPAT(addr)) {
    addr->s6_addr32[2] = lhtonl(0xffff);
  }
  if (IN6_IS_ADDR_V4MAPPED(addr)) {
    if (addr->s6_addr32[3] == lhtonl(0)) {
      addr->s6_addr32[2] = 0;
      return 1;
    }
    if ((addr->s6_addr32[3] & lhtonl(IN_CLASSA_NET)) ==
          lhtonl(INADDR_LOOPBACK & IN_CLASSA_NET)) {
      addr->s6_addr32[2] = 0;
      addr->s6_addr32[3] = lhtonl(1);
      return 1;
    }
  }
  return 0;
}


union in46_addr {
  struct in_addr v4;
  struct in6_addr v6;
};

__attribute__((nonnull))
inline int in_to6 (void *addr) {
  ((struct in6_addr *) addr)->s6_addr32[3] = ((struct in_addr *) addr)->s_addr;
  IN6_SET_ADDR_V4MAPPED(addr);
  return 0;
}

__attribute__((nonnull))
inline int in6_to4 (void *addr) {
  if (IN6_IS_ADDR_LOOPBACK(addr)) {
    ((struct in_addr *) addr)->s_addr = lhtonl(INADDR_LOOPBACK);
    return 0;
  }
  if (IN6_IS_ADDR_V4OK(addr)) {
    ((struct in_addr *) addr)->s_addr =
      ((struct in6_addr *) addr)->s6_addr32[3];
    return 0;
  }
  return 1;
}

__attribute__((nonnull))
inline int in46_is_addr_unspecified (int af, void *addr) {
  return af == AF_INET ?
    ((struct in_addr *) addr)->s_addr == 0 : IN6_IS_ADDR_UNSPECIFIED(addr);
}


struct in46i_addr {
  union {
    struct in_addr v4;
    struct in6_addr v6;
    uint32_t scope_id;
  };
  sa_family_t sa_family;
};

#define IN46I_UNSPEC {.sa_family = AF_UNSPEC}
#define IN46I_IS_UNSPEC(addr) ((addr)->sa_family == AF_UNSPEC)


struct in64_addr {
  union {
    struct in6_addr addr6;
    struct {
      uint32_t __u32_addr_padding[3];
      struct in_addr addr;
    };
  };
  uint32_t scope_id;
};

#define IN64_IS_ADDR_UNSPECIFIED(a) ( \
  IN6_IS_ADDR_V4OK(a) && ((struct in6_addr *) (a))->s6_addr32[3] == 0)

__attribute__((nonnull, access(write_only, 2), access(read_only, 3)))
inline int in64_set (int af, void *dst, const void *src) {
  switch (af) {
    case AF_INET:
      ((struct in64_addr *) dst)->addr.s_addr = *(in_addr_t *) src;
      IN6_SET_ADDR_V4MAPPED(dst);
      break;
    case AF_INET6: {
      struct in6_addr *dst_ = dst;
      const struct in6_addr *src_ = src;
      dst_->s6_addr32[0] = src_->s6_addr32[0];
      dst_->s6_addr32[1] = src_->s6_addr32[1];
      dst_->s6_addr32[2] = src_->s6_addr32[2];
      dst_->s6_addr32[3] = src_->s6_addr32[3];
      break;
    }
    default:
      errno = EAFNOSUPPORT;
  }
  return 0;
}

__attribute__((nonnull, access(read_only, 2)))
inline void *in64_get (int af, const void *addr) {
  return af == AF_INET ?
    (void *) &((struct in64_addr *) addr)->addr : (void *) addr;
}


#endif /* INET46I_IN46_H */
