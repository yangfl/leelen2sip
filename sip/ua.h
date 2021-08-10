#ifndef SIP_UA_H
#define SIP_UA_H

#include <osip2/osip.h>


#define LEELEN2SIP_PORT 5060
#define LEELEN2SIP_MTU 1500
#define LEELEN2SIP_MAX_MESSAGE_LENGTH (LEELEN2SIP_MTU - 20 - 8)


struct LeelenSIP {
  char *ua;

  struct LeelenDiscovery *device;
  osip_t *osip;
  int domain;
  int socket;
};

__attribute__((nonnull(1, 2)))
int LeelenSIP_receive (
  struct LeelenSIP *self, char *buf, int buflen, int socket,
  struct sockaddr *addr);
__attribute__((nonnull))
void LeelenSIP_execute (struct LeelenSIP *self);
__attribute__((nonnull))
void LeelenSIP_destroy (struct LeelenSIP *self);
__attribute__((nonnull, warn_unused_result, access(read_only, 2)))
int LeelenSIP_init (struct LeelenSIP *self, struct LeelenDiscovery *device);


#endif /* SIP_UA_H */
