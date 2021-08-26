#ifndef LEELEN_VOIP_PROTOCOL_H
#define LEELEN_VOIP_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "../family.h"


/**
 * @ingroup leelen-voip
 * @brief VoIP timeout, in milliseconds
 */
#define LEELEN_VOIP_TIMEOUT 500

/**
 * @ingroup leelen-voip
 * @brief VoIP message code.
 * @note Transmitted in little-endian.
 */
enum LeelenCode {
  /// invalid message code
  LEELEN_CODE_INVALID       = 0x000,
  /// start a new call invitation
  LEELEN_CODE_CALL          = 0x001,
  /// start a new video-only call invitation
  LEELEN_CODE_VIEW          = 0x002,
  /// ack last message
  LEELEN_CODE_OK            = 0x110,
  /// end a call
  LEELEN_CODE_BYE           = 0x214,
  /// accept a call invitation
  LEELEN_CODE_ACCEPTED      = 0x215,
  /// unlock gate
  LEELEN_CODE_OPEN_GATE     = 0x216,
  /// leave a voice message
  LEELEN_CODE_VOICE_MESSAGE = 0x217,
};

/**
 * @ingroup leelen-voip
 * @brief type capable of holding a VoIP message code
 */
typedef uint32_t leelen_code_t;
/**
 * @ingroup leelen-voip
 * @brief format string for @c printf() of ::leelen_code_t
 */
#define PRI_LEELEN_CODE "%03" PRIx32
/**
 * @ingroup leelen-voip
 * @brief type capable of holding a VoIP dialog id
 */
typedef uint32_t leelen_id_t;
/**
 * @ingroup leelen-voip
 * @brief format string for @c printf() of ::leelen_id_t
 */
#define PRI_LEELEN_ID "%08" PRIx32

/**
 * @relates LeelenMessage
 * @brief Get the message code from a raw message.
 *
 * @param s Message.
 * @return Message code.
 * @see LeelenCode
 * @note Always use `le32toh()` and `htole32()` to convert between host
 *  endianness and transmitted endianness.
 */
#define LEELEN_MESSAGE_CODE(s) (*((leelen_code_t *) (s)))
/**
 * @relates LeelenMessage
 * @brief Get the dialog ID from a raw message.
 *
 * @param s Message.
 * @return Dialog ID.
 * @see LeelenMessage::id
 */
#define LEELEN_MESSAGE_ID(s) (*((leelen_id_t *) ((char *) (s) + 4)))

#define LEELEN_MESSAGE_HEADER_SIZE 8
#define LEELEN_MESSAGE_MIN_SIZE ( \
  LEELEN_MESSAGE_HEADER_SIZE + sizeof("From=?\nTo=\n") + \
  2 * (LEELEN_NUMBER_STRLEN - 1) + 3)

/**
 * @relates LeelenMessage
 * @brief Test if message looks like a LEELEN message.
 *
 * @param str Message.
 * @param len Length of the message.
 * @return @c true if the message looks like a LEELEN message.
 */
#define LEELEN_MESSAGE_ISVALID(str, len) ( \
  (len) >= LEELEN_MESSAGE_MIN_SIZE && \
  memcmp((str) + LEELEN_MESSAGE_HEADER_SIZE, "From=", 5) == 0)


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_VOIP_PROTOCOL_H */
