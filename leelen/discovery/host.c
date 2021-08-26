#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <inet46i/inet46.h>
#include <inet46i/sockaddr46.h>

#include "utils/macro.h"
#include "../family.h"
#include "protocol.h"
#include "host.h"


int LeelenHost_init (
    struct LeelenHost *self, const char *report, const struct sockaddr *src) {
  int ret = 0;
  self->desc = NULL;

  // addr
  char *msg_addr_sep = strchr(report, '?');
  {
    char saddr[SOCKADDR_STRLEN];
    unsigned int addrlen = msg_addr_sep != NULL ?
      (unsigned int) (msg_addr_sep - report) : strnlen(report, sizeof(saddr));
    goto_if_fail (addrlen < sizeof(saddr)) fail_addr;
    memcpy(saddr, report, addrlen);
    saddr[addrlen] = '\0';
    should (
        sockaddr46_aton(saddr, (union sockaddr_in46 *) self) != 0) otherwise {
fail_addr:
      ret |= 1;
      if (src != NULL) {
        memcpy(self, src, sizeof(union sockaddr_in46));
      } else {
        self->sa_family = AF_UNSPEC;
      }
    }
    self->sa_port = 0;
  }
  return_if_fail (msg_addr_sep != NULL) ret | 2 | 4;

  // src
  if (src != NULL) {
    memcpy(&self->src, src, sizeof(union sockaddr_in46));
  }

  // type
  char *msg_type_end;
  self->type = strtol(msg_addr_sep + 1, &msg_type_end, 10);
  should (*msg_type_end == '*') otherwise {
    ret |= 2;
    msg_type_end = strchr(msg_type_end, '*');
  }

  // desc
  goto_if_fail (msg_type_end != NULL && *msg_type_end != '\0') fail_desc;
  self->desc = strdup(msg_type_end + 1);
  should (self->desc != NULL) otherwise {
fail_desc:
    ret |= 4;
  }

  return ret;
}


int LeelenHost__discovery (
    int af, int sockfd, in_port_t port, const char *phone) {
  return_if_fail (af == AF_INET || af == AF_INET6) 255;

  // prepare destination address
  union sockaddr_in46 addr;
  sockaddr46_set(
    &addr, af, af == AF_INET6 ?
      (const void *) &leelen_discovery_groupaddr6 :
      (const void *) &leelen_discovery_groupaddr,
    port == 0 ? LEELEN_DISCOVERY_PORT : port);
  // send
  int phone_len = strlen(phone);
  return sendto(
    sockfd, phone, phone_len, 0, &addr.sock, af == AF_INET ?
      sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
  ) == phone_len ? 0 : -1;
}


static int LeelenHost_init_read (
    struct LeelenHost *self, int sockfd, int *initerr) {
  // receive
  char buf[LEELEN_MAX_MESSAGE_LENGTH + 1];
  union sockaddr_in46 addr;
  socklen_t addrlen = sizeof(addr);
  int buflen =
    recvfrom(sockfd, buf, sizeof(buf) - 1, MSG_DONTWAIT, &addr.sock, &addrlen);
  return_if_fail (buflen > 0) 1;
  buf[buflen] = '\0';
  return_if_fail (Leelen_discovery_is_advertisement(buf)) 255;
  // parse
  int res = LeelenHost_init(self, buf, &addr.sock);
  if unlikely (initerr != NULL) {
    *initerr = res;
  }
  return 0;
}


static suseconds_t timems (void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


int LeelenHost_init_recv (
    struct LeelenHost *self, int sockfd, int timeout, int *initerr) {
  struct pollfd pollfd = {.fd = sockfd, .events = POLLIN};
  if likely (timeout == 0) {
    timeout = LEELEN_DISCOVERY_TIMEOUT;
  }

  for (suseconds_t start = timems(); ; ) {
    suseconds_t elapsed = timems() - start;
    break_if_fail (
      timeout > elapsed && poll(&pollfd, 1, timeout - elapsed) > 0);
    int res = LeelenHost_init_read(self, sockfd, initerr);
    return_if (res != 255) res;
  }
  return 2;
}


int LeelenHost_init_discovery (
    struct LeelenHost *self, int af, int sockfd, in_port_t port,
    const char *phone, int timeout, int *initerr) {
  return_nonzero (LeelenHost__discovery(af, sockfd, port, phone));
  return LeelenHost_init_recv(self, sockfd, timeout, initerr);
}
