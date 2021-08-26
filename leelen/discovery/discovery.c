#include <errno.h>
#include <poll.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <inet46i/in46.h>
#include <inet46i/recvfromto.h>
#include <inet46i/sockaddr46.h>
#include <inet46i/socket46.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "utils/single.h"
#include "utils/threadname.h"
#include "../config.h"
#include "advertiser.h"
#include "host.h"
#include "protocol.h"
#include "discovery.h"


int LeelenDiscovery_discovery (
    struct LeelenDiscovery *self, struct LeelenHost *host, const char *phone) {
  return_if_fail (single_is_running(&self->state)) 255;

  mtx_lock(&self->mutex);

  // write output parameters
  self->host = host;
  self->initres = 254;  // timeout reached

  // send solicitation
  int res;
  if (self->spec_sockfd >= 0) {
    res = LeelenHost__discovery(
      self->spec_af, self->spec_sockfd, self->config->discovery, phone);
  } else {
    res = likely (self->sockfd6 < 0) ? 255 : LeelenHost__discovery(
      AF_INET6, self->sockfd6, self->config->discovery, phone);
    if likely (self->sockfd >= 0) {
      int res4 = LeelenHost__discovery(
        AF_INET, self->sockfd, self->config->discovery, phone);
      if (res != 0) {
        res = res4;
      }
    }
  }

  if likely (res == 0) {
    // wait for reply
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    ts.tv_nsec += self->config->timeout * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000;  // handle nsec overflow
    ts.tv_nsec %= 1000000000;
    cnd_timedwait(&self->cond, &self->mutex, &ts);
    // fetch result
    self->host = NULL;
    res = self->initres;
  }

  mtx_unlock(&self->mutex);
  return res;
}


/**
 * @memberof LeelenDiscovery
 * @private
 * @brief Discovery main thread.
 *
 * @param args Discovery daemon.
 * @return 0.
 */
static int LeelenDiscovery_mainloop (void *args) {
  threadname_set("Discovery");

  struct LeelenDiscovery *self = args;
  void *spec_dst = sockaddr_addr(&self->config->addr.sock);
  uint32_t spec_ifindex = self->config->addr.sa_scope_id;

  struct pollfd pollfds[3];
  int n_pollfd = 0;
  bool has_spec = self->spec_sockfd >= 0;

  if (self->spec_sockfd >= 0) {
    pollfds[n_pollfd].fd = self->spec_sockfd;
    pollfds[n_pollfd].events = POLLIN;
    n_pollfd++;
  }
  if (self->sockfd >= 0) {
    pollfds[n_pollfd].fd = self->sockfd;
    pollfds[n_pollfd].events = POLLIN;
    n_pollfd++;
  }
  if (self->sockfd6 >= 0) {
    pollfds[n_pollfd].fd = self->sockfd6;
    pollfds[n_pollfd].events = POLLIN;
    n_pollfd++;
  }

  while (single_continue(&self->state)) {
    int pollres = poll(pollfds, n_pollfd, 1000);
    should (pollres >= 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "poll() failed");
      continue;
    }
    continue_if (likely (pollres <= 0));

    // process received packets
    for (int i = 0; i < 2; i++) {
      continue_if (pollfds[i].revents == 0);

      // read incoming packet
      char buf[self->config->mtu + 1];
      union sockaddr_in46 src;
      socklen_t srclen = sizeof(src);
      union sockaddr_in46 dst;
      struct in_addr recv_dst;
      int buflen = recvfromto(pollfds[i].fd, buf, sizeof(buf) - 1, 0, &src.sock,
                              &srclen, &dst, &recv_dst);
      should (buflen >= 0) otherwise {
        LOG_PERROR(LOG_LEVEL_WARNING, "recvfromto() failed");
        continue;
      }
      continue_if_fail (buflen > 0);
      buf[buflen] = '\0';

      // log
      bool is_advertisement = Leelen_discovery_is_advertisement(buf);
      bool should_reply = !is_advertisement && LeelenAdvertiser_should_reply(
        (struct LeelenAdvertiser *) self, buf);
      Leelen_discovery_logger(
        buf, &src.sock, has_spec && i == 0 ? spec_dst : &recv_dst,
        has_spec && i == 0 ? spec_ifindex : dst.sa_scope_id, should_reply);

      // process
      if (is_advertisement) {
        if likely (self->host != NULL) {
          // filter destination address
          if (has_spec && i != 0) {
            LOGEVENT (LOG_LEVEL_INFO) {
              char s_dst[SOCKADDR_STRLEN];
              sockaddr46_toa(&dst, s_dst, sizeof(s_dst));
              LOGEVENT_LOG(
                "Got advertisement to %s, which is not the sending address",
                s_dst);
            }
          } else {
            mtx_lock(&self->mutex);
            if likely (self->host != NULL) {
              self->initres = LeelenHost_init(self->host, buf, &src.sock);
              self->host = NULL;
              cnd_signal(&self->cond);
            }
            mtx_unlock(&self->mutex);
          }
        }
      } else if (should_reply) {
        should (LeelenAdvertiser_reply(
            (struct LeelenAdvertiser *) self, pollfds[i].fd, &src.sock,
            &recv_dst) == 0) otherwise {
          LOG_PERROR(LOG_LEVEL_WARNING, "sendto() failed");
        }
      }
    }
  }

  return 0;
}


int LeelenDiscovery_run (struct LeelenDiscovery *self) {
  return_if_fail (self->sockfd >= 0 || self->sockfd6 >= 0) 255;

  char name[THREADNAME_SIZE];
  threadname_get(name, sizeof(name));
  int res = single_run(&self->state, LeelenDiscovery_mainloop, self);
  threadname_set(name);
  return res;
}


int LeelenDiscovery_start (struct LeelenDiscovery *self) {
  return_if_fail (self->sockfd >= 0 || self->sockfd6 >= 0) 255;
  return single_start(&self->state, LeelenDiscovery_mainloop, self);
}


int LeelenDiscovery_connect (struct LeelenDiscovery *self) {
  const struct LeelenConfig *config = self->config;

  union sockaddr_in46 leelen_addr_local = {
    .sa_scope_id = config->addr.sa_scope_id,
    .sa_port = htons(config->discovery_src),
  };
  const int on = 1;

  bool listen_v6 = config->addr.sa_family == AF_INET6;
  bool listen_v4 = config->addr.sa_family != AF_INET6 ||
                   IN6_IS_ADDR_UNSPECIFIED(&config->addr.v6.sin6_addr);
  bool has_spec = config->addr.sa_family == AF_INET6 ?
    !IN6_IS_ADDR_UNSPECIFIED(&config->addr.v6.sin6_addr) :
    config->addr.v4.sin_addr.s_addr != 0;

  // open v6 socket
  if (unlikely (listen_v6) && likely (self->sockfd6 < 0)) {
    leelen_addr_local.sa_family = AF_INET6;

    self->sockfd6 = openaddr46(
      &leelen_addr_local, SOCK_DGRAM, OPENADDR_REUSEADDR | OPENADDR_V6ONLY);
    should (self->sockfd6 >= 0) otherwise {
      return_if_fail (listen_v4) -1;
      LOG_PERROR(LOG_LEVEL_WARNING, "Cannot open IPv6 socket");
    };

    should (setsockopt(
        self->sockfd6, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)
    ) == 0) otherwise {
      int saved_errno = errno;
      close(self->sockfd6);
      self->sockfd6 = -1;
      errno = saved_errno;
      return -1;
    }
  }

  // open v4 socket
  if (likely (listen_v4) && likely (self->sockfd < 0)) {
    leelen_addr_local.sa_family = AF_INET;

    self->sockfd = openaddr46(
      &leelen_addr_local, SOCK_DGRAM, OPENADDR_REUSEADDR);
    return_if_fail (self->sockfd >= 0) -1;

    should (setsockopt(
        self->sockfd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on)) == 0) otherwise {
      int saved_errno = errno;
      close(self->sockfd);
      self->sockfd = -1;
      errno = saved_errno;
      return -1;
    }
  }

  // open spec socket
  if (has_spec) {
    in_port_t port = leelen_addr_local.sa_port;
    leelen_addr_local = config->addr;
    leelen_addr_local.sa_port = port;

    self->spec_sockfd = openaddr46(
      &leelen_addr_local, SOCK_DGRAM, OPENADDR_REUSEADDR);
    return_if_fail (self->spec_sockfd >= 0) -1;

    self->spec_af = config->addr.sa_family;
  }

  return 0;
}


void LeelenDiscovery_destroy (struct LeelenDiscovery *self) {
  LeelenDiscovery_stop(self);
  if likely (self->sockfd >= 0) {
    close(self->sockfd);
  }
  if unlikely (self->sockfd6 >= 0) {
    close(self->sockfd6);
  }
  cnd_broadcast(&self->cond);
  single_join(&self->state);

  mtx_destroy(&self->mutex);
  cnd_destroy(&self->cond);
  LeelenAdvertiser_destroy((struct LeelenAdvertiser *) self);
}


int LeelenDiscovery_init (
    struct LeelenDiscovery *self, const struct LeelenConfig *config) {
  return_if_fail (mtx_init(&self->mutex, mtx_plain) == thrd_success) -1;
  should (cnd_init(&self->cond) == thrd_success) otherwise {
    int saved_errno = errno;
    mtx_destroy(&self->mutex);
    errno = saved_errno;
    return -1;
  }
  self->state = false;
  self->spec_af = AF_UNSPEC;
  self->spec_sockfd = -1;
  self->sockfd = -1;
  self->sockfd6 = -1;
  return LeelenAdvertiser_init((struct LeelenAdvertiser *) self, config);
}
