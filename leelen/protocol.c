#include <netinet/in.h>

#include <inet46i/endian.h>

#include "protocol.h"


const struct in_addr leelen_discovery_groupaddr = {
  .s_addr = lhtonl(LEELEN_DISCOVERY_GROUPADDR)
};
