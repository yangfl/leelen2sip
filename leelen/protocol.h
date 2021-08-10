#ifndef LEELEN_PROTOCOL_H
#define LEELEN_PROTOCOL_H

#include <netinet/in.h>


#define LEELEN_NAME "LEELEN"

enum LeelenDeviceType {
  LEELEN_DEVICE_INDOOR_STATION = 1,
  LEELEN_DEVICE_DOOR_PHONE = 4,
  LEELEN_DEVICE_GUARD_STATION = 16,
};

enum LeelenMessageType {
  LEELEN_UNSPECIFIED_MESSAGE = 0,
  LEELEN_DEVICE_DISCOVERY,
  LEELEN_DEVICE_SOLICITATION,
};

// little endian
enum LeelenCode {
  LEELEN_CODE_CALL = 0x10,
  LEELEN_CODE_VIEW = 0x20,
  LEELEN_CODE_OK = 0x110,
  LEELEN_CODE_BYE = 0x214,
  LEELEN_CODE_OPEN_GATE = 0x216,
  LEELEN_CODE_VOICE_MESSAGE = 0x217,
};

#define LEELEN_DISCOVERY_PORT 6789
#define LEELEN_SIP_PORT 5060
#define LEELEN_AUDIO_PORT 7078
#define LEELEN_VIDEO_PORT 9078
#define LEELEN_CONRTOL_PORT 17722

#define LEELEN_MTU 1500
#define LEELEN_MAX_MESSAGE_LENGTH (LEELEN_MTU - 20 - 8)
#define LEELEN_MAX_PHONE_LENGTH 32

#define LEELEN_DISCOVERY_GROUPADDR INADDR_ALLHOSTS_GROUP  // 224.0.0.1
#define LEELEN_DISCOVERY_FORMAT "%s?%d*%s"
#define LEELEN_DISCOVERY_TIMEOUT (200 * 1000)

extern const struct in_addr leelen_discovery_groupaddr;


#endif /* LEELEN_PROTOCOL_H */