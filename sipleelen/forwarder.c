#include <poll.h>
#include <stdatomic.h>
#include <threads.h>
#include <unistd.h>
#include <sys/socket.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "utils/single.h"
#include "utils/threadname.h"
#include "forwarder.h"


/**
 * @memberof Forwarder
 * @private
 * @brief Forwarder main thread.
 *
 * @param arg Socket forwarder.
 * @return 0.
 */
static int Forwarder_mainloop (void *arg) {
  struct Forwarder *self = arg;
  threadname_format("%d <=> %d", self->socket1, self->socket2);

  struct pollfd pollfds[2] = {
    {.fd = self->socket1, .events = POLLIN},
    {.fd = self->socket2, .events = POLLIN},
  };

  while (single_continue(&self->state)) {
    int pollres = poll(pollfds, 2, 1000);
    should (pollres >= 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "poll() failed");
      continue;
    }
    continue_if (pollres <= 0);

    for (int i = 0; i < 2; i++) {
      continue_if (pollfds[i].revents == 0);
      unsigned char buf[self->mtu];
      int buflen = recv(pollfds[i].fd, buf, sizeof(buf), MSG_DONTWAIT);
      should (buflen >= 0) otherwise {
        LOG_PERROR(LOG_LEVEL_WARNING, "recv() failed");
        continue;
      }
      should (send(pollfds[!i].fd, buf, buflen, 0) == buflen) otherwise {
        LOG_PERROR(LOG_LEVEL_WARNING, "send() failed");
        continue;
      }
    }
  }

  return 0;
}


int Forwarder_start (struct Forwarder *self) {
  return_if_fail (self->socket1 >= 0 && self->socket2 >= 0) 255;
  return single_start(&self->state, Forwarder_mainloop, self);
}


void Forwarder_destroy (struct Forwarder *self) {
  Forwarder_stop(self);
  if likely (self->socket1 >= 0) {
    close(self->socket1);
  }
  if likely (self->socket2 >= 0) {
    close(self->socket2);
  }
  single_join(&self->state);
}
