#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>  // osip
#include <sys/socket.h>

#include <inet46i/sockaddr46.h>
#include <inet46i/socket46.h>
#include <osip2/osip.h>
#include <osip2/osip_dialog.h>

#include "utils/macro.h"
#include "utils/log.h"
#include "utils/osip.h"
#include "utils/refcount.h"
#include "leelen/config.h"
#include "leelen/voip/dialog.h"
#include "forwarder.h"
#include "sipleelen.h"
#include "transaction.h"
#include "session.h"


bool SIPLeelenSession_established (struct SIPLeelenSession *self) {
  if (self->leelen.id != 0) {
    LeelenDialog_ack_timeout(&self->leelen);
    return_if (self->leelen.state != LEELEN_DIALOG_DISCONNECTED) true;
  }
  if (self->sip != NULL) {
    return_if (self->sip->state != DIALOG_CLOSE) true;
  }
  return false;
}


int SIPLeelenSession_bye_leelen (struct SIPLeelenSession *self) {
  return_if (likely (LeelenDialog_may_bye(
    &self->leelen, self->device->socket_leelen) >= 0)) 0;
  self->leelen.state = LEELEN_DIALOG_DISCONNECTED;
  self->leelen.last_activity = time(NULL);
  return -1;
}


int SIPLeelenSession_bye_sip (struct SIPLeelenSession *self, bool now) {
  return_if_fail (self->sip != NULL) 255;

  osip_message_t *request;
  return_if_fail (osip_message_request(
    &request, self->sip, "BYE", self->device->ua) == OSIP_SUCCESS) -1;
  osip_event_t *event = osip_new_outgoing_sipmessage(request);
  should (event != NULL) otherwise {
    osip_message_free(request);
    return -1;
  }
  osip_transaction_t *tr = osip_create_transaction(self->device->osip, event);
  should (tr != NULL) otherwise {
    osip_event_free(event);
    return -1;
  }

  LOG(LOG_LEVEL_DEBUG, "Transaction %d: Create %s transaction for "
      PRI_LEELEN_ID, tr->transactionid, osip_fsm_type_names[tr->ctx_type],
      self->leelen.id);

  osip_transaction_t *old_tr = NULL;
  atomic_compare_exchange_strong(&self->transaction, &old_tr, tr);

  tr->your_instance = (void *) self->device;
  tr->out_socket = self->device->socket_sip;
  tr->reserved1 = self;
  SIPTransactionData_get(tr, out_af) = self->device->addr.sa_family;

  // transaction needs one reference
  SIPLeelenSession_incref(self);
  if (now) {
    // osip_transaction_execute returns 1 if event got consumed
    osip_transaction_execute(tr, event);
  } else {
    // BUG of osip: osip_transaction_add_event may fail when OOM, but the return
    // value is missing
    osip_transaction_add_event(tr, event);
  }
  return 0;
}


int SIPLeelenSession_bye (struct SIPLeelenSession *self, bool now) {
  leelen_id_t id = self->leelen.id;
  int ret = 0;
  should (SIPLeelenSession_bye_leelen(self) == 0) otherwise {
    LOG_PERROR(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_ID
              ": Cannot close LEELEN session", id);
    ret |= 1;
  }
  should (SIPLeelenSession_bye_sip(self, now) == 0) otherwise {
    LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_ID
        ": Cannot create SIP BYE transaction", id);
    ret |= 2;
  }
  return ret;
}


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
      }
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
      }
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

  self->refcount = 1;

  memset(&self->ours, 0, sizeof(self->ours));

  int mtu = min(device->mtu, device->leelen.config->mtu);
  Forwarder_init(&self->audio, mtu);
  Forwarder_init(&self->video, mtu);

  self->invite_state = SINGLE_FLAG_INIT;
  return 0;
}


bool SIPLeelenSession_may_free (struct SIPLeelenSession *self) {
  return_if_not (refcount_should_free(&self->refcount)) false;
  SIPLeelenSession_destroy(self);
  free(self);
  return true;
}


bool SIPLeelenSession_decref (struct SIPLeelenSession *self) {
  return refcount_dec(&self->refcount) && SIPLeelenSession_may_free(self);
}
