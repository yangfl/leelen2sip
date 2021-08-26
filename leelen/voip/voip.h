#ifndef LEELEN_VOIP_VOIP_H
#define LEELEN_VOIP_VOIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

// #include "../config.h"
struct LeelenConfig;
// #include "../number.h"
struct LeelenNumber;
// #include "dialog.h"
struct LeelenDialog;
#include "protocol.h"

/**
 * @ingroup leelen
 * @defgroup leelen-voip VoIP
 * LEELEN VoIP functions.
 */


/**
 * @ingroup leelen-voip
 * @brief LEELEN VoIP controller.
 */
struct LeelenVoIP {
  /// VoIP dialogs
  struct LeelenDialog **dialogs;
  /// length of LeelenVoIP::dialogs
  int n_dialog;
  /// lock for LeelenVoIP::dialogs
  pthread_rwlock_t lock;
  /// device config
  const struct LeelenConfig *config;
};

__attribute__((nonnull(1, 2), access(write_only, 4), access(write_only, 5),
               access(read_only, 6)))
/**
 * @memberof LeelenVoIP
 * @brief Process an incoming VoIP message according to dialog ID.
 *
 * @param self Controller.
 * @param msg VoIP message.
 * @param sockfd Socket to reply with.
 * @param[out] audio_formats Parsed audio description.
 * @param[out] video_formats Parsed video description.
 * @param src Source address. Used to create new dialog when no dialog matches.
 *  Can be @c NULL.
 * @return 0 on success, -1 if out of memory.
 */
int LeelenVoIP_receive (
  struct LeelenVoIP *self, char *msg, int sockfd,
  char ***audio_formats, char ***video_formats, const struct sockaddr *src);
__attribute__((nonnull(1, 2), access(read_only, 2), access(read_only, 3)))
/**
 * @memberof LeelenVoIP
 * @brief Create a new VoIP dialog object.
 *
 * @note This function does not really establish a VoIP dialog with the peer,
 *  just create an object to manage it.
 *
 * @param self Controller.
 * @param dst Peer address.
 * @param to Peer phone number. If @c NULL, the phone number is waiting to be
 *  filled by LeelenDialog_receive().
 * @param id Dialog ID. If 0, a random ID will be generated.
 * @return New dialog object, or @c NULL if out of memory.
 */
struct LeelenDialog *LeelenVoIP_connect (
  struct LeelenVoIP *self, const struct sockaddr *dst,
  const struct LeelenNumber *to, leelen_id_t id);

__attribute__((nonnull))
/**
 * @memberof LeelenVoIP
 * @brief Destroy a VoIP controller.
 *
 * @note This function does not check whether dialogs are terminated.
 *
 * @param self Controller.
 */
void LeelenVoIP_destroy (struct LeelenVoIP *self);

__attribute__((nonnull, access(write_only, 1)))
/**
 * @memberof LeelenVoIP
 * @brief Initialize a VoIP controller.
 *
 * @param[out] self Controller.
 * @param config Device config.
 * @return 0 on success, -1 if `pthread_rwlock_init()` error.
 */
static inline int LeelenVoIP_init (
    struct LeelenVoIP *self, const struct LeelenConfig *config) {
  if (pthread_rwlock_init(&self->lock, NULL) != 0) {
    return -1;
  }
  self->dialogs = NULL;
  self->n_dialog = 0;
  self->config = config;
  return 0;
}


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_VOIP_VOIP_H */
