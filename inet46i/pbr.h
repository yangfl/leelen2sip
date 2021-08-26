#ifndef INET46I_PBR_H
#define INET46I_PBR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <netinet/in.h>

#include "sockaddr46.h"

/**
 * @file
 * Dispatch packets according to source-destination filters.
 */


/// Contains source-destination information.
struct Socket5Tuple {
  /// source address, source port
  union sockaddr_in46 src;
  /// destination address, destination port, receive interface
  union sockaddr_in46 dst;
};


/// Address filter rule.
struct SockaddrPolicy {
  union {
    /// IPv6 network prefix
    struct in6_addr network6;
    struct {
      /// @cond GARBAGE
      uint32_t __u32_network_padding[3];
      /// @endcond
      /// IPv4 network prefix
      struct in_addr network;
    };
  };
  /// receive interface scope ID
  uint32_t scope_id;
  union {
    /// port, valid if SockaddrPolicy::port_max is zero
    in_port_t port;
    /// port lower bound, valid if SockaddrPolicy::port_max is non-zero
    in_port_t port_min;
  };
  /// port upper bound
  in_port_t port_max;
  /// network prefix length, -1 for maximum prefix length (32 for IPv4, 128 for IPv6)
  short prefix;
};

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1),
               access(read_only, 2)))
/**
 * @memberof SockaddrPolicy
 * @brief Test if source socket address matches a policy.
 *
 * @param policy Policy.
 * @param addr Source socket address.
 * @return @c true if matches.
 */
bool sockaddrpr_match (
  const struct SockaddrPolicy * __restrict policy,
  const union sockaddr_in46 * __restrict addr);


/**
 * @memberof SocketPolicyRoute
 * @brief base type of SocketPolicyRoute::func
 */
typedef int (*SocketPolicyRouteFunc) ();
/**
 * @memberof SocketPolicyRoute
 * @brief SocketPolicyRoute::func that processes UDP packets
 */
typedef int (*SocketPolicyRouteStreamFunc) (
  void *context, int sockfd, struct Socket5Tuple *conn);
/**
 * @memberof SocketPolicyRoute
 * @brief SocketPolicyRoute::func that processes TCP packets
 */
typedef int (*SocketPolicyRouteDgramFunc) (
  void *context, void *buf, int buflen, int sockfd,
  struct Socket5Tuple *conn, struct in_addr recv_dst);

/// Packet filter and dispatch rule.
struct SocketPolicyRoute {
  /// source address filter
  struct SockaddrPolicy src;
  /// destination address filter
  struct SockaddrPolicy dst;
  /// dispatch function
  SocketPolicyRouteFunc func;
  /// user data for SocketPolicyRoute::func
  void *context;
};

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1),
               access(read_only, 2)))
/**
 * @relates SocketPolicyRoute
 * @brief Find a matching policy.
 *
 * @param routes Policy array.
 * @param conn Socket 5-Tuple.
 * @return Matching policy, or @c NULL if no match.
 */
struct SocketPolicyRoute *socketpr_resolve (
  const struct SocketPolicyRoute * __restrict routes,
  const struct Socket5Tuple * __restrict conn);


/// Packet filter and dispatch rules for socket.
struct SocketPBR {
  union {
    /// policy array; SocketPBR::single_route must be unset
    const struct SocketPolicyRoute *routes;
    struct {
      /// dispatch function; SocketPBR::single_route must be set
      SocketPolicyRouteFunc func;
      /// user data for SocketPBR::func
      void *context;
    };
  };
  /// if set, SocketPBR::func is valid; otherwise, SocketPBR::routes is valid
  bool single_route;
  /// get and match against destination address information; if not set, SocketPolicyRoute::dst is ignored
  bool dst_required;
  /// default return value when no policy matches
  int nothing_ret;
  union {
    /// pre-set destination address; only valid for TCP
    union sockaddr_in46 dst_addr;
    struct {
      /// buffer length; only valid for UDP
      unsigned short buflen;
      /// read length, must less than or equal to SocketPBR::buflen; only valid for UDP
      unsigned short recvlen;
      /// pre-set destination port; only valid for UDP
      in_port_t dst_port;
    };
  };
};

__attribute__((nonnull, access(read_only, 2)))
/**
 * @memberof SocketPBR
 * @brief Accept a TCP connection and apply routing policy.
 *
 * @param sockfd Socket.
 * @param pbr Routing policy.
 * @param flags Flags for @c accept4().
 * @return Return value from dispatch function, or @p pbr->nothing_ret if no
 *  policy matches.
 */
int socketpbr_accept4 (
  int sockfd, const struct SocketPBR * __restrict pbr, int flags);
__attribute__((nonnull, access(read_only, 2)))
/**
 * @memberof SocketPBR
 * @brief Accept a TCP connection and apply routing policy.
 *
 * @param sockfd Socket.
 * @param pbr Routing policy.
 * @return Return value from dispatch function, or @p pbr->nothing_ret if no
 *  policy matches.
 */
int socketpbr_accept (int sockfd, const struct SocketPBR * __restrict pbr);
__attribute__((nonnull, access(read_only, 2)))
/**
 * @memberof SocketPBR
 * @brief Read date from UDP socket and apply routing policy.
 *
 * @param sockfd Socket.
 * @param pbr Routing policy.
 * @param flags Flags for @c recv().
 * @return Return value from dispatch function, or @p pbr->nothing_ret if no
 *  policy matches.
 */
int socketpbr_recv (
  int sockfd, const struct SocketPBR * __restrict pbr, int flags);
__attribute__((nonnull, access(read_only, 2)))
/**
 * @memberof SocketPBR
 * @brief Read date from UDP socket and apply routing policy.
 *
 * @param sockfd Socket.
 * @param pbr Routing policy.
 * @return Return value from dispatch function, or @p pbr->nothing_ret if no
 *  policy matches.
 */
int socketpbr_read (int sockfd, const struct SocketPBR * __restrict pbr);
__attribute__((nonnull, access(read_only, 2)))
/**
 * @memberof SocketPBR
 * @brief Set up socket for future applying routing policy.
 *
 * @param sockfd Socket.
 * @param pbr Routing policy.
 * @param af Address family. Can be @c AF_INET or @c AF_INET6, or 0 for
 *  auto-detection.
 * @return 0 on success, -1 on error.
 */
int socketpbr_set (int sockfd, const struct SocketPBR * __restrict pbr, int af);


#ifdef __cplusplus
}
#endif

#endif /* INET46I_PBR_H */
