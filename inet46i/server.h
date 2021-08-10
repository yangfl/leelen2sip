#ifndef INET64_SERVER_H
#define INET64_SERVER_H


typedef int (*pollserverfunc) ();

enum pollserverfunctype {
  POLLSERVER_FUNC_ARGS = 3,
  POLLSERVER_FUNC_CONTEXT_SOCKET = 0,
  POLLSERVER_FUNC_SOCKET_CONTEXT = 1,
  POLLSERVER_FUNC_CB = 2,
  // POLLSERVER_FUNC_UNUSED = 3,

  POLLSERVER_FUNC_RETURNS = 3 << 2,
  POLLSERVER_FUNC_FATAL_NONE = 0 << 2,
  POLLSERVER_FUNC_VOID = 1 << 2,
  POLLSERVER_FUNC_FATAL_NEG = 2 << 2,
  POLLSERVER_FUNC_FATAL_NONZERO = 3 << 2,
};

struct pollservercb {
  int fd;
  pollserverfunc func;
  void *context;
  unsigned char type;
};

__attribute__((access(read_only, 1)))
int pollserver_recvall (void *context, int socket);
__attribute__((nonnull(1), access(read_only, 1), access(read_only, 4)))
int pollserver (
  const struct pollservercb cbs[], int nfds, int timeout,
  const struct pollservercb *timeout_cb);


#endif /* INET64_SERVER_H */
