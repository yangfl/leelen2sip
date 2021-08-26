#ifndef INET46I_INET46_H
#define INET46I_INET46_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <net/if.h>
#include <netinet/in.h>

#include "in46.h"

/**
 * @file
 * Convert between binary network forms and presentation forms.
 */


__attribute__((nonnull, access(write_only, 1), access(read_only, 2)))
/**
 * @brief Copy interface name.
 *
 * @param[out] dst Destination buffer.
 * @param src Source buffer.
 * @return @p dst.
 */
inline char *ifncpy (char * __restrict dst, const char * __restrict src) {
  memcpy(dst, src, IFNAMSIZ);
  dst[IFNAMSIZ - 1] = '\0';
  return dst;
}

__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
/**
 * @brief Get IPv4 address of the interface.
 *
 * @param ifname Interface name.
 * @param[out] dst IPv4 address.
 * @return @c AF_INET on success, @c AF_UNSPEC on error.
 */
int inet_ifton (
  const char * __restrict ifname, struct in_addr * __restrict dst);
__attribute__((nonnull(1), access(read_only, 1), access(write_only, 2),
               access(write_only, 3), access(write_only, 4)))
/**
 * @brief Convert URL-like address string to binary form.
 *
 * @note If @c AF_INET returned, @p dst4 is set. If @c AF_INET6 returned,
 *  @p dst6 is set.
 *
 * @param src URL-like address string.
 * @param[out] dst4 IPv4 address.
 * @param[out] dst6 IPv6 address.
 * @param[out] scope_id Interface scope ID.
 * @return Converted address family, @c AF_UNSPEC on error.
 */
int inet_atonz_p (
  const char * __restrict src, struct in_addr *dst4, struct in6_addr *dst6,
  uint32_t * __restrict scope_id);
__attribute__((nonnull(2, 3), access(read_only, 2),
               access(write_only, 3), access(write_only, 4)))
/**
 * @brief Convert URL-like address string to binary form.
 *
 * @param af Address family.
 * @param src URL-like address string.
 * @param[out] dst Converted address.
 * @param[out] scope_id Interface scope ID.
 * @return 1 on on success, 0 on error.
 */
int inet_ptonz (
  int af, const char * __restrict src, void * __restrict dst,
  uint32_t * __restrict scope_id);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
/**
 * @brief Convert from presentation form to binary network form, with address
 *  family detection.
 *
 * @param src Address in resentation form.
 * @param[out] dst Converted address.
 * @return Address family of the converted address, @c AF_UNSPEC on error.
 */
int inet_aton46 (const char * __restrict src, void * __restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
/**
 * @brief Convert from presentation form to binary network form, or from
 *  interface name to interface scope ID.
 *
 * @param src Address in resentation form, or interface name.
 * @param[out] dst Converted address or interface scope ID.
 * @return Address family of the converted address, @c AF_LOCAL if interface
 *  scope ID returned, @c AF_UNSPEC on error.
 */
int inet_aton46i (const char * __restrict src, void * __restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
/**
 * @brief Convert from presentation form to binary network form, with address
 *  family detection. IPv4 address is returned in IPv6 IPv4-mapped address.
 *
 * @param src Address in resentation form.
 * @param[out] dst Converted address.
 * @return Address family of the converted address, @c AF_UNSPEC on error.
 */
int inet_aton64 (const char * __restrict src, void * __restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
/**
 * @memberof in64_addr
 * @brief Convert URL-like address string to binary form.
 *
 * @param src URL-like address string.
 * @param[out] dst Converted address.
 * @return Address family of the converted address, @c AF_UNSPEC on error.
 */
int inet_atonz64 (
  const char * __restrict src, struct in64_addr * __restrict dst);
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
/**
 * @memberof in64_addr
 * @brief Convert URL-like address string to binary form, or from
 *  interface name to interface scope ID.
 *
 * @param src URL-like address string.
 * @param[out] dst Converted address.
 * @return Address family of the converted address, @c AF_LOCAL if interface
 *  scope ID returned, @c AF_UNSPEC on error.
 */
int inet_atonz64i (
  const char * __restrict src, struct in64_addr * __restrict dst);


#ifdef __cplusplus
}
#endif

#endif /* INET46I_INET46_H */
