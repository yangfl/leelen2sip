#ifndef INET64_SERVER_H
#define INET64_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Server that `poll()` multiple sockets at one time.
 */


/**
 * @memberof pollservercb
 * @brief base type of pollservercb::func
 */
typedef int (*pollserverfunc) ();

/**
 * @brief Argument and return type of ::pollserverfunc.
 */
enum pollserverfunctype {
  /// argument type mask
  POLLSERVER_FUNC_ARGS = 7,
  /// `int (void *, int)`
  POLLSERVER_FUNC_CONTEXT_SOCKET = 0,
  /// `int (int, void *)`
  POLLSERVER_FUNC_SOCKET_CONTEXT = 1,
  /// `int (struct pollservercb *)`
  POLLSERVER_FUNC_CB = 2,
  // POLLSERVER_FUNC_UNUSED = 7,

  /// return type mask
  POLLSERVER_FUNC_RETURNS = 3 << 3,
  /// no return value is fatal; just pass it
  POLLSERVER_FUNC_FATAL_NONE = 0 << 3,
  /// no return value is fatal; ignore it
  POLLSERVER_FUNC_VOID = 1 << 3,
  /// negative return value is fatal
  POLLSERVER_FUNC_FATAL_NEG = 2 << 3,
  /// non-zero return value is fatal
  POLLSERVER_FUNC_FATAL_NONZERO = 3 << 3,
};

/**
 * @brief Socket polling descriptor.
 */
struct pollservercb {
  /// socket file descriptor
  int fd;
  /// process function
  pollserverfunc func;
  /// user data for pollservercb::func
  void *context;
  /// argument and return type of pollservercb::func, see #pollserverfunctype
  unsigned char type;
};

__attribute__((access(read_only, 1)))
/**
 * @relates pollservercb
 * @brief Exhaust the socket read buffer. Used as dummy pollservercb::func.
 *
 * @param context Not used.
 * @param socket Socket to read.
 * @return 0.
 */
int pollserver_recvall (void *context, int socket);
__attribute__((nonnull(1), access(read_only, 1, 2), access(read_only, 4)))
/**
 * @relates pollservercb
 * @brief @c poll() multiple sockets at one time.
 *
 * @param cbs Socket polling descriptors.
 * @param nfds Number of descriptors.
 * @param timeout @c poll() timeout in milliseconds. Negative value means no
 *  timeout.
 * @param timeout_cb Callback to be called if @c poll() times out. Can be
 *  @c NULL.
 * @return 0 on success, error otherwise. If timeout happens and @p timeout_cb
 *  is not @c NULL, return the return value of calling @p timeout_cb->func.
 */
int pollserver (
  const struct pollservercb * __restrict cbs, int nfds, int timeout,
  const struct pollservercb * __restrict timeout_cb);


#ifdef __cplusplus
}
#endif

#endif /* INET64_SERVER_H */
