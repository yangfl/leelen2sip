#ifndef LEELEN_CONFIG_H
#define LEELEN_CONFIG_H

#include <stddef.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "protocol.h"


extern const char * const leelen_protocol_name[3];


struct LeelenConfig {
  char *desc;
  unsigned char type;
  in_port_t audio;
  in_port_t video;

  union {
    struct {
      in_port_t discovery;
      in_port_t sip;
      in_port_t control;
    };
    in_port_t ports[3];
  };
};

__attribute__((nonnull))
inline int LeelenConfig_init_default (struct LeelenConfig *self) {
  self->desc = NULL;
  self->type = LEELEN_DEVICE_INDOOR_STATION;
  self->audio = LEELEN_AUDIO_PORT;
  self->video = LEELEN_VIDEO_PORT;
  self->discovery = LEELEN_DISCOVERY_PORT;
  self->sip = LEELEN_SIP_PORT;
  self->control = LEELEN_CONRTOL_PORT;
  return 0;
}

__attribute__((nonnull))
inline void LeelenConfig_destroy (struct LeelenConfig *self) {
  free(self->desc);
}


#endif /* LEELEN_CONFIG_H */
