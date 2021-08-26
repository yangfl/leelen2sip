#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>  // snprintf
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <inet46i/in46.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "../config.h"
#include "../number.h"
#include "protocol.h"
#include "advertiser.h"


bool LeelenAdvertiser_should_reply (
    const struct LeelenAdvertiser *self, const char *number) {
  return_if (self->number_regex_set)
    regexec(&self->number_regex, number, 0, NULL, 0) == 0;
  return_if (self->config->number.str[0] != 0)
    LeelenNumber_should_reply(&self->config->number, number);
  // No phone number rules set, return false
  return false;
}


int LeelenAdvertiser_reply (
    const struct LeelenAdvertiser *self, int sockfd,
    const struct sockaddr *addr, const void *ouraddr) {
  // get our addr, if not pre-defined
  char s_ouraddr[INET6_ADDRSTRLEN];
  if (self->report_addr[0] == '\0') {
    return_if_fail (ouraddr != NULL) 255;
    inet_ntop(addr->sa_family,
              addr->sa_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(ouraddr) ?
                &((const struct in64_addr *) ouraddr)->addr : ouraddr,
              s_ouraddr, sizeof(s_ouraddr));
  }

  char buf[self->config->mtu];
  int buflen = snprintf(
    buf, sizeof(buf), LEELEN_DISCOVERY_FORMAT,
    self->report_addr[0] == '\0' ? s_ouraddr : self->report_addr,
    self->config->type, self->config->desc);
  LOG(LOG_LEVEL_INFO, "I'm %s", buf);
  return sendto(
    sockfd, buf, buflen, MSG_CONFIRM, addr, addr->sa_family == AF_INET ?
      sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
  ) == buflen ? 0 : -1;
}


int LeelenAdvertiser_receive (
    const struct LeelenAdvertiser *self, const char *msg, int sockfd,
    const struct sockaddr *src, const void *dst, int ifindex) {
  bool should_reply =
    !Leelen_discovery_is_advertisement(msg) &&
    LeelenAdvertiser_should_reply(self, msg);
  Leelen_discovery_logger(msg, src, dst, ifindex, should_reply);
  return !should_reply ?
    254 : LeelenAdvertiser_reply(self, sockfd, src, dst);
}


int LeelenAdvertiser_set_report_addr (
    struct LeelenAdvertiser *self, int af, const void *addr) {
  if (af == AF_INET6) {
    if (IN6_IS_ADDR_LOOPBACK(addr)) {
      strcpy(self->report_addr, "127.0.0.1");
      return 0;
    }
    if (IN6_IS_ADDR_V4OK(addr)) {
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


int LeelenAdvertiser_fix_report_addr (struct LeelenAdvertiser *self) {
  return LeelenAdvertiser_set_report_addr(
    self, self->config->addr.sa_family,
    sockaddr_addr(&self->config->addr.sock));
}
