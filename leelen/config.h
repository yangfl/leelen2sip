#ifndef LEELEN_CONFIG_H
#define LEELEN_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <netinet/in.h>

#include <inet46i/sockaddr46.h>

#include "number.h"


/**
 * @ingroup leelen
 * @brief LEELEN device type.
 */
enum LeelenDeviceType {
  /// invalid device type
  LEELEN_DEVICE_UNKNOWN     = 0,
  /// E60/E75
  LEELEN_DEVICE_BASIC       = 1 << 0,
  /// E26
  LEELEN_DEVICE_THIN        = 1 << 1,
  /// 10型
  LEELEN_DEVICE_DOORWAY_3_5 = 1 << 2,
  /// 1型
  LEELEN_DEVICE_DOORWAY_10  = 1 << 3,
  /// 管理机
  LEELEN_DEVICE_MANCENTER   = 1 << 4,
  /// 8型
  LEELEN_DEVICE_DOORWAY_8   = 1 << 5,
  LEELEN_DEVICE_IP_SWITCH   = 1 << 6,
  /// 4型
  LEELEN_DEVICE_DOORWAY_4   = 1 << 7,
  /// 16型
  LEELEN_DEVICE_DOORWAY_16  = 1 << 8,
};

#define LEELEN_DEVICE_IS_VALID(x) (__builtin_popcount(x) == 1)

/// zhuji
#define LEELEN_DEVICE_IS_DOORWAY(x) (!!((x) & ( \
  LEELEN_DEVICE_DOORWAY_3_5 | LEELEN_DEVICE_DOORWAY_10 | \
  LEELEN_DEVICE_DOORWAY_8 | LEELEN_DEVICE_DOORWAY_4 | LEELEN_DEVICE_DOORWAY_16 \
)))

#define LEELEN_DEVICE_IS_INDOOR(x) (!!((x) & ( \
  LEELEN_DEVICE_BASIC | LEELEN_DEVICE_THIN)))


/**
 * @ingroup leelen
 * @brief LEELEN device config.
 */
struct LeelenConfig {
  /// address for outgoing data
  /// @note `addr.sa_port == 0`
  union sockaddr_in46 addr;
  /// device description, {HardwareType}-{DevInfo}-{HardVersion}-{SoftVersion}
  char *desc;
  /// phone number of this device (optional)
  struct LeelenNumber number;
  /// device type, see LeelenDeviceType
  unsigned char type;

  struct {
    /// port for device discovery
    in_port_t discovery;
    /// port for LEELEN VoIP
    in_port_t voip;
    /// port for device control and metadata exchange
    in_port_t control;
  };

  union {
    struct {
      /// port for sending device discovery
      in_port_t discovery_src;
      /// port for sending LEELEN VoIP
      in_port_t voip_src;
      /// port for sending device control and metadata exchange
      in_port_t control_src;
    };
    /// ports that controller needs to listen on
    in_port_t ports[3];
  };

  /// maximum transmission unit for UDP packet
  unsigned short mtu;
  /// timeout for device discovery, in milliseconds
  int timeout;

  /// port for VoIP audio stream
  in_port_t audio;
  /// port for VoIP video stream
  in_port_t video;
};

__attribute__((nonnull))
/**
 * @memberof LeelenConfig
 * @brief Destroy a config object.
 *
 * @param self Config.
 */
static inline void LeelenConfig_destroy (struct LeelenConfig *self) {
  free(self->desc);
}

__attribute__((nonnull))
/**
 * @memberof LeelenConfig
 * @brief Set default values for config object.
 *
 * @param[out] self Config.
 * @return 0.
 */
int LeelenConfig_init (struct LeelenConfig *self);


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_CONFIG_H */
