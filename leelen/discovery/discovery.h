#ifndef LEELEN_DISCOVERY_DISCOVERY_H
#define LEELEN_DISCOVERY_DISCOVERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <threads.h>

#include <inet46i/in46.h>

#include "utils/single.h"
// #include "../number.h"
struct LeelenNumber;
#include "advertiser.h"
// #include "host.h"
struct LeelenHost;


/**
 * @ingroup leelen-discovery
 * @extends LeelenAdvertiser
 * @brief LEELEN peer discovery daemon.
 */
struct LeelenDiscovery {
  struct LeelenAdvertiser;
  /** @privatesection */
  /// daemon thread state
  single_flag state;

  /** @publicsection */
  /// address family of LeelenDiscovery::spec_sockfd
  unsigned char spec_af;
  /// socket for sending discovery packets
  int spec_sockfd;
  /// IPv4 socket
  int sockfd;
  /// IPv6 socket
  int sockfd6;

  /** @privatesection */
  /// mutex for peer discovery
  mtx_t mutex;
  /// condition when advertisement is received
  cnd_t cond;
  /// discovery result
  struct LeelenHost *host;
  /// return value of LeelenHost_init()
  int initres;
};

__attribute__((nonnull, access(write_only, 2), access(read_only, 3)))
/**
 * @memberof LeelenDiscovery
 * @brief Do a peer discovery.
 *
 * @param self Discovery daemon.
 * @param[out] host Host object.
 * @param phone Peer phone number.
 * @return 0 on success, 254 if timeout reached, 255 if discovery daemon not set
 *  up, -1 if LeelenHost__discovery() error.
 */
int LeelenDiscovery_discovery (
  struct LeelenDiscovery *self, struct LeelenHost *host, const char *phone);

__attribute__((nonnull))
/**
 * @memberof LeelenDiscovery
 * @brief Start discovery daemon in current thread.
 *
 * @param self Discovery daemon.
 * @return 0 on success, 255 if socket not set up, -1 if daemon already
 *  running in background.
 */
int LeelenDiscovery_run (struct LeelenDiscovery *self);
__attribute__((nonnull))
/**
 * @memberof LeelenDiscovery
 * @brief Start discovery daemon.
 *
 * @param self Discovery daemon.
 * @return 0 on success, 255 if socket not set up, -1 if @c thrd_create() error.
 */
int LeelenDiscovery_start (struct LeelenDiscovery *self);

__attribute__((nonnull, access(write_only, 1)))
/**
 * @memberof LeelenDiscovery
 * @brief Stop discovery daemon.
 *
 * This function does not wait for the thread to stop.
 *
 * @param self Discovery daemon.
 */
static inline void LeelenDiscovery_stop (struct LeelenDiscovery *self) {
  single_stop(&self->state);
}

__attribute__((nonnull))
/**
 * @memberof LeelenDiscovery
 * @brief Set up sockets.
 *
 * @param self Discovery daemon.
 * @return 0 on success, -1 on error and @c errno is set appropriately.
 */
int LeelenDiscovery_connect (struct LeelenDiscovery *self);

__attribute__((nonnull))
/**
 * @memberof LeelenDiscovery
 * @brief Destroy a discovery daemon object.
 *
 * This function does wait for the thread to stop.
 *
 * @param self Discovery daemon.
 */
void LeelenDiscovery_destroy (struct LeelenDiscovery *self);
__attribute__((nonnull(1), access(write_only, 1), access(read_only, 2)))
/**
 * @memberof LeelenDiscovery
 * @brief Initialize a discovery daemon object.
 *
 * @param[out] self Discovery daemon.
 * @param config Device config.
 * @return 0 on success, -1 on error and @c errno is set appropriately.
 */
int LeelenDiscovery_init (
  struct LeelenDiscovery *self, const struct LeelenConfig *config);


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_DISCOVERY_DISCOVERY_H */
