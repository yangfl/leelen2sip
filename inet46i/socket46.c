#include <errno.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "private/macro.h"
#include "inet46.h"
#include "sockaddr46.h"
#include "socket46.h"


int openaddrz (
    const struct sockaddr * __restrict addr, int type, int flags,
    int scope_id) {
  int fd = socket(addr->sa_family, type, 0);
  return_if_fail (fd >= 0) -1;

  const int on = 1;
  if (flags & OPENADDR_REUSEADDR) {
    goto_if_fail (setsockopt(
      fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == 0) fail;
  }
  if (flags & OPENADDR_REUSEPORT) {
    goto_if_fail (setsockopt(
      fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) == 0) fail;
  }
  if (flags & OPENADDR_V6ONLY) {
    goto_if_fail (setsockopt(
      fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) == 0) fail;
  }

  if (scope_id != 0) {
    char ifname[IFNAMSIZ];
    goto_if_fail (if_indextoname(scope_id, ifname) != NULL) fail;
    goto_if_fail (setsockopt(
      fd, SOL_SOCKET, SO_BINDTODEVICE, ifname, sizeof(ifname)) == 0) fail;
  }

  goto_if_fail (bind(fd, addr, addr->sa_family == AF_INET ?
    sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) == 0) fail;

  return fd;

  int err;
fail:
  err = errno;
  close(fd);
  errno = err;
  return -1;
}


int openaddr (const struct sockaddr * __restrict addr, int type, int flags) {
  return openaddrz(
    addr, type, flags, addr->sa_family == AF_INET6 ?
      ((const struct sockaddr_in6 *) addr)->sin6_scope_id : 0);
}


int openaddr46 (
    const union sockaddr_in46 * __restrict addr, int type, int flags) {
  return openaddrz(&addr->sock, type, flags, addr->sa_scope_id);
}
