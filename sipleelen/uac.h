#ifndef SIPLEELEN_UAC_H
#define SIPLEELEN_UAC_H

#ifdef __cplusplus
extern "C" {
#endif

// #include <sys/socket.h>
struct sockaddr;

// #include "session.h"
struct SIPLeelenSession;
// #include "sipleelen.h"
struct SIPLeelen;


int SIPLeelenSession_receive (
  struct SIPLeelenSession *self, char *msg, int sockfd,
  const struct sockaddr *src);


#ifdef __cplusplus
}
#endif

#endif /* SIPLEELEN_UAC_H */
