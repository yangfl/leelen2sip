#include <regex.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <inet46i/in46.h>
#include <inet46i/sockaddr46.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "config.h"
#include "host.h"
#include "protocol.h"
#include "discovery.h"


extern inline void LeelenDiscovery_destroy (struct LeelenDiscovery *self);
extern inline int LeelenDiscovery_init (
  struct LeelenDiscovery *self, const struct LeelenConfig *config);


int LeelenDiscovery_receive (
    struct LeelenDiscovery *self, const char *msg, int socket,
    const struct sockaddr *theirs, const void *ours, int ifindex) {
  // 1: release lock, 0: lock then unlock, -1: do not lock
  char *msg_addr_sep = strchr(msg, '?');

  enum LogLevelFlags lvl = LOG_LEVEL_INFO;
  bool should_reply = false;
  if (msg_addr_sep == NULL) {
    should_reply = !self->phone_set ||
                   regexec(&self->phone, msg, 0, NULL, 0) == 0;
    if (!should_reply) {
      lvl = LOG_LEVEL_DEBUG;
    }
  }

  if (logger_would_log(lvl)) {
    char sours[INET6_ADDRSTRLEN];
    char stheirs[SOCKADDR_STRLEN];
    inet_ntop(theirs->sa_family, ours, sours, sizeof(sours));
    sockaddrz_toa(theirs, stheirs, sizeof(stheirs), ifindex);
    if (msg_addr_sep == NULL) {
      LOGGER(LEELEN_NAME, lvl, "[%s] Who is %s? Tell %s", sours, msg, stheirs);
    } else {
      LOGGER(LEELEN_NAME, lvl, "[%s] %s: I'm %s", sours, stheirs, msg);
    }
  }

  if (msg_addr_sep != NULL) {
    LeelenHost_destroy(&self->last);
    should ((LeelenHost_init(&self->last, msg) & 1) == 0 && (
        self->domain != AF_INET || IN6_IS_ADDR_V4MAPPED(&self->last.addr6)
    )) otherwise {
      *msg_addr_sep = '\0';
      LOGGER(
        LEELEN_NAME, LOG_LEVEL_WARNING,
        self->domain == AF_INET && !IN6_IS_ADDR_V4MAPPED(&self->last.addr6) ?
          "'%s' looks like an IPv6 address, but socket is IPv4!" :
          "'%s' doesn't look like a valid reply!", msg);
      *msg_addr_sep = '?';
      self->last.desc = NULL;
      in64_set(
        self->domain, (struct in64_addr *) &self->last,
        self->domain == AF_INET ?
          (const void *) &((const struct sockaddr_in *)theirs)->sin_addr :
          (const void *) &((const struct sockaddr_in6 *)theirs)->sin6_addr);
    }

    self->waiting = false;
    return LEELEN_DEVICE_SOLICITATION;
  }

  if (should_reply) {
    char ouraddr[INET6_ADDRSTRLEN];
    if (self->report_addr[0] == '\0') {
      inet_ntop(theirs->sa_family,
                theirs->sa_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(ours) ?
                  &((const struct in64_addr *) ours)->addr : ours,
                ouraddr, sizeof(ouraddr));
    }
    char buf[LEELEN_MAX_MESSAGE_LENGTH];
    int buflen = snprintf(
      buf, sizeof(buf), LEELEN_DISCOVERY_FORMAT,
      self->report_addr[0] == '\0' ? ouraddr : self->report_addr,
      self->config->type, self->config->desc);
    LOGGER(LEELEN_NAME, LOG_LEVEL_INFO, "I'm %s", buf);
    return_if_fail (sendto(
      socket, buf, buflen, MSG_CONFIRM, theirs, theirs->sa_family == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
    ) == buflen) -1;
    return LEELEN_DEVICE_DISCOVERY;
  }

  return 0;
}


int LeelenDiscovery_send (
    struct LeelenDiscovery *self, const char *phone, int socket) {
  return_if_fail (!atomic_exchange(&self->waiting, true)) 1;
  if (socket < 0) {
    socket = self->socket;
  }

  union sockaddr_in46 theirs;
  struct in64_addr groupaddr6;
  if (self->domain == AF_INET6) {
    in64_set(AF_INET, &groupaddr6, &leelen_discovery_groupaddr);
  }
  sockaddr46_set(
    &theirs, self->domain, self->domain == AF_INET6 ?
      (const void *) &groupaddr6 : (const void *) &leelen_discovery_groupaddr,
    self->config->discovery);
  int phone_len = strlen(phone);
  should (sendto(
      socket, phone, phone_len, 0, &theirs.sock, self->domain == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
  ) == phone_len) otherwise {
    self->waiting = false;
    return 2;
  }
  return 0;
}


int LeelenDiscovery_set_report_addr (
    struct LeelenDiscovery *self, int af, const void *addr) {
  if (af == AF_INET6) {
    if (IN6_IS_ADDR_LOOPBACK(addr)) {
      strcpy(self->report_addr, "127.0.0.1");
      return 0;
    }
    if (IN6_IS_ADDR_V4COMPAT(addr) || IN6_IS_ADDR_V4MAPPED(addr)) {
      af = AF_INET;
      addr = &((const struct in6_addr *) addr)->s6_addr32[3];
    }
  }
  if (memcmp(&in6addr_any, addr, af == AF_INET ?
              sizeof(struct in_addr) : sizeof(struct in6_addr)) != 0) {
    inet_ntop(af, addr, self->report_addr, sizeof(self->report_addr));
  }
  return 0;
}
