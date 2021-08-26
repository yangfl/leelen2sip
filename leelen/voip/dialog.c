#include <endian.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <inet46i/sockaddr46.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "utils/timeval.h"
#include "../config.h"
#include "message.h"
#include "protocol.h"
#include "dialog.h"


// Update state after receiving code AND acking it
static enum LeelenDialogState LeelenDialogState_receive (
    enum LeelenDialogState state, enum LeelenCode code) {
  switch (code) {
    case LEELEN_CODE_CALL:
    case LEELEN_CODE_VIEW:
    case LEELEN_CODE_VOICE_MESSAGE:
    case LEELEN_CODE_ACCEPTED:
      return LEELEN_DIALOG_CONNECTED;
    case LEELEN_CODE_BYE:
      return LEELEN_DIALOG_DISCONNECTED;
    default:
      return state;
  }
}


// Update state after sending code
static enum LeelenDialogState LeelenDialogState_send (
    enum LeelenDialogState state, enum LeelenCode code) {
  switch (code) {
    case LEELEN_CODE_CALL:
    case LEELEN_CODE_VIEW:
    case LEELEN_CODE_VOICE_MESSAGE:
    case LEELEN_CODE_ACCEPTED:
      return LEELEN_DIALOG_CONNECTING;
    case LEELEN_CODE_BYE:
      return LEELEN_DIALOG_DISCONNECTING;
    default:
      return state;
  }
}


// Update state after getting ack of last code from peer
static enum LeelenDialogState LeelenDialogState_ack (
    enum LeelenDialogState state) {
  switch (state) {
    case LEELEN_DIALOG_CONNECTING:
      return LEELEN_DIALOG_CONNECTED;
    case LEELEN_DIALOG_DISCONNECTING:
      return LEELEN_DIALOG_DISCONNECTED;
    default:
      return state;
  }
}


// Update state after NOT getting ack of last code from peer
static enum LeelenDialogState LeelenDialogState_nak (
    enum LeelenDialogState state) {
  switch (state) {
    case LEELEN_DIALOG_CONNECTING:
      return LEELEN_DIALOG_DISCONNECTED;
    case LEELEN_DIALOG_DISCONNECTING:
      return LEELEN_DIALOG_CONNECTED;
    default:
      return state;
  }
}


bool LeelenDialog_check_timeout (struct LeelenDialog *self) {
  do {
    break_if (self->last_sent.tv_sec == 0);
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    break_if_not (timeout_reached(
      now.tv_sec, now.tv_nsec,
      self->last_sent.tv_sec, self->last_sent.tv_nsec,
      LEELEN_VOIP_TIMEOUT * 2 / 1000,
      (LEELEN_VOIP_TIMEOUT * 2 * 1000000) % 1000000000));
    self->last_sent.tv_sec = 0;
    self->state = LeelenDialogState_nak(self->state);
  } while (0);
  return self->last_sent.tv_sec == 0;
}


int LeelenDialog_sendmsg (
    struct LeelenDialog *self, void *buf, unsigned int len, int sockfd) {
  // log
  if (LOG_WOULD_LOG(LOG_LEVEL_VERBOSE)) {
    char s_dst[SOCKADDR_STRLEN];
    sockaddr_toa(&self->theirs.sock, s_dst, sizeof(s_dst));
    LOG(
      LOG_LEVEL_VERBOSE,
      "To %s, dialog " PRI_LEELEN_ID ", code " PRI_LEELEN_CODE ":\n%.*s",
      s_dst, LEELEN_MESSAGE_ID(buf), le32toh(LEELEN_MESSAGE_CODE(buf)),
      len - LEELEN_MESSAGE_HEADER_SIZE,
      (char *) buf + LEELEN_MESSAGE_HEADER_SIZE);
  }
  // send
  return_if_fail (sendto(
    sockfd, buf, len, 0, &self->theirs.sock,
    self->theirs.sa_family == AF_INET ?
      sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) == len) -1;
  // update state
  enum LeelenCode code = le32toh(LEELEN_MESSAGE_CODE(buf));
  if (code != LEELEN_CODE_OK) {
    self->state = LeelenDialogState_send(self->state, code);
    timespec_get(&self->last_sent, TIME_UTC);
  }
  return 0;
}


int LeelenDialog_sendcode (
    struct LeelenDialog *self, unsigned int code, int sockfd) {
  char buf[LEELEN_MESSAGE_MIN_SIZE];
  struct LeelenMessage message = {
    .id = self->id, .code = code,
    .from = self->from, .from_type = self->from_type, .to = self->to,
  };
  int len = LeelenMessage_tostring(&message, buf, sizeof(buf)) + 1;
  return LeelenDialog_sendmsg(self, buf, len, sockfd);
}


int LeelenDialog_may_bye (struct LeelenDialog *self, int sockfd) {
  return_if_fail (self->id != 0) 1;
  return_if_fail (self->state == LEELEN_DIALOG_DISCONNECTED ||
                  self->state == LEELEN_DIALOG_DISCONNECTING) 1;
  return LeelenDialog_sendcode(self, LEELEN_CODE_BYE, sockfd);
}


int LeelenDialog_ack (struct LeelenDialog *self, int sockfd) {
  return LeelenDialog_sendcode(self, LEELEN_CODE_OK, sockfd);
}


int LeelenDialog_send (
    struct LeelenDialog *self, unsigned int code, int sockfd,
    char * const *audio_formats, char * const *video_formats) {
  char buf[self->mtu];
  struct LeelenMessage message = {
    .id = self->id, .code = code,
    .from = self->from, .from_type = self->from_type, .to = self->to,
    .audio_formats = (char **) audio_formats,
    .video_formats = (char **) video_formats,
    .audio_port = self->our_audio_port, .video_port = self->our_video_port,
  };
  unsigned int len = LeelenMessage_tostring(&message, buf, sizeof(buf)) + 1;
  should (len <= sizeof(buf)) otherwise {
    LOG(LOG_LEVEL_WARNING, "Message too long, want %d bytes, got %ld bytes",
        len + 1, sizeof(buf));
    len = sizeof(buf);
    buf[len - 1] = '\0';
  }
  return LeelenDialog_sendmsg(self, buf, len, sockfd);
}


int LeelenDialog_receive (
    struct LeelenDialog *self, char *msg, int sockfd,
    char ***audio_formats, char ***video_formats) {
  return_if_fail (LEELEN_MESSAGE_ID(msg) == self->id) 255;

  enum LeelenCode code = le32toh(LEELEN_MESSAGE_CODE(msg));
  if (code == LEELEN_CODE_OK) {
    return_if_fail (!LeelenDialog_check_timeout(self)) 254;
    self->state = LeelenDialogState_ack(self->state);
    if (audio_formats != NULL) {
      *audio_formats = NULL;
    };
    if (video_formats != NULL) {
      *video_formats = NULL;
    };
    return 0;
  }

  // parse sdp
  struct LeelenMessage message;
  return_nonzero (LeelenMessage_init(
    &message, msg, audio_formats != NULL, video_formats != NULL));

  // verify
  should (strcmp(self->to.str, message.from.str) == 0) otherwise {
    LOG(LOG_LEVEL_INFO, "Expect their number %s, got %s",
        self->to.str, message.from.str);
    self->to = message.from;
  }
  if (self->to_type == 0) {
    self->to_type = message.from_type;
  } else {
    should (self->to_type == message.from_type) otherwise {
      LOG(LOG_LEVEL_INFO, "Expect their device type %d, got %d",
          self->to_type, message.from_type);
      self->to_type = message.from_type;
    }
  }
  should (strcmp(self->from.str, message.to.str) == 0) otherwise {
    LOG(LOG_LEVEL_INFO, "Expect our number %s, got %s",
        self->from.str, message.to.str);
    // do not change self->from
  }
  if (self->their_audio_port == 0) {
    self->their_audio_port = message.audio_port;
  } else {
    should (self->their_audio_port == message.audio_port) otherwise {
      LOG(LOG_LEVEL_INFO, "Expect audio port %d, got %d",
          self->their_audio_port, message.audio_port);
      self->their_audio_port = message.audio_port;
    }
  }
  if (self->their_video_port == 0) {
    self->their_video_port = message.video_port;
  } else {
    should (self->their_video_port == message.video_port) otherwise {
      LOG(LOG_LEVEL_INFO, "Expect video port %d, got %d",
          self->their_video_port, message.video_port);
      self->their_video_port = message.video_port;
    }
  }

  // ack
  int ret = LeelenDialog_sendcode(self, LEELEN_CODE_OK, sockfd);
  should (ret == 0) otherwise {
    int saved_errno = errno;
    LeelenMessage_destroy(&message);
    errno = saved_errno;
    return ret;
  }

  // update state
  self->state = LeelenDialogState_receive(self->state, code);

  // return
  if (audio_formats != NULL) {
    *audio_formats = message.audio_formats;
  }
  if (video_formats != NULL) {
    *video_formats = message.video_formats;
  }
  return 0;
}


int LeelenDialog_init (
    struct LeelenDialog *self, const struct LeelenConfig *config,
    const struct sockaddr *theirs, const struct LeelenNumber *to,
    leelen_id_t id) {
  self->id = id;
  while unlikely (self->id == 0) {
    self->id = rand();
  }

  self->from = config->number;
  self->from_type = config->type;
  if unlikely (to == NULL) {
    self->to.str[0] = '\0';
  } else {
    self->to = *to;
  }
  self->to_type = LEELEN_DEVICE_UNKNOWN;

  self->ours = config->addr;
  self->theirs = *(union sockaddr_in46 *) theirs;
  self->theirs.sa_port = htons(config->voip);
  self->our_audio_port = config->audio;
  self->our_video_port = config->video;
  self->their_audio_port = 0;
  self->their_video_port = 0;

  self->mtu = config->mtu;

  self->state = LEELEN_DIALOG_DISCONNECTED;
  self->userdata = NULL;
  return 0;
}
