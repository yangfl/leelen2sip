#ifndef INET46I_ROUTE_H
#define INET46I_ROUTE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <netinet/in.h>

/**
 * @file
 * Functions for manipulating IPv4 and IPv6 routes.
 */


__attribute__((const, warn_unused_result))
/**
 * @brief Convert an IPv4 CIDR prefix length to a netmask.
 *
 * If -1 is passed, the netmask will be set to all `1`s (`255.255.255.255`).
 *
 * @param prefix Prefix length.
 * @return Netmask.
 */
inline in_addr_t inet_tomask (int prefix) {
  return prefix < 0 ? 0xffffffff : htonl(~((1u << (32 - prefix)) - 1u));
}


__attribute__((const, warn_unused_result))
/**
 * @brief Test if an IPv4 address is a valid CIDR prefix.
 *
 * @param network IPv4 address.
 * @param prefix Prefix length.
 * @return @c true if the address is a valid CIDR prefix.
 */
inline bool inet_isnetwork4 (struct in_addr network, int prefix) {
  return (network.s_addr & ~inet_tomask(prefix)) == 0;
}

__attribute__((const, warn_unused_result))
/**
 * @brief Test if an IPv6 address is a valid CIDR prefix.
 *
 * @param network IPv6 address.
 * @param prefix Prefix length.
 * @return @c true if the address is a valid CIDR prefix.
 */
inline bool inet_isnetwork6 (struct in6_addr network, int prefix) {
  int prefix32 = 4;
  int prefix32rem = 0;
  if (prefix >= 0) {
    prefix32 = prefix / 32;
    prefix32rem = prefix % 32;
  }
  for (int i = 3; i >= prefix32 + !!prefix32rem; i--) {
    if (network.s6_addr32[i] != 0) {
      return false;
    }
  }
  return prefix32rem == 0 || inet_isnetwork4(
    *(struct in_addr *) &network.s6_addr32[prefix32], prefix32rem);
}

__attribute__((pure, warn_unused_result, access(read_only, 2)))
/**
 * @brief Test if an address is a valid CIDR prefix.
 *
 * @param af Address family. Can be @c AF_INET or @c AF_INET6.
 * @param network Address.
 * @param prefix Prefix length.
 * @return @c true if the address is a valid CIDR prefix.
 */
bool inet_isnetwork (int af, const void *network, int prefix);


__attribute__((const, warn_unused_result))
/**
 * @brief Test if an IPv4 address is in a network.
 *
 * @param addr IPv4 address.
 * @param network IPv4 network.
 * @param prefix Prefix length.
 * @return @c true if the address is in the network.
 */
inline bool inet_innetwork4 (
    struct in_addr addr, struct in_addr network, int prefix) {
  return prefix == 0 ||
    (addr.s_addr & inet_tomask(prefix)) ==
      (network.s_addr & inet_tomask(prefix));
}

__attribute__((const, warn_unused_result))
/**
 * @brief Test if an IPv6 address is in a network.
 *
 * @param addr IPv6 address.
 * @param network IPv6 network.
 * @param prefix Prefix length.
 * @return @c true if the address is in the network.
 */
inline bool inet_innetwork6 (
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
      return false;
    }
  }
  return prefix32rem == 0 || inet_innetwork4(
    *(struct in_addr *) &addr.s6_addr32[prefix32],
    *(struct in_addr *) &network.s6_addr32[prefix32], prefix32rem);
}

__attribute__((pure, warn_unused_result,
               access(read_only, 2), access(read_only, 3)))
/**
 * @brief Test if an address is in a network.
 *
 * @param af Address family. Can be @c AF_INET or @c AF_INET6.
 * @param addr Address.
 * @param network Network.
 * @param prefix Prefix length.
 * @return @c true if the address is in the network.
 */
bool inet_innetwork (
  int af, const void *addr, const void *network, int prefix);


#ifdef __cplusplus
}
#endif

#endif /* INET46I_ROUTE_H */
