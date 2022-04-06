#ifndef INET46I_SOCKET46_H
#define INET46I_SOCKET46_H

#ifdef __cplusplus
extern "C" {
#endif

// #include <sys/socket.h>
struct sockaddr;

// #include "sockaddr46.h"
union sockaddr_in46;

/**
 * @file
 * Helper functions to quickly create sockets.
 */


/// set @c SO_REUSEADDR before @c bind()
#define OPENADDR_REUSEADDR (1 << 0)
/// set @c SO_REUSEPORT before @c bind()
#define OPENADDR_REUSEPORT (1 << 1)
/// set @c IPV6_V6ONLY before @c bind()
#define OPENADDR_V6ONLY (1 << 2)


__attribute__((nonnull, warn_unused_result, access(read_only, 1)))
/**
 * @brief Open socket, possibly bind to an interface.
 *
 * @param addr Address to bind to.
 * @param type Socket type.
 * @param flags Flags.
 * @param scope_id Interface scope ID, or 0 if do not bind to a specific
 *  interface.
 * @return Socket file descriptor, or -1 on error.
 */
int openaddrz (
  const struct sockaddr * __restrict addr, int type, int flags,
  unsigned int scope_id);
__attribute__((nonnull, warn_unused_result, access(read_only, 1)))
/**
 * @brief Open socket.
 *
 * @note If @p addr->sa_family is @c AF_INET6, will bind to interface specified
 *  by @p addr->sin6_scope_id.
 *
 * @param addr Address to bind to.
 * @param type Socket type.
 * @param flags Flags.
 * @return Socket file descriptor, or -1 on error.
 */
int openaddr (const struct sockaddr * __restrict addr, int type, int flags);
__attribute__((nonnull, warn_unused_result, access(read_only, 1)))
/**
 * @memberof sockaddr_in46
 * @brief Open socket, and bind to interface specified by @p addr->sa_scope_id.
 *
 * @param addr Address to bind to.
 * @param type Socket type.
 * @param flags Flags.
 * @return Socket file descriptor, or -1 on error.
 */
int openaddr46 (
  const union sockaddr_in46 * __restrict addr, int type, int flags);


#ifdef __cplusplus
}
#endif

#endif /* INET46I_SOCKET46_H */
