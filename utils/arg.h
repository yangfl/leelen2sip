#ifndef UTILS_ARG_H
#define UTILS_ARG_H

#include <stdbool.h>
#include <netinet/in.h>

// #include <inet46i/in46.h>
struct in46i_addr;


__attribute__((
  nonnull(1, 2), warn_unused_result, access(read_only, 1),
  access(write_only, 2), access(read_only, 5), access(read_only, 6)))
int argtoi (
  const char s[], int *res, int min, int max,
  const char option[], const char reason[]);
__attribute__((
  nonnull(1, 2), warn_unused_result, access(read_only, 1),
  access(write_only, 2), access(read_only, 5), access(read_only, 6)))
int argtoport (
  const char s[], in_port_t *res, bool allow_unspec, bool network_endian,
  const char option[], const char reason[]);
__attribute__((nonnull, warn_unused_result,
               access(read_only, 1), access(write_only, 2)))
int argtoifaceip (const char s[], struct in46i_addr *addr);
__attribute__((nonnull, warn_unused_result,
               access(read_only, 1), access(write_only, 2)))
int argtosockaddr (const char s[], union sockaddr_in46 *addr);


#endif /* UTILS_ARG_H */
