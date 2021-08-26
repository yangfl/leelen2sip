#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>  // osip
#include <sys/socket.h>

#include <inet46i/sockaddr46.h>
#include <inet46i/socket46.h>
#include <osip2/osip.h>
#include <osip2/osip_dialog.h>

#include "utils/macro.h"
#include "leelen/config.h"
#include "leelen/voip/dialog.h"
#include "forwarder.h"
#include "sipleelen.h"
#include "session.h"


int SIPLeelenSession_connect (
    struct SIPLeelenSession *self, in_port_t *audio, in_port_t *video) {
  if (audio != NULL && (
      self->leelen.our_audio_port == 0 || self->leelen.their_audio_port == 0)) {
    *audio = 0;
    audio = NULL;
  }
  if (video != NULL && (
      self->leelen.our_video_port == 0 || self->leelen.their_video_port == 0)) {
    *video = 0;
    video = NULL;
  }
  return_if_fail (audio != NULL || video != NULL) 0;

  union sockaddr_in46 sip_ours = self->device->addr;
  sip_ours.sa_port = 0;
  union sockaddr_in46 leelen_ours = self->leelen.ours;
  union sockaddr_in46 leelen_theirs = self->leelen.theirs;

  if (audio != NULL) {
    if likely (self->audio.socket1 < 0) {
      leelen_ours.sa_port = htons(self->leelen.our_audio_port);
      int sockfd = openaddr46(
        &leelen_ours, SOCK_DGRAM, OPENADDR_REUSEADDR);
      return_if_fail (sockfd >= 0) 1;
      leelen_theirs.sa_port = htons(self->leelen.their_audio_port);
      should (connect(
          sockfd, &leelen_theirs.sock, sizeof(leelen_theirs)) == 0) otherwise {
        int saved_errno = errno;
        close(sockfd);
        errno = saved_errno;
        return 1;
      };
      self->audio.socket1 = sockfd;
    }

    if likely (self->audio.socket2 < 0) {
      int sockfd = openaddr46(&sip_ours, SOCK_DGRAM, OPENADDR_REUSEADDR);
      return_if_fail (sockfd >= 0) 1;
      self->audio.socket2 = sockfd;
    }

    union sockaddr_in46 addr;
    socklen_t addrlen = sizeof(addr);
    return_if_fail (getsockname(
      self->audio.socket2, &addr.sock, &addrlen) == 0) 2;
    *audio = ntohs(addr.sa_port);
  }

  if (video != NULL) {
    if likely (self->video.socket1 < 0) {
      leelen_ours.sa_port = htons(self->leelen.our_video_port);
      int sockfd = openaddr46(
        &leelen_ours, SOCK_DGRAM, OPENADDR_REUSEADDR);
      return_if_fail (sockfd >= 0) 3;
      leelen_theirs.sa_port = htons(self->leelen.their_video_port);
      should (connect(
          sockfd, &leelen_theirs.sock, sizeof(leelen_theirs)) == 0) otherwise {
        int saved_errno = errno;
        close(sockfd);
        errno = saved_errno;
        return 3;
      };
      self->video.socket1 = sockfd;
    }

    if likely (self->video.socket2 < 0) {
      int sockfd = openaddr46(&sip_ours, SOCK_DGRAM, OPENADDR_REUSEADDR);
      return_if_fail (sockfd >= 0) 4;
      self->video.socket2 = sockfd;
    }

    union sockaddr_in46 addr;
    socklen_t addrlen = sizeof(addr);
    return_if_fail (getsockname(
      self->video.socket2, &addr.sock, &addrlen) == 0) 4;
    *video = ntohs(addr.sa_port);
  }

  return 0;
}


void SIPLeelenSession_destroy (struct SIPLeelenSession *self) {
  single_stop(&self->invite_state);

  LeelenDialog_destroy(&self->leelen);
  if (self->sip != NULL) {
    osip_dialog_free(self->sip);
  }
  // do not free self->transaction
  mtx_destroy(&self->mtx);

  Forwarder_destroy(&self->audio);
  Forwarder_destroy(&self->video);

  single_join(&self->invite_state);
}


int SIPLeelenSession_init (
    struct SIPLeelenSession *self, const struct SIPLeelen *device) {
  return_if_fail (mtx_init(&self->mtx, mtx_plain) == thrd_success) -1;

  self->device = device;
  self->leelen.id = 0;
  self->sip = NULL;
  self->transaction = NULL;

  int mtu = min(device->mtu, device->config->mtu);
  Forwarder_init(&self->audio, mtu);
  Forwarder_init(&self->video, mtu);

  self->invite_state = SINGLE_FLAG_INIT;
  return 0;
}
