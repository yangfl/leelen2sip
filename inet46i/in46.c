#include <netinet/in.h>

#include "in46.h"


extern inline int in6_normalize (struct in6_addr *addr);
extern inline int in_to6 (void *addr);
extern inline int in6_to4 (void *addr);
extern inline bool in46_is_addr_unspecified (int af, const void *addr);
extern inline int in64_set (int af, void *dst, const void *src);
extern inline void *in64_get (int af, const void *addr);
