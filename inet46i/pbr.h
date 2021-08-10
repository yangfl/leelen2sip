#ifndef INET46I_PBR_H
#define INET46I_PBR_H

#include <stdbool.h>
#include <stddef.h>
#include <netinet/in.h>

#include "sockaddr46.h"


struct Socket5Tuple {
  union sockaddr_in46 src;
  union sockaddr_in46 dst;
};


struct SockaddrPolicy {
  union {
    struct in6_addr network6;
    struct {
      uint32_t __u32_network_padding[3];
      struct in_addr network;
    };
  };
  uint32_t scope_id;
  union {
    in_port_t port;
    in_port_t port_min;
  };
  in_port_t port_max;
  short prefix;  // may be -1
};

__attribute__((nonnull, pure, warn_unused_result, access(read_only, 1),
               access(read_only, 2)))
bool sockaddrpr_match (
  const struct SockaddrPolicy *policy, const union sockaddr_in46 *addr);


typedef int (*SocketPolicyRouteFunc) ();
typedef int (*SocketPolicyRouteStreamFunc) (
  void *context, int socket, struct Socket5Tuple *conn);
typedef int (*SocketPolicyRouteDgramFunc) (
  void *context, void *buf, int buflen, int socket,
  struct Socket5Tuple *conn, void *spec_dst);

struct SocketPolicyRoute {
  struct SocketPolicy {
    struct SockaddrPolicy src;
    struct SockaddrPolicy dst;
  };
  SocketPolicyRouteFunc func;
  void *context;
};

__attribute__((nonnull, pure, warn_unused_result, access(read_only, 1),
               access(read_only, 2)))
struct SocketPolicyRoute *socketpr_resolve (
  const struct SocketPolicyRoute *routes, const struct Socket5Tuple *conn);


struct SocketPBR {
  union {
    // multiple routes
    const struct SocketPolicyRoute *routes;
    // single route
    struct {
      SocketPolicyRouteFunc func;
      void *context;
    };
  };
  bool single_route;
  bool dst_required;
  int nothing_ret;
  union {
    // only for TCP
    union sockaddr_in46 dst_addr;
    // only for UDP
    struct {
      unsigned short maxlen;
      in_port_t dst_port;
    };
  };
};

__attribute__((nonnull, access(read_only, 2)))
int socketpbr_accept4 (int socket, const struct SocketPBR *pbr, int flags);
__attribute__((nonnull, access(read_only, 2)))
int socketpbr_accept (int socket, const struct SocketPBR *pbr);
__attribute__((nonnull, access(read_only, 2)))
int socketpbr_recv (int socket, const struct SocketPBR *pbr, int flags);
__attribute__((nonnull, access(read_only, 2)))
int socketpbr_read (int socket, const struct SocketPBR *pbr);
__attribute__((nonnull))
int socketpbr_set (int socket, struct SocketPBR *pbr, int af);


#endif /* INET46I_PBR_H */
