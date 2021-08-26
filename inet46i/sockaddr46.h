#ifndef INET46I_SOCKADDR46_H
#define INET46I_SOCKADDR46_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>

#include "in46.h"

/**
 * @file
 * Structures and functions for IPv4 and IPv6 addresses with port number.
 */


/// An IPv4 or IPv6 address, with port number, with interface scope ID.
union sockaddr_in46 {
#ifdef DOXYGEN
  /// address family
  sa_family_t sa_family;
#endif

  /// @cond GARBAGE
  __SOCKADDR_COMMON(sa_);
  /// @endcond
  struct {
    /// @cond GARBAGE
    char __u8_port_padding[offsetof(struct sockaddr_in, sin_port)];
    /// @endcond
    /// port number
    in_port_t sa_port;
    /// @cond GARBAGE
    char __u8_scope_id_padding[
      offsetof(struct sockaddr_in6, sin6_scope_id) -
      offsetof(struct sockaddr_in6, sin6_port)];
    /// @endcond
    /// interface scope ID, or 0 if do not bind to a specific interface
    uint32_t sa_scope_id;
  };
  struct sockaddr sock;
  struct sockaddr_in v4;
  struct sockaddr_in6 v6;
};

_Static_assert(
  offsetof(struct sockaddr_in, sin_port) ==
    offsetof(struct sockaddr_in6, sin6_port),
  "sin_port does not agree with sin6_port");

__attribute__((pure, returns_nonnull, nonnull, access(read_only, 1)))
/**
 * @memberof sockaddr_in46
 * @brief Get host address.
 *
 * @param addr Socket address.
 * @return Host address.
 */
inline void *sockaddr_addr (const struct sockaddr * __restrict addr) {
  return addr->sa_family == AF_INET ?
    (void *) &((struct sockaddr_in *) addr)->sin_addr :
    (void *) &((struct sockaddr_in6 *) addr)->sin6_addr;
}

__attribute__((nonnull(1), access(read_only, 1), access(write_only, 2)))
/**
 * @memberof sockaddr_in46
 * @brief Get host address and port number.
 *
 * @param addr Socket address.
 * @param[out] host Host address. Can be @c NULL.
 * @return Port number.
 */
inline in_port_t sockaddr_get (
    const struct sockaddr * __restrict addr, void * __restrict host) {
  in_port_t port;
  const void *host_;

  switch (addr->sa_family) {
    case AF_INET: {
      const struct sockaddr_in *addr4 = (const struct sockaddr_in *) addr;
      port = addr4->sin_port;
      host_ = &addr4->sin_addr;
      break;
    }
    case AF_INET6: {
      const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *) addr;
      port = addr6->sin6_port;
      host_ = &addr6->sin6_addr;
      break;
    }
    default:
      errno = EAFNOSUPPORT;
      return 0;
  }

  if (host != NULL) {
    *(const void **) host = host_;
  }
  return ntohs(port);
}

__attribute__((nonnull(1), access(read_only, 1), access(write_only, 2)))
/**
 * @memberof sockaddr_in46
 * @brief Get host address in IPv6 address, and port number.
 *
 * @param addr Socket address.
 * @param[out] host Host address. Can be @c NULL.
 * @return Port number.
 */
inline in_port_t sockaddr_get64 (
    const struct sockaddr * __restrict addr,
    struct in64_addr * __restrict host) {
  void *ip = NULL;
  in_port_t port = sockaddr_get(addr, &ip);
  if (ip != NULL && host != NULL) {
    in64_set(addr->sa_family, host, ip);
  }
  return port;
}

__attribute__((nonnull, access(read_only, 1), access(read_only, 2)))
/**
 * @memberof sockaddr_in46
 * @brief Test if two socket addresses are same.
 *
 * @param a Socket address.
 * @param b Socket address.
 * @return @c true if same.
 */
inline bool sockaddr_same (const struct sockaddr *a, const struct sockaddr *b) {
  void *aaddr;
  void *baddr;
  return sockaddr_get(a, &aaddr) == sockaddr_get(b, &baddr) && (
    a->sa_family == AF_INET ?
      memcmp(aaddr, baddr, sizeof(struct in_addr)) :
      memcmp(aaddr, baddr, sizeof(struct in6_addr))) == 0;
}

__attribute__((returns_nonnull, nonnull, access(read_write, 1)))
/**
 * @memberof sockaddr_in46
 * @brief Convert IPv4 to IPv6 socket address in-place.
 *
 * @param[in,out] addr Socket address.
 * @return @p addr.
 */
inline struct sockaddr *sockaddr_to6 (struct sockaddr * __restrict addr) {
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
/**
 * @memberof sockaddr_in46
 * @brief Convert IPv6 to IPv4 socket address in-place.
 *
 * @param[in,out] addr Socket address.
 * @return @p addr on success, or @c NULL if @p addr can not be safely
 *  converted.
 */
inline struct sockaddr *sockaddr_to4 (struct sockaddr * __restrict addr) {
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

__attribute__((nonnull(1), access(write_only, 1), access(read_only, 3)))
/**
 * @memberof sockaddr_in46
 * @brief Set host address and port number.
 *
 * @param[out] addr Socket address.
 * @param af Address family. Can be @c AF_INET or @c AF_INET6.
 * @param host Host address. If is @c NULL, unspecified address is used.
 * @param port Port number.
 * @return @p addr on success, or @c NULL if address family is not supported.
 */
inline struct sockaddr *sockaddr46_set (
    union sockaddr_in46 * __restrict addr, int af, const void * __restrict host,
    in_port_t port) {
  if (host == NULL) {
    host = &in6addr_any;
  }
  switch (af) {
    case AF_INET:
      addr->v4.sin_port = htons(port);
      addr->v4.sin_addr = *(struct in_addr *) host;
      break;
    case AF_INET6:
      addr->v6.sin6_port = htons(port);
      addr->v6.sin6_addr = *(struct in6_addr *) host;
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

// (INET6_ADDRSTRLEN - 1 + IF_NAMESIZE - 1 + 4 + 5 + 1)
/**
 * @relates sockaddr_in46
 * @brief Minimum safe length of buffer capable of holding URL-like address
 *  string, in format `[addr%ifname]:port`.
 */
#define SOCKADDR_STRLEN (INET6_ADDRSTRLEN + IF_NAMESIZE + 8)

__attribute__((nonnull, access(read_only, 1), access(write_only, 2, 3)))
/**
 * @memberof sockaddr_in46
 * @brief Convert socket address to URL-like address string.
 *
 * @param addr Socket address.
 * @param[out] str Buffer to store string. Suggested size SOCKADDR_STRLEN().
 * @param len Length of buffer.
 * @param ifindex Interface scope ID, or 0 if no interface specified.
 * @return 0.
 */
int sockaddrz_toa (
  const struct sockaddr * __restrict addr,
  char * __restrict str, size_t len, int ifindex);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2, 3)))
/**
 * @memberof sockaddr_in46
 * @brief Convert socket address to URL-like address string.
 *
 * @note If @p addr->sa_family is @c AF_INET6, will output interface specified
 *  by @p addr->sin6_scope_id.
 *
 * @param addr Socket address.
 * @param[out] str Buffer to store string. Suggested size SOCKADDR_STRLEN().
 * @param len Length of buffer.
 * @return 0.
 */
int sockaddr_toa (
  const struct sockaddr * __restrict addr, char * __restrict str, size_t len);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2, 3)))
/**
 * @memberof sockaddr_in46
 * @brief Convert socket address to URL-like address string, and output
 *  interface specified by @p addr->sa_scope_id.
 *
 * @param addr Socket address.
 * @param[out] str Buffer to store string. Suggested size SOCKADDR_STRLEN().
 * @param len Length of buffer.
 * @return 0.
 */
int sockaddr46_toa (
  const union sockaddr_in46 * __restrict addr,
  char * __restrict str, size_t len);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
/**
 * @memberof sockaddr_in46
 * @brief Convert URL-like address string to socket address.
 *
 * If only interface name found, @p dst->sa_family is set to @c AF_INET6.
 *
 * @param src URL-like address string.
 * @param[out] dst Socket address.
 * @return Address family of the converted address, @c AF_LOCAL if only
 *  interface name found, @c AF_UNSPEC on error.
 */
int sockaddr46_aton (
  const char * __restrict src, union sockaddr_in46 * __restrict dst);


#ifdef __cplusplus
}
#endif

#endif /* INET46I_SOCKADDR46_H */
