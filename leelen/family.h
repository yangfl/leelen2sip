#ifndef LEELEN_FAMILY_H
#define LEELEN_FAMILY_H

/**
 * @ingroup leelen2sip
 * @defgroup leelen LEELEN
 * LEELEN related functions.
 * @{
 */


/// protocol family name
#define LEELEN_NAME "LEELEN"

/// default maximum transmission unit for UDP packet
#define LEELEN_MAX_MESSAGE_LENGTH 1200

/// default port for device discovery
#define LEELEN_DISCOVERY_PORT 6789
/// discovery protocol name
#define LEELEN_DISCOVERY_PROTOCOL_NAME LEELEN_NAME "-Discovery"

/// default port for LEELEN VoIP
#define LEELEN_VOIP_PORT 5060
/// LEELEN VoIP protocol name
#define LEELEN_VOIP_PROTOCOL_NAME LEELEN_NAME "-VoIP"

/// default port for device control and metadata exchange
#define LEELEN_CONRTOL_PORT 17722
/// control protocol name
#define LEELEN_CONRTOL_PROTOCOL_NAME LEELEN_NAME "-Control"

/// default port for audio stream
#define LEELEN_AUDIO_PORT 7078
/// default port for video stream
#define LEELEN_VIDEO_PORT 9078

/// minimum saft buffer length capabale to hold a LEELEN phone number
#define LEELEN_NUMBER_STRLEN 12


/**@}*/

#endif /* LEELEN_FAMILY_H */
