#ifndef LEELEN_DISCOVERY_PROTOCOL_H
#define LEELEN_DISCOVERY_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>

/**
 * @ingroup leelen
 * @defgroup leelen-discovery Discovery
 * LEELEN peer discovery.
 * @{
 */


#define LEELEN_DISCOVERY_GROUPADDR INADDR_ALLHOSTS_GROUP  // 224.0.0.1
/// discovery advertisement format
#define LEELEN_DISCOVERY_FORMAT "%s?%d*%s"
/// discovery timeout, in milliseconds
#define LEELEN_DISCOVERY_TIMEOUT 200

/// IPv4 address to send discovery messages on
extern const struct in_addr leelen_discovery_groupaddr;
/// IPv6 address to send discovery messages on
extern const struct in6_addr leelen_discovery_groupaddr6;

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1)))
/**
 * @brief Test if message is a advertisement message.
 *
 * @param msg Message.
 * @return @c true if message is a advertisement message.
 */
static inline bool Leelen_discovery_is_advertisement (const char *msg) {
  return strchr(msg, '?') != NULL;
}

__attribute__((nonnull(1, 2), access(read_only, 1), access(read_only, 2),
               access(read_only, 3)))
/**
 * @brief Log a solicitation/advertisement message.
 *
 * @param msg Message.
 * @param src Source address.
 * @param dst Destination address that received the message. Can be @c NULL.
 * @param ifindex Interface index that received the message.
 * @param should_reply @c true if the message is a solicitation and we should
 *  reply it.
 */
void Leelen_discovery_logger (
  const char *msg, const struct sockaddr *src, const void *dst,
  int ifindex, bool should_reply);


/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* LEELEN_DISCOVERY_PROTOCOL_H */
