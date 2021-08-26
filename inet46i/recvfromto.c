#define _GNU_SOURCE

#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "private/macro.h"
#include "sockaddr46.h"
#include "recvfromto.h"


ssize_t recvfromto(
    int sockfd, void * __restrict buf, size_t len, int flags,
    struct sockaddr * __restrict src_addr, socklen_t * __restrict addrlen,
    union sockaddr_in46 * __restrict dst_addr,
    struct in_addr * __restrict recv_dst) {
  return_if_fail (dst_addr != NULL || recv_dst != NULL)
    recvfrom(sockfd, buf, len, flags, src_addr, addrlen);

  // prepare buffers
  union sockaddr_in46 src_addr_buf;
  unsigned char cmbuf[1024];
  struct iovec iov = {.iov_base = buf, .iov_len = len};
  struct msghdr mh = {
    .msg_name = &src_addr_buf, .msg_namelen = sizeof(src_addr_buf),
    .msg_iov = &iov, .msg_iovlen = 1,
    .msg_control = cmbuf, .msg_controllen = sizeof(cmbuf),
  };

  ssize_t recvlen = recvmsg(sockfd, &mh, flags);
  return_if_fail (recvlen >= 0) recvlen;
  if likely (src_addr != NULL && addrlen != NULL) {
    socklen_t src_addr_len = src_addr_buf.sa_family == AF_INET ?
      sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    memcpy(src_addr, &src_addr_buf, min(*addrlen, src_addr_len));
    *addrlen = src_addr_len;
  }
  return_if_fail (
    dst_addr != NULL || (src_addr_buf.sa_family == AF_INET && recv_dst != NULL)
  ) recvlen;

  if likely (dst_addr != NULL) {
    dst_addr->sa_port = 0;
  }
  for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&mh); cmsg != NULL;
       cmsg = CMSG_NXTHDR(&mh, cmsg)) {
    if (cmsg->cmsg_level == IPPROTO_IP) {
      if (cmsg->cmsg_type == IP_PKTINFO) {
        struct in_pktinfo *pi = (void *) CMSG_DATA(cmsg);
        if likely (dst_addr != NULL) {
          dst_addr->v4.sin_family = AF_INET;
          dst_addr->v4.sin_addr = pi->ipi_spec_dst;
          dst_addr->sa_scope_id = pi->ipi_ifindex;
        }
        if (recv_dst != NULL) {
          *recv_dst = pi->ipi_addr;
        }
        // that's enough
        break;
      } else if (cmsg->cmsg_type == IP_ORIGDSTADDR) {
        struct sockaddr_in *pi = (void *) CMSG_DATA(cmsg);
        if likely (dst_addr != NULL) {
          dst_addr->v4.sin_family = pi->sin_family;
          dst_addr->v4.sin_port = pi->sin_port;
          dst_addr->v4.sin_addr = pi->sin_addr;
          // do not copy zeros
        }
        // wait ipi_ifindex and ipi_spec_dst
      }
    } else if (cmsg->cmsg_level == IPPROTO_IPV6) {
      if (cmsg->cmsg_type == IPV6_PKTINFO) {
        struct in6_pktinfo *pi = (void *) CMSG_DATA(cmsg);
        if likely (dst_addr != NULL) {
          dst_addr->v6.sin6_family = AF_INET6;
          dst_addr->v6.sin6_flowinfo = 0;
          dst_addr->v6.sin6_addr = pi->ipi6_addr;
          dst_addr->v6.sin6_scope_id = pi->ipi6_ifindex;
        }
        break;
      } else if (cmsg->cmsg_type == IPV6_ORIGDSTADDR) {
        struct sockaddr_in6 *pi = (void *) CMSG_DATA(cmsg);
        if likely (dst_addr != NULL) {
          dst_addr->v6 = *pi;
        }
        break;
      }
    }
  }
  return recvlen;
}
