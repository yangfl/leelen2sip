#ifndef LEELEN_DISCOVERY_HOST_H
#define LEELEN_DISCOVERY_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <netinet/in.h>

#include <inet46i/in46.h>


/**
 * @ingroup leelen-discovery
 * @extends sockaddr_in46
 * @brief LEELEN peer host.
 */
struct LeelenHost {
  union sockaddr_in46;
  /// address of this device that received the discovery solicitation
  union sockaddr_in46 src;
  /// device description
  char *desc;
  /// device type, see @link LeelenDeviceType @endlink
  unsigned char type;
};

__attribute__((nonnull))
/**
 * @memberof LeelenHost
 * @brief Destroy a host object.
 *
 * @param self Host object.
 */
static inline void LeelenHost_destroy (struct LeelenHost *self) {
  free(self->desc);
}

__attribute__((nonnull(1, 2), access(write_only, 1),
               access(read_only, 2), access(read_only, 3)))
/**
 * @memberof LeelenHost
 * @brief Initialize a host object.
 *
 * @param[out] self Host object.
 * @param report Solicitation message.
 * @param src Address of this device that received the discovery solicitation.
 *  Can be @c NULL.
 * @return 0 on success. If 1 is set, parsing the host address failed. If 2 is
 *  set, parsing host type failed. If 4 is set, copying host description failed.
 */
int LeelenHost_init (
  struct LeelenHost *self, const char *report, const struct sockaddr *src);

__attribute__((nonnull, access(read_only, 4)))
/**
 * @relates LeelenHost
 * @brief Send discovery solicitations.
 *
 * @param af Address family. Can be `AF_INET` or `AF_INET6`.
 * @param sockfd Socket to send the solicitation on.
 * @param port Port to send discovery to. If 0, @link LEELEN_DISCOVERY_PORT
 *  @endlink is used.
 * @param phone Peer phone number.
 * @return 0 on success, 255 if address family is invalid, -1 if @c sendto()
 *  error.
 */
int LeelenHost__discovery (
  int af, int sockfd, in_port_t port, const char *phone);
__attribute__((nonnull(1), access(write_only, 1), access(write_only, 4)))
/**
 * @memberof LeelenHost
 * @brief Receive soliciation message, and initialize a host object.
 *
 * @note This function is not thread-safe.
 *
 * @param[out] self Host object.
 * @param sockfd Socket to receive solications on.
 * @param timeout Timeout in milliseconds. If 0, @link LEELEN_DISCOVERY_TIMEOUT
 *  @endlink is used. If negative, no timeout is used.
 * @param[out] initerr Error code during initialization. Can be NULL.
 * @return 0 on succeeding message reception, 1 if reception failed, 2 if
 *  timeout reached.
 * @note For error during initialization, use `initerr`.
 */
int LeelenHost_init_recv (
  struct LeelenHost *self, int sockfd, int timeout, int *initerr);
__attribute__((nonnull(1, 5), access(write_only, 1), access(read_only, 5),
               access(write_only, 7)))
/**
 * @memberof LeelenHost
 * @brief Discover, and initialize a host object.
 *
 * @note This function is not thread-safe.
 *
 * @param[out] self Host object.
 * @param af Address family of `sockfd`.
 * @param sockfd Socket for discovery.
 * @param port Port to send discovery to. If 0, @link LEELEN_DISCOVERY_PORT
 *  @endlink is used.
 * @param phone Peer phone number.
 * @param timeout Timeout in milliseconds. If 0, @link LEELEN_DISCOVERY_TIMEOUT
 *  @endlink is used. If negative, no timeout is used.
 * @param[out] initerr Error code during initialization. Can be NULL.
 * @return 0 on succeeding peer discovery, 255 if address family is invalid, -1
 *  if @c sendto() error, 1 if reception failed, 2 if timeout reached.
 * @note For error during initialization, use `initerr`.
 */
int LeelenHost_init_discovery (
  struct LeelenHost *self, int af, int sockfd, in_port_t port,
  const char *phone, int timeout, int *initerr);


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_DISCOVERY_HOST_H */
