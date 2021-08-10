#ifndef INET46I_SOCKET46_H
#define INET46I_SOCKET46_H

#include "sockaddr46.h"


__attribute__((warn_unused_result, access(read_only, 1)))
int bindport (const union sockaddr_in46 *addr, int type);


#endif /* INET46I_SOCKET46_H */
