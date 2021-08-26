#include <stdbool.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <inet46i/endian.h>
#include <inet46i/sockaddr46.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "../family.h"
#include "protocol.h"


const struct in_addr leelen_discovery_groupaddr = {
  .s_addr = lhtonl(LEELEN_DISCOVERY_GROUPADDR)};
const struct in6_addr leelen_discovery_groupaddr6 = {{{
  0xf,0xf,0,2,0,0,0,0,0,0,0,0,0,0,0,1}}};


void Leelen_discovery_logger (
    const char *msg, const struct sockaddr *src, const void *dst,
    int ifindex, bool should_reply) {
  LOGEVENT (should_reply ? LOG_LEVEL_INFO : LOG_LEVEL_DEBUG) {
    LOGEVENT_PUTS("[");

    char s_dst[INET6_ADDRSTRLEN];
    if (dst == NULL) {
      strcpy(s_dst, "?");
    } else {
      inet_ntop(src->sa_family, dst, s_dst, sizeof(s_dst));
    }
    LOGEVENT_PUTS(s_dst);

    if (ifindex != 0) {
      LOGEVENT_PUTS("%");
      char ifname[IF_NAMESIZE];
      if_indextoname(ifindex, ifname);
      LOGEVENT_PUTS(ifname);
    }

    char s_src[SOCKADDR_STRLEN];
    sockaddr_toa(src, s_src, sizeof(s_src));
    if (Leelen_discovery_is_advertisement(msg)) {
      LOGEVENT_LOG("] Advertisement from %s: %s", s_src, msg);
    } else {
      LOGEVENT_LOG("] Who is %s? Tell %s", msg, s_src);
    }
  }
}
