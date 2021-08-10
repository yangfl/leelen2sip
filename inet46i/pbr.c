#define _GNU_SOURCE

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "private/macro.h"
#include "in46.h"
#include "route.h"
#include "socket46.h"
#include "sockaddr46.h"
#include "pbr.h"


bool sockaddrpr_match (
    const struct SockaddrPolicy *policy, const union sockaddr_in46 *addr) {
  const union in46_addr *host;
  in_port_t port = sockaddr46_get(&addr->sock, (void **) &host);

  if (policy->prefix != 0 && (
        addr->sa_family != AF_INET ||
        IN6_IS_ADDR_V4MAPPED(&policy->network6) ||
        IN6_IS_ADDR_V4COMPAT(&policy->network6)
      ) && (
        addr->sa_family == AF_INET ? host->v4.s_addr != 0 :
          !IN6_IS_ADDR_UNSPECIFIED(&host->v6))) {
    return_if_not (inet_innetwork(
      addr->sa_family, host, addr->sa_family == AF_INET ?
        (const void *) &policy->network : (const void *) &policy->network6,
      policy->prefix)) false;
  }
  if (port != 0) {
    return_if_not (policy->port_max == 0 ?
      policy->port == 0 || port == policy->port :
      policy->port_min <= port && port <= policy->port_max) false;
  }
  if (policy->scope_id != 0 && addr->sa_scope_id != 0) {
    return_if_not (policy->scope_id == addr->sa_scope_id) false;
  }
  return true;
}


struct SocketPolicyRoute *socketpr_resolve (
    const struct SocketPolicyRoute *routes, const struct Socket5Tuple *conn) {
  for (int i = 0; routes[i].func != NULL; i++) {
    continue_if_not (sockaddrpr_match(&routes[i].src, &conn->src));
    continue_if_not (sockaddrpr_match(&routes[i].dst, &conn->dst));
    return (struct SocketPolicyRoute *) routes + i;
  }
  return NULL;
}


int socketpbr_accept4 (int socket, const struct SocketPBR *pbr, int flags) {
  struct Socket5Tuple conn;
  conn.src.sa_scope_id = 0;
  conn.dst.sa_scope_id = 0;

  socklen_t srclen = sizeof(conn.src);
  int socket_child = accept4(socket, &conn.src.sock, &srclen, flags);
  return_if_fail (socket_child >= 0) pbr->nothing_ret;

  if (!pbr->dst_required) {
    memset(&conn.dst, 0, sizeof(conn.dst));
  } else {
    if (pbr->dst_addr.sa_family == AF_UNSPEC) {
      socklen_t dstlen = sizeof(conn.dst);
      getsockname(socket_child, &conn.dst.sock, &dstlen);
    } else {
      memcpy(&conn.dst, &pbr->dst_addr, sizeof(union sockaddr_in46));
    }
  }

  SocketPolicyRouteFunc func;
  void *context;
  if (pbr->single_route) {
    func = pbr->func;
    context = pbr->context;
  } else {
    const struct SocketPolicyRoute *route = socketpr_resolve(pbr->routes, &conn);
    return_if_fail (route != NULL) pbr->nothing_ret;
    func = route->func;
    context = route->context;
  }
  return ((SocketPolicyRouteStreamFunc) func)(context, socket_child, &conn);
}


int socketpbr_accept (int socket, const struct SocketPBR *pbr) {
  return socketpbr_accept4(socket, pbr, 0);
}


static int getsockport (int socket) {
  struct sockaddr addr;
  socklen_t addrlen = sizeof(addr);
  return_if_fail (getsockname(socket, &addr, &addrlen) == 0) -1;
  return sockaddr46_get(&addr, NULL);
}


int socketpbr_recv (int socket, const struct SocketPBR *pbr, int flags) {
  struct Socket5Tuple conn;
  conn.src.sa_scope_id = 0;
  memset(&conn.dst, 0, sizeof(conn.dst));
  union in46_addr spec_dst;

  char buf[pbr->maxlen];
  int buflen;

  if (!pbr->dst_required) {
    socklen_t srclen = sizeof(conn.src);
    buflen = recvfrom(
      socket, buf, sizeof(buf), flags, &conn.src.sock, &srclen);
    return_if_fail (buflen > 0) pbr->nothing_ret;
  } else {
    char cmbuf[1024];
    struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};
    struct msghdr mh = {
      .msg_name = &conn.src.sock, .msg_namelen = sizeof(conn.src),
      .msg_iov = &iov, .msg_iovlen = 1,
      .msg_control = cmbuf, .msg_controllen = sizeof(cmbuf),
    };

    buflen = recvmsg(socket, &mh, flags);
    return_if_fail (buflen > 0) pbr->nothing_ret;

    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&mh); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&mh, cmsg)) {
      switch (cmsg->cmsg_level) {
        case IPPROTO_IP:
          if (cmsg->cmsg_type == IP_PKTINFO) {
            struct in_pktinfo *pi = (void *) CMSG_DATA(cmsg);
            conn.dst.sa_family = AF_INET;
            memcpy(&conn.dst.v4.sin_addr, &pi->ipi_addr, sizeof(pi->ipi_addr));
            conn.dst.v4.sin_port = pbr->dst_port != 0 ?
              pbr->dst_port : getsockport(socket);
            memcpy(&spec_dst, &pi->ipi_spec_dst, sizeof(pi->ipi_spec_dst));
            conn.dst.sa_scope_id = pi->ipi_ifindex;
          }
          break;
        case IPPROTO_IPV6:
          if (cmsg->cmsg_type == IPV6_PKTINFO) {
            struct in6_pktinfo *pi = (void *) CMSG_DATA(cmsg);
            conn.dst.sa_family = AF_INET6;
            memcpy(&conn.dst.v6.sin6_addr, &pi->ipi6_addr,
                   sizeof(pi->ipi6_addr));
            conn.dst.v6.sin6_port = pbr->dst_port != 0 ?
              pbr->dst_port : getsockport(socket);
            memcpy(&spec_dst, &pi->ipi6_addr, sizeof(pi->ipi6_addr));
            conn.dst.sa_scope_id = pi->ipi6_ifindex;
          }
          break;
      }
    }
  }

  SocketPolicyRouteFunc func;
  void *context;
  if (pbr->single_route) {
    func = pbr->func;
    context = pbr->context;
  } else {
    const struct SocketPolicyRoute *route = socketpr_resolve(pbr->routes, &conn);
    return_if_fail (route != NULL) pbr->nothing_ret;
    func = route->func;
    context = route->context;
  }
  return ((SocketPolicyRouteDgramFunc) func)(
    context, buf, buflen, socket, &conn, &spec_dst);
}


int socketpbr_read (int socket, const struct SocketPBR *pbr) {
  return socketpbr_recv(socket, pbr, 0);
}


int socketpbr_set (int socket, struct SocketPBR *pbr, int af) {
  (void) pbr;

  if (af == 0) {
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    return_if_fail (getsockname(socket, &addr, &addrlen) == 0) -1;
    af = addr.sa_family;
  }

  should (af == AF_INET || af == AF_INET6) otherwise {
    errno = EAFNOSUPPORT;
    return -1;
  }

  int enabled = 1;
  return setsockopt(
    socket, af == AF_INET ? IPPROTO_IP : IPPROTO_IPV6,
    af == AF_INET ? IP_PKTINFO : IPV6_RECVPKTINFO,
    &enabled, sizeof(enabled));
}
