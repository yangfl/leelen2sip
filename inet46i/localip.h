#ifndef INET46I_LOCALIP_H
#define INET46I_LOCALIP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Detect local IP address.
 */


__attribute__((nonnull, access(write_only, 2)))
/**
 * @brief Detect IP address on interface.
 *
 * @param af Address family.
 * @param[out] addr Interface address.
 * @param ifindex Interface index. Can be 0.
 * @return 0 on success, -1 on error.
 */
int detect_iface_ip (int af, void * __restrict addr, unsigned int ifindex);
__attribute__((nonnull, access(write_only, 2), access(read_only, 3)))
/**
 * @brief Detect source IP address for specific destination.
 *
 * @param af Address family.
 * @param[out] addr Source address.
 * @param dst Destination address.
 * @param ifindex Interface index. Can be 0.
 * @return 0 on success, -1 on error.
 */
int detect_src_ip (
  int af, void * __restrict addr, const void * __restrict dst,
  unsigned int ifindex);
__attribute__((nonnull(2), access(write_only, 2), access(read_only, 3)))
/**
 * @brief Detect local IP address.
 *
 * @param af Address family.
 * @param[out] addr Local address.
 * @param dst Destination address. Can be @c NULL.
 * @param ifindex Interface index. Can be 0.
 * @return 0 on success, -1 on error.
 */
int detect_local_ip (
  int af, void * __restrict addr, const void * __restrict dst,
  unsigned int ifindex);


#ifdef __cplusplus
}
#endif

#endif /* INET46I_LOCALIP_H */
