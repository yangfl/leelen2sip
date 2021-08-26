#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <inet46i/in46.h>
#include <inet46i/inet46.h>
#include <inet46i/sockaddr46.h>

#include "macro.h"
#include "arg.h"


int argtoi (
    const char s[], int *res, int min, int max,
    const char option[], const char reason[]) {
  char *s_end;
  *res = strtol(s, &s_end, 10);
  int ret = *s_end == '\0' && min <= *res && *res <= max ? 0 : 1;
  if unlikely (ret && option) {
    fprintf(stderr, "error when parsing %s: ", option);
    fprintf(stderr,
            *s_end != '\0' ? "'%s' not a number\n" :
            reason != NULL ? reason :
            "'%s' out of range\n", s);
  }
  return ret;
}


int argtoport (
    const char s[], in_port_t *res, bool allow_unspec, bool network_endian,
    const char option[], const char reason[]) {
  int port;
  int ret = argtoi(
    s, &port, !allow_unspec, 65535, option, reason == NULL ?
      "'%s' is not a valid port number\n" : reason);
  *res = network_endian ? htons(port) : port;
  return ret;
}


int argtoifaceip (const char s[], struct in46i_addr *addr) {
  addr->sa_family = inet_aton46i(s, addr);
  should (!IN46I_IS_UNSPEC(addr)) otherwise {
    fprintf(
      stderr, "error: cannot parse '%s' as an interface or address\n", s);
    return 1;
  }
  return 0;
}


int argtosockaddr (const char s[], union sockaddr_in46 *addr) {
  int domain = sockaddr46_aton(s, addr);
  should (domain != AF_UNSPEC) otherwise {
    fprintf(
      stderr, "error: cannot parse '%s' as an interface or address\n", s);
  }
  return domain;
}
