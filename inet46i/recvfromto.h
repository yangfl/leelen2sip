#ifndef INET46I_RECVFROMTO_H
#define INET46I_RECVFROMTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>

#include "sockaddr46.h"

/**
 * @file
 * Extend @c recvfrom().
 */


__attribute__((
  nonnull(2), access(write_only, 2, 3), access(write_only, 5),
  access(read_write, 6), access(write_only, 7), access(write_only, 8)))
/**
 * @brief Receive a message from a socket, and its source and destination
 *  address.
 *
 * @param sockfd Socket to receive on.
 * @param[out] buf Data buffer.
 * @param len Length of the data buffer.
 * @param flags Flags for @c recvfrom().
 * @param[out] src_addr Source address of the packet. Can be @c NULL.
 * @param[in,out] addrlen Length of buffer of @p src_addr. Can be @c NULL.
 * @param[out] dst_addr Header destination address. Can be @c NULL.
 * @param[out] recv_dst Local address that received the packet. Can be @c NULL.
 *  Ignored when address family is IPv6.
 * @return Number of bytes received, or -1 if an error occurred.
 */
ssize_t recvfromto(
  int sockfd, void * __restrict buf, size_t len, int flags,
  struct sockaddr * __restrict src_addr, socklen_t * __restrict addrlen,
  union sockaddr_in46 * __restrict dst_addr,
  struct in_addr * __restrict recv_dst);


#ifdef __cplusplus
}
#endif

#endif /* INET46I_RECVFROMTO_H */
