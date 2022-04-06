#include <poll.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>
#include <sys/socket.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "utils/single.h"
#include "utils/threadname.h"
#include "forwarder.h"


struct HalfForwarder {
  single_flag *state;
  int from;
  int to;
  unsigned short mtu;
};


/**
 * @memberof HalfForwarder
 * @brief HalfForwarder main thread.
 *
 * @param arg Socket unidirectional forwarder.
 * @return 0.
 */
static int HalfForwarder_mainloop (void *arg) {
  struct HalfForwarder *self = arg;
  single_flag *state = self->state;
  unsigned char buf[self->mtu];
  int from = self->from;
  int to = self->to;
  free(self);

  threadname_format("%d => %d", from, to);

  struct pollfd pollfd = {.fd = from, .events = POLLIN};

  while (single_continue(state)) {
    int pollres = poll(&pollfd, 1, 1000);
    should (pollres >= 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "poll() failed");
      continue;
    }
    continue_if (pollres <= 0);

    int buflen = recv(from, buf, sizeof(buf), MSG_DONTWAIT);
    should (buflen >= 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "recv() failed");
      continue;
    }
    should (send(to, buf, buflen, 0) == buflen) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "send() failed");
      continue;
    }
  }

  return 0;
}


int Forwarder_start (struct Forwarder *self) {
  return_if_fail (self->socket1 >= 0 && self->socket2 >= 0) 255;

  return_if_fail (single_acquire(&self->state)) 0;

  for (int i = 0; i < 2; i++) {
    struct HalfForwarder *half = malloc(sizeof(struct HalfForwarder));
    goto_if_fail (half != NULL) fail;
    half->state = &self->state;
    if (i == 0) {
      half->from = self->socket1;
      half->to = self->socket2;
    } else {
      half->from = self->socket2;
      half->to = self->socket1;
    }
    half->mtu = self->mtu;
    should (thrd_execute(
        HalfForwarder_mainloop, half) == thrd_success) otherwise {
      free(half);
      goto fail;
    }
  }
  return 0;

fail:
  single_finish(&self->state);
  return -1;
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
