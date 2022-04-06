#ifndef SIPLEELEN_UAC_H
#define SIPLEELEN_UAC_H

#ifdef __cplusplus
extern "C" {
#endif

// #include <sys/socket.h>
struct sockaddr;

// #include "session.h"
struct SIPLeelenSession;
// #include "sipleelen.h"
struct SIPLeelen;


__attribute__((nonnull, access(read_only, 4)))
/**
 * @memberof SIPLeelenSession
 * @brief Process incoming LEELEN message.
 *
 * @param self LEELEN2SIP session.
 * @param msg Message.
 * @param sockfd Socket that received the message.
 * @param src Source address of the message.
 * @return 0 on success, otherwise error.
 */
int SIPLeelenSession_receive (
  struct SIPLeelenSession *self, char *msg, int sockfd,
  const struct sockaddr *src);

__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Add callbacks for SIP transaction processing.
 *
 * @param self LEELEN2SIP object.
 * @return 0.
 */
int SIPLeelen_set_uac_callbacks (struct SIPLeelen *self);


#ifdef __cplusplus
}
#endif

#endif /* SIPLEELEN_UAC_H */
