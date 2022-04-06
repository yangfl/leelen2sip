#ifndef SIPLEELEN_SIPLEELEN_H
#define SIPLEELEN_SIPLEELEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>  // osip

#include <inet46i/sockaddr46.h>
#include <osip2/osip.h>

#include "utils/single.h"
// #include "leelen/config.h"
struct LeelenConfig;
#include "leelen/discovery/discovery.h"
// #include "session.h"
struct SIPLeelenSession;

/**
 * @ingroup leelen2sip
 * @defgroup sip SIP
 * SIP related functions.
 * @{
 */


#define SIPLEELEN_PORT 5060
#define SIPLEELEN_MTU 1500
#define SIPLEELEN_MAX_MESSAGE_LENGTH (SIPLEELEN_MTU - 20 - 8)
#define SIPLEELEN_EXPIRES 300  // s


/**@}*/

/**
 * @ingroup sip
 * @extends LeelenDiscovery
 * @brief LEELEN2SIP object.
 */
struct SIPLeelen {
  /// LEELEN discovery daemon
  struct LeelenDiscovery leelen;

  /// SIP user agent
  char *ua;
  /// address for SIP outgoing data
  /// @note usually `addr.sa_port == 5060`
  union sockaddr_in46 addr;
  /// maximum transmission unit for UDP packet
  unsigned short mtu;

  /** @privatesection */
  /// OSIP stack
  osip_t *osip;
  /// URL of end user
  char *client;
  /// address of end user (only support single user)
  union sockaddr_in46 clients;

  /// SIP sessions, holding SIPLeelenSession::refcount for LEELEN dialog
  struct SIPLeelenSession **sessions;
  /// mutex for SIPLeelen::sessions
  mtx_t mtx_sessions;
  /// length of SIPLeelen::sessions
  int n_session;

  /// socket for SIP
  int socket_sip;
  /// socket for LEELEN VoIP
  int socket_leelen;

  /// executer thread state
  single_flag state;
};

__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Create a new session in @p self->sessions.
 *
 * @param self LEELEN2SIP object.
 * @param lock =0: Don't perform locking/unlocking; >0: Perform
 *  locking/unlocking; <0: Don't perform locking, but perform unlocking.
 * @return New session, or @c NULL on error.
 */
struct SIPLeelenSession *SIPLeelen_new_session (
  struct SIPLeelen *self, int lock);
__attribute__((nonnull, access(read_only, 2)))
/**
 * @memberof SIPLeelen
 * @brief Decrease reference counter of a session in @p self->sessions and
 *  destroy it when necessary.
 *
 * @param self LEELEN2SIP object.
 * @param session LEELEN2SIP session.
 * @param index Index of @p session in @p self->sessions, or @c -1 if unknown.
 * @param lock =0: Don't perform locking/unlocking; >0: Perform
 *  locking/unlocking; <0: Don't perform locking, but perform unlocking.
 */
void SIPLeelen_decref_session (
  struct SIPLeelen *self, struct SIPLeelenSession *session, int index,
  int lock);

__attribute__((nonnull(1, 2), access(read_only, 5), access(read_only, 6)))
/**
 * @memberof SIPLeelen
 * @brief Process incoming SIP message.
 *
 * @param self LEELEN2SIP object.
 * @param buf Message.
 * @param len Length of message.
 * @param sockfd Socket that received the message.
 * @param src Source address of the message.
 * @param recv_dst Local address that received the packet.
 * @return 0 on success, 1 if message cannot be parese, 2 if error on processing
 *  message.
 */
int SIPLeelen_receive (
  struct SIPLeelen *self, char *buf, int len, int sockfd,
  const struct sockaddr *src, const void *recv_dst);
__attribute__((nonnull(1, 2), access(read_only, 5)))
/**
 * @memberof SIPLeelen
 * @brief Process incoming LEELEN VoIP message.
 *
 * @param self LEELEN2SIP object.
 * @param buf Message.
 * @param len Length of message.
 * @param sockfd Socket that received the message.
 * @param src Source address of the message.
 * @return 0 on success, 255 if message malformed, -1 if out of memory, 1 if
 *  error on processing message.
 */
int SIPLeelen_receive_leelen (
  struct SIPLeelen *self, char *buf, int len, int sockfd,
  const struct sockaddr *src);

__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Start discovery daemon in current thread.
 *
 * @param self Discovery daemon.
 * @return 0 on success, 255 if socket not set up, -1 if daemon already running
 *  in background.
 */
int SIPLeelen_run (struct SIPLeelen *self);
__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Start SIP timer thread.
 *
 * @param self LEELEN2SIP object.
 * @return 0 on success, 255 if socket not set up, -1 if @c thrd_create() error.
 */
int SIPLeelen_start (struct SIPLeelen *self);
__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Stop SIP timer thread.
 *
 * This function does not wait for timer thread to stop.
 *
 * @param self LEELEN2SIP object.
 */
void SIPLeelen_stop (struct SIPLeelen *self);

__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Set up sockets.
 *
 * @param self LEELEN2SIP object.
 * @return 0 on success, 1 if open LEELEN discovery failed, 2 if open LEELEN
 *  VoIP failed, 3 if open LEELEN control failed, 4 if open SIP failed, and on
 *  error @c errno is set appropriately.
 */
int SIPLeelen_connect (struct SIPLeelen *self);

__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Destroy a LEELEN2SIP object.
 *
 * This function does wait for timer thread to stop.
 *
 * @param self LEELEN2SIP object.
 */
void SIPLeelen_destroy (struct SIPLeelen *self);
__attribute__((nonnull, access(read_only, 2)))
/**
 * @memberof SIPLeelen
 * @brief Initialize a LEELEN2SIP object.
 *
 * @param[out] self LEELEN2SIP object.
 * @param config Device config.
 * @return 0 on success, -1 on error and @c errno is set appropriately.
 */
int SIPLeelen_init (struct SIPLeelen *self, const struct LeelenConfig *config);


#ifdef __cplusplus
}
#endif

#endif /* SIPLEELEN_SIPLEELEN_H */
