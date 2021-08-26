#ifndef INET46I_IN46_H
#define INET46I_IN46_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <net/if.h>
#include <netinet/in.h>

#include "endian.h"

/**
 * @file
 * Structures and functions for IPv4 and IPv6 addresses.
 */


/// Set IPv4-mapped prefix for IPv6 address.
#define IN6_SET_ADDR_V4MAPPED(a) ( \
  ((struct in6_addr *) (a))->s6_addr32[0] = 0, \
  ((struct in6_addr *) (a))->s6_addr32[1] = 0, \
  ((struct in6_addr *) (a))->s6_addr32[2] = lhtonl(0xffff) \
)
/// Test if IPv6 address can be safely converted to IPv4 address.
#define IN6_IS_ADDR_V4OK(a) ( \
  ((struct in6_addr *) (a))->s6_addr32[0] == 0 && \
  ((struct in6_addr *) (a))->s6_addr32[1] == 0 && \
  (((struct in6_addr *) (a))->s6_addr32[2] == 0 || \
   ((struct in6_addr *) (a))->s6_addr32[2] == lhtonl(0xffff)) \
)

__attribute__((nonnull))
/**
 * @brief Fix some common error with converted IPv6 address.
 *
 * @param addr IPv6 address.
 * @return 1 if address changed, 0 otherwise.
 */
inline int in6_normalize (struct in6_addr *addr) {
  // convert v4-compat to v4-mapped
  if (IN6_IS_ADDR_V4COMPAT(addr)) {
    addr->s6_addr32[2] = lhtonl(0xffff);
    goto fix_mapped;
  }
  if (IN6_IS_ADDR_V4MAPPED(addr)) {
    // convert v4-mapped unspecified address to ::0
    if (addr->s6_addr32[3] == 0) {
      addr->s6_addr32[2] = 0;
      return 1;
    }
fix_mapped:
    // convert v4 loopback to ::1
    if ((addr->s6_addr32[3] & lhtonl(IN_CLASSA_NET)) ==
          lhtonl(INADDR_LOOPBACK & IN_CLASSA_NET)) {
      addr->s6_addr32[2] = 0;
      addr->s6_addr32[3] = lhtonl(1);
      return 1;
    }
  }
  return 0;
}


/// An IPv4 or IPv6 address.
union in46_addr {
  struct in_addr v4;
  struct in6_addr v6;
};


/// An IPv4 or IPv6 address, or an interface.
struct in46i_addr {
  union {
    struct in_addr v4;
    struct in6_addr v6;
    uint32_t scope_id;
  };
  sa_family_t sa_family;
};

/// in46i_addr initializer
#define IN46I_UNSPEC {.sa_family = AF_UNSPEC}
/// Test if address is unspecified.
#define IN46I_IS_UNSPEC(addr) ((addr)->sa_family == AF_UNSPEC)

/// Test if address is unspecified.
#define IN64_IS_ADDR_UNSPECIFIED(a) ( \
  IN6_IS_ADDR_V4OK(a) && ((struct in6_addr *) (a))->s6_addr32[3] == 0)


/// An IPv4 or IPv6 address, with interface specification.
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


__attribute__((nonnull))
/**
 * @memberof in46_addr
 * @brief Convert IPv4 address to IPv6 address in-place. The address is not
 *  normalized.
 *
 * @param[in,out] addr IPv4 address with space capable of holding IPv6 address.
 * @return 0.
 */
inline int in_to6 (void *addr) {
  ((struct in6_addr *) addr)->s6_addr32[3] = ((struct in_addr *) addr)->s_addr;
  IN6_SET_ADDR_V4MAPPED(addr);
  return 0;
}

__attribute__((nonnull))
/**
 * @memberof in46_addr
 * @brief Convert IPv6 address to IPv4 address in-place.
 *
 * @param[in,out] addr IPv6 address.
 * @return 0 if success, 1 if address can not be safely converted.
 */
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

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 2)))
/**
 * @relates in46_addr
 * @brief Test if address is unspecified address.
 *
 * @param af Address family, can be @c AF_INET or @c AF_INET6.
 * @param addr Address.
 * @return @c true if address is unspecified.
 */
inline bool in46_is_addr_unspecified (int af, const void *addr) {
  return af == AF_INET ?
    ((struct in_addr *) addr)->s_addr == 0 : IN6_IS_ADDR_UNSPECIFIED(addr);
}

__attribute__((nonnull, access(write_only, 2), access(read_only, 3)))
/**
 * @brief Convert IPv4/IPv6 address to IPv6 address. The address is not
 *  normalized.
 *
 * @param af Address family of @p src, can be @c AF_INET or @c AF_INET6.
 * @param[out] dst Converted IPv6 address.
 * @param src IPv4/IPv6 address.
 * @return 0 on success, -1 if @p af is invalid.
 */
inline int in64_set (int af, void *dst, const void *src) {
  switch (af) {
    case AF_INET:
      ((struct in64_addr *) dst)->addr.s_addr = *(in_addr_t *) src;
      IN6_SET_ADDR_V4MAPPED(dst);
      break;
    case AF_INET6: {
      struct in6_addr *dst_ = (struct in6_addr *) dst;
      const struct in6_addr *src_ = (const struct in6_addr *) src;
      dst_->s6_addr32[0] = src_->s6_addr32[0];
      dst_->s6_addr32[1] = src_->s6_addr32[1];
      dst_->s6_addr32[2] = src_->s6_addr32[2];
      dst_->s6_addr32[3] = src_->s6_addr32[3];
      break;
    }
    default:
      errno = EAFNOSUPPORT;
      return -1;
  }
  return 0;
}

__attribute__((nonnull, access(read_only, 2)))
/**
 * @brief Get IPv4/IPv6 address from IPv6 address.
 *
 * @param af Address family, can be @c AF_INET or @c AF_INET6.
 * @param addr IPv6 address.
 * @return IPv4/IPv6 address.
 */
inline void *in64_get (int af, const void *addr) {
  return af == AF_INET ?
    (void *) &((struct in64_addr *) addr)->addr : (void *) addr;
}


#ifdef __cplusplus
}
#endif

#endif /* INET46I_IN46_H */
