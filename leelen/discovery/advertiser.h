#ifndef LEELEN_DISCOVERY_ADVERTISER_H
#define LEELEN_DISCOVERY_ADVERTISER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <regex.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>

#include "../config.h"


/**
 * @ingroup leelen-discovery
 * @brief Handle and reply LEELEN peer solicitations.
 */
struct LeelenAdvertiser {
  /// device config
  const struct LeelenConfig *config;

  /// phone number regex for solicitation matching
  regex_t number_regex;
  /// @c true if LeelenAdvertiser::number_regex is set
  bool number_regex_set;

  /// IP address of the device to be reported
  char report_addr[INET6_ADDRSTRLEN];

  /// maximum transmission unit for UDP packet
  unsigned short mtu;
};

__attribute__((nonnull, access(read_only, 1), access(read_only, 2)))
/**
 * @memberof LeelenAdvertiser
 * @brief Test if a phone number solicitation should be replied.
 *
 * @param self Advertiser instance.
 * @param number Requested phone number.
 * @return @c true if the solicitation should be replied.
 */
bool LeelenAdvertiser_should_reply (
  const struct LeelenAdvertiser *self, const char *number);
__attribute__((nonnull(1, 3), access(read_only, 1), access(read_only, 3)))
/**
 * @memberof LeelenAdvertiser
 * @brief Reply to a peer solicitation.
 *
 * @param self Advertiser instance.
 * @param sockfd Socket file descriptor to send the reply on.
 * @param addr Destination address.
 * @param ouraddr Report address when @p self->report_addr is empty.
 *  Can be @c NULL.
 * @return 0 on success, 255 if our address can not be determined, -1 if
 *  @c sendto() error.
 */
int LeelenAdvertiser_reply (
  const struct LeelenAdvertiser *self, int sockfd,
  const struct sockaddr *addr, const void *ouraddr);
__attribute__((nonnull(1, 2, 4), access(read_only, 1), access(read_only, 2),
               access(read_only, 4), access(read_only, 5)))
/**
 * @memberof LeelenAdvertiser
 * @brief Process a discovery message and reply if appropriate.
 *
 * @param self Advertiser.
 * @param msg The message.
 * @param sockfd Socket file descriptor the message was received on.
 * @param src Source address.
 * @param dst Destination address that received the message.
 * @param ifindex Interface index that received the message.
 * @return 0 on success, 254 if the solicitation should not be replied, 255 if
 *  our address can not be determined, -1 if @c sendto() error.
 */
int LeelenAdvertiser_receive (
  const struct LeelenAdvertiser *self, const char *msg, int sockfd,
  const struct sockaddr *src, const void *dst, int ifindex);

__attribute__((nonnull, access(read_only, 3)))
/**
 * @memberof LeelenAdvertiser
 * @brief Set @p self->report_addr from given IP address.
 *
 * @param[in,out] self Advertiser.
 * @param af The address family.
 * @param addr The IP address.
 * @return 0 on success, 255 if address family is not supported.
 */
int LeelenAdvertiser_set_report_addr (
  struct LeelenAdvertiser *self, int af, const void *addr);
__attribute__((nonnull))
/**
 * @memberof LeelenAdvertiser
 * @brief Sync properties from @p self->config.
 *
 * @param[in,out] self Advertiser.
 * @return 0 on success, error otherwise.
 */
int LeelenAdvertiser_sync (struct LeelenAdvertiser *self);

__attribute__((nonnull))
/**
 * @memberof LeelenAdvertiser
 * @brief Destroy a advertiser object.
 *
 * @param self Advertiser.
 */
static inline void LeelenAdvertiser_destroy (struct LeelenAdvertiser *self) {
  if (self->number_regex_set) {
    regfree(&self->number_regex);
  }
}

__attribute__((nonnull(1), access(write_only, 1), access(read_only, 2)))
/**
 * @memberof LeelenAdvertiser
 * @brief Initialize a advertiser object.
 *
 * @param[out] self Advertiser.
 * @param config Device config.
 * @return 0.
 */
static inline int LeelenAdvertiser_init (
    struct LeelenAdvertiser *self, const struct LeelenConfig *config) {
  self->config = config;
  self->number_regex_set = false;
  LeelenAdvertiser_sync(self);
  return 0;
}


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_DISCOVERY_ADVERTISER_H */
