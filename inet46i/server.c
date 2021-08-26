#include <poll.h>
#include <stddef.h>
#include <sys/socket.h>

#include "private/macro.h"
#include "server.h"


int pollserver_recvall (void *context, int socket) {
  (void) context;
  unsigned char buf[1500];
  while (recv(socket, buf, sizeof(buf), MSG_DONTWAIT) > 0) { }
  return 0;
}


static int pollserver_call (const struct pollservercb * __restrict cb) {
  return_if_fail (cb->func != (pollserverfunc) NULL) 0;
  int ret;
  switch (cb->type & POLLSERVER_FUNC_ARGS) {
    default:
    case POLLSERVER_FUNC_CONTEXT_SOCKET:
      ret = ((int (*) (void *, int)) cb->func)(cb->context, cb->fd);
      break;
    case POLLSERVER_FUNC_SOCKET_CONTEXT:
      ret = ((int (*) (int, void *)) cb->func)(cb->fd, cb->context);
      break;
    case POLLSERVER_FUNC_CB:
      ret = ((int (*) (const void *)) cb->func)(cb);
      break;
  }
  return (cb->type & POLLSERVER_FUNC_RETURNS) == POLLSERVER_FUNC_VOID ? 0 : ret;
}


int pollserver (
    const struct pollservercb * __restrict cbs, int nfds, int timeout,
    const struct pollservercb * __restrict timeout_cb) {
  return_if_fail (nfds > 0) 255;

  struct pollfd pollfds[nfds];
  for (int i = 0; i < nfds; i++) {
    pollfds[i].fd = cbs[i].fd;
    pollfds[i].events = POLLIN;
  };

  while (1) {
    int pollres = poll(pollfds, nfds, timeout);
    should (pollres >= 0) otherwise {
      if (timeout < 0) {
        return_if_not (timeout_cb != NULL && timeout_cb->func != NULL) 1;
      }
      return_nonzero (pollserver_call(timeout_cb));
    }

    for (int i = 0; i < nfds; i++) {
      continue_if (pollfds[i].revents == 0);
      int res = pollserver_call(cbs + i);
      switch (cbs[i].type & POLLSERVER_FUNC_RETURNS) {
        case POLLSERVER_FUNC_FATAL_NEG:
          return_if_fail (res >= 0) res;
          break;
        case POLLSERVER_FUNC_FATAL_NONZERO:
          return_if_fail (res == 0) res;
          break;
      }
    }
  }

  return 0;
}
