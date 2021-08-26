#ifndef LEELEN_VOIP_MESSAGE_H
#define LEELEN_VOIP_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>

// #include "../config.h"
struct LeelenConfig;
#include "protocol.h"


/**
 * @ingroup leelen-voip
 * @brief VoIP message.
 */
struct LeelenMessage {
  /// dialog ID, see LeelenDialog::id
  leelen_id_t id;
  /// message code, in host byte order, see LeelenCode
  leelen_code_t code;

  /// phone number of source device
  struct LeelenNumber from;
  /// device type of source device
  unsigned char from_type;
  /// phone number of destination device
  struct LeelenNumber to;

  /// audio descriptor, if any
  char **audio_formats;
  /// video descriptor, if any
  char **video_formats;
  /// audio port, if any
  in_port_t audio_port;
  /// video port, if any
  in_port_t video_port;
};

__attribute__((nonnull, access(read_only, 1), access(write_only, 2, 3)))
/**
 * @memberof LeelenMessage
 * @brief Convert message object to string, ready to be sent.
 *
 * @param self Message.
 * @param[out] buf Buffer.
 * @param len Length of the buffer.
 * @return The number of characters that would have been written if buffer had
 *  been sufficiently large, not counting the terminating null character.
 */
unsigned int LeelenMessage_tostring (
  const struct LeelenMessage *self, char *buf, unsigned int len);

__attribute__((nonnull, access(write_only, 1), access(read_only, 2)))
/**
 * @brief Fill fields with device config.
 *
 * @param[out] self Message.
 * @param config Device config.
 */
void LeelenMessage_copy_config (
  struct LeelenMessage *self, const struct LeelenConfig *config);

__attribute__((nonnull))
/**
 * @memberof LeelenMessage
 * @brief Destroy a VoIP message.
 *
 * @param self Message.
 */
void LeelenMessage_destroy (struct LeelenMessage *self);

__attribute__((nonnull, access(write_only, 1), access(read_only, 2)))
/**
 * @memberof LeelenMessage
 * @brief Initialize a VoIP reply message.
 *
 * @param[out] self Message.
 * @param request Request Message.
 * @param from_type Device type of source device.
 * @return 0.
 */
int LeelenMessage_init_reply (
  struct LeelenMessage *self, const struct LeelenMessage *request,
  int from_type);
__attribute__((warn_unused_result, nonnull, access(write_only, 1)))
/**
 * @memberof LeelenMessage
 * @brief Parse and initialize a VoIP reply message.
 *
 * @param[out] self Message.
 * @param[in,out] msg Raw message.
 * @param no_audio_formats If @c true, discard @p self->audio_formats.
 * @param no_video_formats If @c true, discard @p self->video_formats.
 * @return 0 on success, -1 if out of memory.
 */
int LeelenMessage_init (
  struct LeelenMessage *self, char *msg,
  bool no_audio_formats, bool no_video_formats);


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_VOIP_MESSAGE_H */
