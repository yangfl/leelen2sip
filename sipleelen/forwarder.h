#ifndef SIPLEELEN_FORWARDER_H
#define SIPLEELEN_FORWARDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/single.h"


/**
 * @ingroup sip
 * @brief Forward one socket to another.
 */
struct Forwarder {
  /** @privatesection */
  /// forwarder thread state
  single_flag state;

  /** @publicsection */
  /// length of read/write buffer (maximum transmission unit for UDP packet)
  int mtu;
  /// socket 1
  int socket1;
  /// socket 2
  int socket2;
};

__attribute__((nonnull))
/**
 * @memberof Forwarder
 * @brief Start forwarder thread.
 *
 * @param self Socket forwarder.
 * @return 0 on success, 255 if no socket not set up, -1 if `thrd_create()`
 *  error.
 */
int Forwarder_start (struct Forwarder *self);

__attribute__((nonnull, access(write_only, 1)))
/**
 * @memberof Forwarder
 * @brief Stop forwarder thread.
 *
 * This function does not wait for the thread to stop.
 *
 * @param self Socket forwarder.
 */
static inline void Forwarder_stop (struct Forwarder *self) {
  single_stop(&self->state);
}

__attribute__((nonnull))
/**
 * @memberof Forwarder
 * @brief Destroy a socket forwarder.
 *
 * This function does wait for the thread to stop.
 *
 * @param self Socket forwarder.
 */
void Forwarder_destroy (struct Forwarder *self);

__attribute__((nonnull, access(write_only, 1)))
/**
 * @memberof Forwarder
 * @brief Initialize a socket forwarder.
 *
 * @param[out] self Socket forwarder.
 * @param mtu Length of read/write buffer.
 * @return 0.
 */
static inline int Forwarder_init (struct Forwarder *self, int mtu) {
  self->state = SINGLE_FLAG_INIT;
  self->mtu = mtu;
  self->socket1 = -1;
  self->socket2 = -1;
  return 0;
}


#ifdef __cplusplus
}
#endif

#endif /* SIPLEELEN_FORWARDER_H */
