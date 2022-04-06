#include <stddef.h>
#include <string.h>

#include "family.h"
#include "discovery/protocol.h"
#include "voip/protocol.h"
#include "config.h"


int LeelenConfig_init (struct LeelenConfig *self) {
  memset(&self->addr, 0, sizeof(self->addr));
  self->desc = NULL;
  self->number.str[0] = '\0';
  self->type = LEELEN_DEVICE_BASIC;

  self->discovery = LEELEN_DISCOVERY_PORT;
  self->voip = LEELEN_VOIP_PORT;
  self->control = LEELEN_CONRTOL_PORT;

  self->discovery_src = LEELEN_DISCOVERY_PORT;
  self->voip_src = LEELEN_VOIP_PORT;
  self->control_src = LEELEN_CONRTOL_PORT;

  self->mtu = LEELEN_MAX_MESSAGE_LENGTH;

  self->voip_timeout = LEELEN_VOIP_TIMEOUT;
  self->duration = LEELEN_VOIP_DURATION;

  self->audio = LEELEN_AUDIO_PORT;
  self->video = LEELEN_VIDEO_PORT;

  return 0;
}
