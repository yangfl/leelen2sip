#include "config.h"


extern inline int LeelenConfig_init_default (struct LeelenConfig *self);
extern inline void LeelenConfig_destroy (struct LeelenConfig *self);


const char * const leelen_protocol_name[3] = {
  "LEELEN-Discovery", "LEELEN-SIP", "LEELEN-Control"};
