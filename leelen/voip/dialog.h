#ifndef LEELEN_VOIP_DIALOG_H
#define LEELEN_VOIP_DIALOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <threads.h>
#include <time.h>
#include <netinet/in.h>

#include <inet46i/sockaddr46.h>

// #include "../config.h"
struct LeelenConfig;
#include "../number.h"
// #include "message.h"
struct LeelenMessage;
#include "protocol.h"


/**
 * @ingroup leelen-voip
 * @brief VoIP dialog state.
 */
enum LeelenDialogState {
  /// dialog is not yet established
  LEELEN_DIALOG_DISCONNECTED = 0,
  /// ACCEPT sent; waiting for ACK
  LEELEN_DIALOG_CONNECTING,
  /// dialog is established
  LEELEN_DIALOG_CONNECTED,
  /// BYE sent; waiting for ACK
  LEELEN_DIALOG_DISCONNECTING,
};


/**
 * @ingroup leelen-voip
 * @brief LEELEN VoIP dialog.
 */
struct LeelenDialog {
  /// dialog ID; if 0, the object is invalid
  leelen_id_t id;

  /// phone number of this device
  struct LeelenNumber from;
  /// device type of this device
  unsigned char from_type;
  /**
   * @brief phone number of peer device
   *
   * This field can actually be different than the real peer phone number, since
   * seems LEELEN does not check the phone number in VoIP messages.
   */
  struct LeelenNumber to;
  /// device type of peer device
  unsigned char to_type;

  /// address of this device, for external use
  union sockaddr_in46 ours;
  /// address of peer device
  /// @note `theirs.port == config->voip`
  union sockaddr_in46 theirs;
  /// audio port of this device
  in_port_t our_audio_port;
  /// video port of this device
  in_port_t our_video_port;
  /// audio port of peer device
  in_port_t their_audio_port;
  /// video port of peer device
  in_port_t their_video_port;
  /// maximum length of the message
  unsigned short mtu;

  /** @privatesection */

  /// dialog state, see #LeelenDialogState
  unsigned char state;
  /// last sent time
  struct timespec last_sent;

  /** @publicsection */

  /// user data
  void *userdata;
};

__attribute__((nonnull))
/**
 * @memberof LeelenDialog
 * @brief Check if peer does not reply the message within timeout.
 *
 * @param self Dialog.
 * @return @c true if timeout reached.
 */
bool LeelenDialog_check_timeout (struct LeelenDialog *self);

__attribute__((nonnull))
/**
 * @memberof LeelenDialog
 * @brief Send raw message to peer device and update dialog state. The dialog ID
 *  in the message is not checked.
 *
 * @param self Dialog.
 * @param buf Raw message.
 * @param len Length of the message.
 * @param sockfd Socket for sending reply.
 * @return 0 on success, -1 if @c sendto() error.
 */
int LeelenDialog_sendmsg (
  struct LeelenDialog *self, void *buf, unsigned int len, int sockfd);

__attribute__((nonnull))
/**
 * @memberof LeelenDialog
 * @brief Send specified code to peer device.
 *
 * @param self Dialog.
 * @param code VoIP code.
 * @param sockfd Socket for sending reply.
 * @return 0 on success, -1 if @c sendto() error.
 */
int LeelenDialog_sendcode (
  struct LeelenDialog *self, unsigned int code, int sockfd);
__attribute__((nonnull))
/**
 * @memberof LeelenDialog
 * @brief Send #LEELEN_CODE_BYE to peer device.
 *
 * @param self Dialog.
 * @param sockfd Socket for sending reply.
 * @return 0 on success, 1 if dialog is not connected, -1 if @c sendto() error.
 */
int LeelenDialog_may_bye (struct LeelenDialog *self, int sockfd);
__attribute__((nonnull))
/**
 * @memberof LeelenDialog
 * @brief Ack last message (send code @link LEELEN_CODE_OK @endlink).
 *
 * @param self Dialog.
 * @param sockfd Socket for sending reply.
 * @return 0 on success, -1 if @c sendto() error.
 */
int LeelenDialog_ack (struct LeelenDialog *self, int sockfd);

__attribute__((nonnull(1), access(read_only, 4), access(read_only, 5)))
/**
 * @memberof LeelenDialog
 * @brief Send message to peer device and update dialog state.
 *
 * @param self Dialog.
 * @param code VoIP code.
 * @param sockfd Socket for sending reply.
 * @param audio_formats Audio description. Can be @c NULL.
 * @param video_formats Video description. Can be @c NULL.
 * @return 0 on success, -1 if @c sendto() error.
 */
int LeelenDialog_send (
  struct LeelenDialog *self, unsigned int code, int sockfd,
  char * const *audio_formats, char * const *video_formats);
__attribute__((nonnull(1, 2), access(write_only, 4), access(write_only, 5)))
/**
 * @memberof LeelenDialog
 * @brief Process an incoming VoIP message and update the dialog state.
 *
 * @param self Dialog.
 * @param[in,out] msg Raw message.
 * @param sockfd Socket for sending reply.
 * @param[out] audio_formats Parsed audio description. Can be @c NULL.
 * @param[out] video_formats Parsed video description. Can be @c NULL.
 * @return 0 on success, 255 if dialog ID mismatched, 254 if ACK timeout, -1 on
 *  error and @c errno is set appropriately.
 */
int LeelenDialog_receive (
  struct LeelenDialog *self, char *msg, int sockfd,
  char ***audio_formats, char ***video_formats);

__attribute__((nonnull))
/**
 * @memberof LeelenDialog
 * @brief Destroy a VoIP dialog.
 *
 * @note This function does not check whether the dialog is terminated.
 *
 * @param self Dialog.
 */
static inline void LeelenDialog_destroy (struct LeelenDialog *self) {
  self->id = 0;
}

__attribute__((nonnull(1, 2, 3), access(write_only, 1), access(read_only, 2),
               access(read_only, 3), access(read_only, 4)))
/**
 * @memberof LeelenDialog
 * @brief Initialize a VoIP dialog.
 *
 * @param[out] self Dialog.
 * @param config Device config.
 * @param theirs Peer address.
 * @param to Peer phone number. If @c NULL, the phone number is waiting to be
 *  filled by LeelenDialog_receive().
 * @param id Dialog ID. If 0, a random ID will be generated.
 * @return 0.
 */
int LeelenDialog_init (
  struct LeelenDialog *self, const struct LeelenConfig *config,
  const struct sockaddr *theirs, const struct LeelenNumber *to, leelen_id_t id);


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_VOIP_DIALOG_H */
