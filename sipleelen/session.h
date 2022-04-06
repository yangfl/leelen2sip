#ifndef SIPLEELEN_SESSION_H
#define SIPLEELEN_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <netinet/in.h>
#include <sys/time.h>  // osip

#include <inet46i/in46.h>
#include <osip2/osip.h>
#include <osip2/osip_dialog.h>

#include "utils/refcount.h"
#include "utils/single.h"
#include "leelen/number.h"
#include "leelen/voip/dialog.h"
#include "forwarder.h"
// #include "sipleelen.h"
struct SIPLeelen;


/**
 * @ingroup sip
 * @brief LEELEN2SIP SIP session.
 */
struct SIPLeelenSession {
  /// device object
  const struct SIPLeelen *device;

  /// LEELEN dialog
  struct LeelenDialog leelen;
  /// SIP dialog
  osip_dialog_t *sip;
  /// SIP transaction, borrowed
  osip_transaction_t *transaction;

  /// reference counter
  refcount_t refcount;

  /// local address that received the SIP request
  union in46_addr ours;

  /** @privatesection */
  /// audio forwarder (usually of port #LEELEN_AUDIO_PORT)
  struct Forwarder audio;
  /// video forwarder (usually of port #LEELEN_VIDEO_PORT)
  struct Forwarder video;

  /// mutex
  mtx_t mtx;

  /// invite thread state
  single_flag invite_state;
  /// LEELEEN phone number (for SIP INVITE)
  struct LeelenNumber number;
};

__attribute__((warn_unused_result, nonnull))
/**
 * @memberof SIPLeelenSession
 * @brief Test if session is established.
 *
 * @param self LEELEN2SIP session.
 * @return @c true if established.
 */
bool SIPLeelenSession_established (struct SIPLeelenSession *self);

__attribute__((nonnull))
/**
 * @memberof SIPLeelenSession
 * @brief Start audio/video forwarder threads.
 *
 * @param self LEELEN2SIP session.
 * @return 0 on success, -1 on error.
 */
static inline int SIPLeelenSession_start_forward (
    struct SIPLeelenSession *self) {
  return
    Forwarder_start(&self->audio) == 0 && Forwarder_start(&self->video) == 0 ?
    0 : -1;
}

__attribute__((nonnull, access(write_only, 1)))
/**
 * @memberof SIPLeelenSession
 * @brief Stop audio/video forwarder threads.
 *
 * This function does not wait for threads to stop.
 *
 * @param self LEELEN2SIP session.
 */
static inline void SIPLeelenSession_stop_forward (
    struct SIPLeelenSession *self) {
  Forwarder_stop(&self->audio);
  Forwarder_stop(&self->video);
}

__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Disconnect session on LEELEN half.
 *
 * @param self LEELEN2SIP object.
 * @return 0 on success, -1 on error and @c errno is set appropriately.
 */
int SIPLeelenSession_bye_leelen (struct SIPLeelenSession *self);
__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Disconnect session on SIP half.
 *
 * @param self LEELEN2SIP object.
 * @param now Whether to send SIP transaction now.
 * @return 0 on success, 255 if @p self->sip is @c NULL, -1 if SIP stack error.
 */
int SIPLeelenSession_bye_sip (struct SIPLeelenSession *self, bool now);
__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Disconnect session on LEELEN half.
 *
 * @param self LEELEN2SIP object.
 * @param now Whether to send SIP transaction now.
 * @return 0 on success; if 1 set, error on sending LEELEN BYE; if 2 set, error
 *  on creating SIP transaction.
 */
int SIPLeelenSession_bye (struct SIPLeelenSession *self, bool now);

__attribute__((nonnull(1)))
/**
 * @memberof SIPLeelen
 * @brief Set up sockets.
 *
 * @param self LEELEN2SIP object.
 * @param audio If not @c NULL, set up audio socket and save SIP port number.
 * @param video If not @c NULL, set up video socket and save SIP port number.
 * @return 0 on success, 1 if open LEELEN audio socket failed, 2 if open SIP
 *  audio socket failed, 3 if open LEELEN video socket failed, 4 if open SIP
 *  video socket failed, and on error @c errno is set appropriately.
 */
int SIPLeelenSession_connect (
  struct SIPLeelenSession *self, in_port_t *audio, in_port_t *video);

__attribute__((nonnull))
/**
 * @memberof SIPLeelenSession
 * @brief Destroy a LEELEN2SIP session.
 *
 * This function does wait for the threads to stop.
 *
 * @param self LEELEN2SIP session.
 */
void SIPLeelenSession_destroy (struct SIPLeelenSession *self);
__attribute__((nonnull, access(write_only, 1), access(read_only, 2)))
/**
 * @memberof SIPLeelenSession
 * @brief Initialize a LEELEN2SIP session.
 *
 * @param[out] self LEELEN2SIP session.
 * @param device Device object.
 * @param mtu Maximum transmission unit for UDP packet.
 * @return 0 on success, -1 if `mtx_init()` error.
 */
int SIPLeelenSession_init (
  struct SIPLeelenSession *self, const struct SIPLeelen *device);

__attribute__((nonnull))
/**
 * @memberof SIPLeelenSession
 * @brief Free a LEELEN2SIP session when necessary.
 *
 * This function does wait for the threads to stop.
 *
 * @param self LEELEN2SIP session.
 * @return @c true if session was destroyed.
 */
bool SIPLeelenSession_may_free (struct SIPLeelenSession *self);
__attribute__((nonnull))
/**
 * @memberof SIPLeelenSession
 * @brief Decrease reference counter of a LEELEN2SIP session and destroy it when
 *  necessary.
 *
 * @param self LEELEN2SIP session.
 * @return @c true if session was destroyed.
 */
bool SIPLeelenSession_decref (struct SIPLeelenSession *self);

__attribute__((nonnull))
/**
 * @memberof SIPLeelenSession
 * @brief Increase reference counter of a LEELEN2SIP session.
 *
 * @param self LEELEN2SIP session.
 */
static inline void SIPLeelenSession_incref (struct SIPLeelenSession *self) {
  refcount_inc(&self->refcount, self);
}


#ifdef __cplusplus
}
#endif

#endif /* SIPLEELEN_SESSION_H */
