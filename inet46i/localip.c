#include <errno.h>
#include <ifaddrs.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "private/macro.h"
#include "endian.h"
#include "sockaddr46.h"
#include "socket46.h"
#include "localip.h"


int detect_iface_ip (int af, void * __restrict addr, unsigned int ifindex) {
  should (af == AF_INET || af == AF_INET6) otherwise {
    errno = EAFNOSUPPORT;
    return -1;
  }

  char ifname[IF_NAMESIZE];
  if (ifindex != 0) {
    return_if_fail (if_indextoname(ifindex, ifname) != NULL) -1;
  }

  struct ifaddrs *ifaddr;
  return_nonzero (getifaddrs(&ifaddr));
  unsigned int level = 0;

  for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    continue_if_not (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == af);
    if (ifindex != 0) {
      continue_if_not (strncmp(ifname, ifa->ifa_name, sizeof(ifname)) == 0);
    }

    if (af == AF_INET) {
      struct in_addr *if_addr =
        &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
      continue_if (if_addr->s_addr == lhtonl(INADDR_ANY));
      continue_if (if_addr->s_addr == lhtonl(INADDR_BROADCAST));
      continue_if ((if_addr->s_addr & lhtonl(IN_CLASSA_NET)) ==
                   lhtonl(INADDR_LOOPBACK & IN_CLASSA_NET));
      continue_if ((if_addr->s_addr & lhtonl(0xf0000000)) ==
                   lhtonl(0xe0000000));
      *(struct in_addr *) addr = *if_addr;
      goto end;
    } else {
      struct in6_addr *if_addr =
        &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
      continue_if (IN6_IS_ADDR_UNSPECIFIED(if_addr));
      continue_if (IN6_IS_ADDR_LOOPBACK(if_addr));
      continue_if (IN6_IS_ADDR_MULTICAST(if_addr));
      bool found = false;
      if (IN6_IS_ADDR_LINKLOCAL(if_addr)) {
        continue_if_not (level < 1);
        level = 1;
      } else if (IN6_IS_ADDR_SITELOCAL(if_addr)) {
        continue_if_not (level < 2);
        level = 2;
      } else {
        found = true;
      }
      *(struct in6_addr *) addr = *if_addr;
      goto_if (found) end;
    }
  }

  freeifaddrs(ifaddr);
  return level == 0 ? -1 : 0;

end:
  freeifaddrs(ifaddr);
  return 0;
}


int detect_src_ip (
    int af, void * __restrict addr, const void * __restrict dst,
    unsigned int ifindex) {
  union sockaddr_in46 remote = {
    .sa_family = af,
    .sa_port = 0xcccc,
    .sa_scope_id = ifindex,
  };
  if (af == AF_INET) {
    remote.v4.sin_addr = *(struct in_addr *) dst;
  } else if likely (af == AF_INET6) {
    remote.v6.sin6_addr = *(struct in6_addr *) dst;
  } else {
    errno = EAFNOSUPPORT;
    return -1;
  }

  int type = SOCK_DGRAM;
#ifdef SOCK_CLOEXEC
  type = SOCK_CLOEXEC | SOCK_DGRAM;
#endif
  int sockfd = openaddr46(&remote, type, OPENADDR_REUSEADDR);
  return_if_fail (sockfd >= 0) -1;

  const int on = 1;
  goto_if_fail (setsockopt(
    sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) == 0) fail;

  union sockaddr_in46 local;
  socklen_t len = sizeof(local);
  goto_if_fail (getsockname(sockfd, &local.sock, &len) == 0) fail;

  if (local.sa_family == AF_INET) {
    *(struct in_addr *) addr = local.v4.sin_addr;
  } else {
    *(struct in6_addr *) addr = local.v6.sin6_addr;
  }

  close(sockfd);
  return 0;

fail:
  close(sockfd);
  return -1;
}


int detect_local_ip (
    int af, void * __restrict addr, const void * __restrict dst,
    unsigned int ifindex) {
  return dst == NULL ?
    detect_iface_ip(af, addr, ifindex) : detect_src_ip(af, addr, dst, ifindex);
}
