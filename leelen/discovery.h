#ifndef LEELEN_DISCOVERY_H
#define LEELEN_DISCOVERY_H

#include <regex.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <netinet/in.h>

// #include "config.h"
struct LeelenConfig;
#include "host.h"


struct LeelenDiscovery {
  const struct LeelenConfig *config;
  regex_t phone;
  bool phone_set;
  char report_addr[INET6_ADDRSTRLEN];

  unsigned char domain;
  int socket;
  atomic_bool waiting;
  struct LeelenHost last;
};

__attribute__((
  nonnull, access(read_only, 2), access(read_only, 4), access(read_only, 5)))
int LeelenDiscovery_receive (
  struct LeelenDiscovery *self, const char *msg, int socket,
  const struct sockaddr *theirs, const void *ours, int ifindex);
__attribute__((nonnull, warn_unused_result, access(read_only, 2)))
int LeelenDiscovery_send (
  struct LeelenDiscovery *self, const char *phone, int socket);
__attribute__((nonnull, access(read_only, 3)))
int LeelenDiscovery_set_report_addr (
  struct LeelenDiscovery *self, int af, const void *addr);

__attribute__((nonnull))
inline void LeelenDiscovery_destroy (struct LeelenDiscovery *self) {
  if (self->phone_set) {
    regfree(&self->phone);
  }
  LeelenHost_destroy(&self->last);
}

__attribute__((nonnull(1), access(write_only, 1), access(read_only, 2)))
inline int LeelenDiscovery_init (
    struct LeelenDiscovery *self, const struct LeelenConfig *config) {
  self->config = config;
  self->phone_set = false;
  self->report_addr[0] = '\0';
  self->domain = AF_UNSPEC;
  self->socket = -1;
  self->waiting = false;
  self->last.desc = NULL;
  return 0;
}


#endif /* LEELEN_DISCOVERY_H */
