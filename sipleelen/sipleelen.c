#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>  // osip
#include <sys/types.h>

#include <inet46i/in46.h>
#include <inet46i/inet46.h>
#include <inet46i/sockaddr46.h>
#include <inet46i/socket46.h>
#include <osip2/osip.h>

#include "utils/macro.h"
#include "utils/array.h"
#include "utils/log.h"
#include "utils/osip.h"
#include "utils/single.h"
#include "utils/threadname.h"
#include "leelen/config.h"
#include "leelen/discovery/discovery.h"
#include "leelen/voip/protocol.h"
#include "session.h"
#include "transaction.h"
#include "uac.h"
#include "uas.h"
#include "sipleelen.h"


void SIPLeelen_lock_sessions (struct SIPLeelen *self) {
  should (mtx_lock(&self->mtx_sessions) == thrd_success) otherwise {
    LOG_PERROR(LOG_LEVEL_WARNING, "Cannot lock mutex");
  }
}


void SIPLeelen_unlock_sessions (struct SIPLeelen *self) {
  should (mtx_unlock(&self->mtx_sessions) == thrd_success) otherwise {
    LOG_PERROR(LOG_LEVEL_WARNING, "Cannot unlock mutex");
  }
}


int SIPLeelen_create_session (struct SIPLeelen *self) {
  int i = PTRVCREATE(
    &self->sessions, &self->n_session, struct SIPLeelenSession);
  return_if_fail (i >= 0) i;
  struct SIPLeelenSession *session = self->sessions[i];
  should (SIPLeelenSession_init(session, self) == 0) otherwise {
    ptrvsteal(&self->sessions, &self->n_session, i);
    free(session);
    return -1;
  }
  return i;
}


void SIPLeelen_remove_session (
    struct SIPLeelen *self, const struct SIPLeelenSession *session) {
  ptrvtake(&self->sessions, &self->n_session, session);
}


int SIPLeelen_receive (
    struct SIPLeelen *self, char *buf, int len, int sockfd,
    const struct sockaddr *src) {
  LOGEVENT (LOG_LEVEL_VERBOSE) {
    char s_src[SOCKADDR_STRLEN];
    sockaddr_toa(src, s_src, sizeof(s_src));
    LOGEVENT_LOG("From %s:\n%.*s", s_src, len, buf);
  }

  // parse
  osip_event_t *event = osip_parse(buf, len);
  should (event != NULL) otherwise {
    LOG(
      LOG_LEVEL_WARNING, LOG_WOULD_LOG(LOG_LEVEL_VERBOSE) ?
        "Cannot parse SIP message:\n%.*s" : "Cannot parse SIP message",
      len, buf);
    return 1;
  }

  // fix via header
  if (!MSG_IS_RESPONSE(event->sip) && src != NULL) {
    void *host;
    in_port_t port = sockaddr_get(src, &host);

    int af = src->sa_family;
    if (af == AF_INET6 && IN6_SET_ADDR_V4MAPPED(host)) {
      af = AF_INET;
      host = in64_get(af, host);
    }

    char hoststr[INET6_ADDRSTRLEN];
    inet_ntop(af, host, hoststr, sizeof(hoststr));
    should (osip_message_fix_last_via_header(
        event->sip, hoststr, port) == OSIP_SUCCESS) otherwise {
      LOG(LOG_LEVEL_WARNING, "Out of memory, cannot fix last via header");
      return 2;
    }
  }

  if (osip_find_transaction_and_add_event(self->osip, event) != OSIP_SUCCESS) {
    // create
    osip_transaction_t *tr = osip_create_transaction(self->osip, event);
    osip_event_free(event);
    should (tr != NULL) otherwise {
      LOG(LOG_LEVEL_WARNING, "Cannot create transaction for SIP message: %.*s",
          len, buf);
      return 2;
    }
    if (LOG_WOULD_LOG(LOG_LEVEL_DEBUG)) {
      char *from;
      osip_from_to_str(tr->from, &from);
      LOG(LOG_LEVEL_DEBUG, "Transaction %d: Create %s transaction for %s",
          tr->transactionid, osip_fsm_type_names[tr->ctx_type], from);
      osip_free(from);
    }
    // init data
    tr->your_instance = self;
    tr->out_socket = sockfd;
    SIPTransactionData_get(tr, out_af) = src->sa_family;
    should (_SIPLeelen_init_transaction(tr->ctx_type, tr) == 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING,
                 "Transaction %d: Cannot create IST transaction data",
                 tr->transactionid);
      osip_transaction_free(tr);
      return 2;
    }
  }

  SIPLeelen_execute(self);
  return 0;
}


int SIPLeelen_receive_leelen (
    struct SIPLeelen *self, char *buf, int len, int sockfd,
    const struct sockaddr *src) {
  return_if_fail (len > LEELEN_MESSAGE_HEADER_SIZE) 255;
  buf[len - 1] = '\0';

  leelen_id_t id = LEELEN_MESSAGE_ID(buf);
  enum LeelenCode code = le32toh(LEELEN_MESSAGE_CODE(buf));

  LOGEVENT (LOG_LEVEL_VERBOSE) {
    char s_src[SOCKADDR_STRLEN];
    sockaddr_toa(src, s_src, sizeof(s_src));
    LOGEVENT_LOG(
      "From %s, dialog " PRI_LEELEN_ID ", code " PRI_LEELEN_CODE ":\n%.*s",
      s_src, id, code,
      len - LEELEN_MESSAGE_HEADER_SIZE, buf + LEELEN_MESSAGE_HEADER_SIZE);
  }

  SIPLeelen_lock_sessions(self);

  // find session
  forindex (int, i, self->sessions, self->n_session) {
    struct SIPLeelenSession *session = self->sessions[i];
    continue_if_not (session->leelen.id == id);
    single_join(&session->invite_state);
    LOG(LOG_LEVEL_DEBUG,
        "Dialog " PRI_LEELEN_CODE ": Matched session %s", session->leelen.id,
        session->sip == NULL ? "(no dialog)" : session->sip->call_id);
    // dispatch
    SIPLeelen_unlock_sessions(self);
    return SIPLeelenSession_receive(session, buf, sockfd, src);
  }

  int ret;

  // create session
  should (code == LEELEN_CODE_CALL || code == LEELEN_CODE_VIEW ||
          code == LEELEN_CODE_VOICE_MESSAGE) otherwise {
    LOG(LOG_LEVEL_WARNING,
        "Dialog " PRI_LEELEN_ID ": Initial code " PRI_LEELEN_CODE
        " is not an invitation", id, code);
    ret = 1;
    goto end;
  }

  int i = SIPLeelen_create_session(self);
  should (i >= 0) otherwise {
    LOG_PERROR(
      LOG_LEVEL_WARNING, "Cannot create session for dialog " PRI_LEELEN_ID, id);
    ret = -1;
    goto end;
  }
  struct SIPLeelenSession *session = self->sessions[i];
  LeelenDialog_init(&session->leelen, self->config, src, NULL, id);

  // dispatch
  ret = SIPLeelenSession_receive(session, buf, sockfd, src);
  should (ret == 0) otherwise {
    SIPLeelenSession_destroy(session);
    ptrvsteal(&self->sessions, &self->n_session, i);
    free(session);
    goto end;
  }
  LOG(LOG_LEVEL_DEBUG, "Dialog " PRI_LEELEN_ID ": Created new session", id);

end:
  SIPLeelen_unlock_sessions(self);
  return ret;
}


/**
 * @relates SIPLeelen
 * @brief Callback for sending SIP messages.
 *
 * @param tr OSIP transaction.
 * @param msg Message to send.
 * @param host Host to send to.
 * @param port Port to send to.
 * @param out_socket Socket to send to.
 * @return 0 on success, -1 if @c sendto() error, 255 if address family
 *  unsupported.
 */
static int _SIPLeelen_send (
    osip_transaction_t *tr, osip_message_t *msg, char *host, int port,
    int out_socket) {
  union sockaddr_in46 dst;
  dst.sa_family = SIPTransactionData_get(tr, out_af);

  struct in64_addr addr;
  if (inet_aton64(host, &addr) == AF_INET6 && dst.sa_family == AF_INET) {
    should (IN6_IS_ADDR_V4MAPPED(&addr)) otherwise {
      LOG(LOG_LEVEL_WARNING,
          "SIP wants to send message to %s on unsupported socket of type %d",
          host, dst.sa_family);
      return 255;
    }
  }
  sockaddr46_set(&dst, dst.sa_family, in64_get(dst.sa_family, &addr), port);

  char *buf;
  size_t len;
  return_if_fail (osip_message_to_str(msg, &buf, &len) == 0) 1;
  if (LOG_WOULD_LOG(LOG_LEVEL_VERBOSE)) {
    char s_dst[SOCKADDR_STRLEN];
    sockaddr_toa(&dst.sock, s_dst, sizeof(s_dst));
    LOG(LOG_LEVEL_VERBOSE, "To %s:\n%.*s", s_dst, (int) len, buf);
  }

  int ret = 0;
  should (sendto(
      out_socket, buf, len, 0, &dst.sock, dst.sa_family == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
  ) == (ssize_t) len) otherwise {
    LOG_PERROR(LOG_LEVEL_INFO, "sendto()");
    ret = -1;
  }
  osip_free(buf);
  return ret;
}


void SIPLeelen_execute (struct SIPLeelen *self) {
  osip_timers_ict_execute(self->osip);
  osip_timers_ist_execute(self->osip);
  osip_timers_nict_execute(self->osip);
  osip_timers_nist_execute(self->osip);
  osip_ict_execute(self->osip);
  osip_ist_execute(self->osip);
  osip_nict_execute(self->osip);
  osip_nist_execute(self->osip);
}


/**
 * @memberof SIPLeelen
 * @private
 * @brief Thread to handle events.
 *
 * @param args LEELEN2SIP object.
 * @return 0.
 */
static int SIPLeelen_mainloop (void *args) {
  threadname_set("SIPLeelen");

  struct SIPLeelen *self = args;

  struct pollfd pollfds[] = {
    {.fd = self->socket_leelen, .events = POLLIN},
    {.fd = self->socket_sip, .events = POLLIN},
  };

  for (unsigned int t = 0; single_continue(&self->state); t++) {
    int pollres = poll(pollfds, 2, 100);

    // process timeout events every ~400ms
    if (t % 4 == 0) {
      SIPLeelen_execute(self);
    }

    should (pollres >= 0) otherwise {
      break_if (!single_continue(&self->state));
      LOG_PERROR(LOG_LEVEL_WARNING, "poll() failed");
      continue;
    }
    continue_if (likely (pollres <= 0));

    // process received packets
    for (int i = 0; i < 2; i++) {
      continue_if (pollfds[i].revents == 0);

      // read incoming packet
      char buf[i == 0 ? self->config->mtu : self->mtu];
      union sockaddr_in46 src;
      socklen_t srclen = sizeof(src);
      int buflen = recvfrom(
        pollfds[i].fd, buf, sizeof(buf), 0, &src.sock, &srclen);
      should (buflen >= 0) otherwise {
        LOG_PERROR(LOG_LEVEL_WARNING, "recvfrom() failed");
        continue;
      }
      continue_if_fail (buflen > 0);

      // process
      (i == 0 ? SIPLeelen_receive_leelen : SIPLeelen_receive)(
        self, buf, buflen, pollfds[i].fd, &src.sock);
    }
  }

  return 0;
}


int SIPLeelen_run (struct SIPLeelen *self) {
  return_if_fail (self->socket_leelen >= 0 && self->socket_sip >= 0) 255;
  return_nonzero (LeelenDiscovery_start(&self->device));

  char name[THREADNAME_SIZE];
  threadname_get(name, sizeof(name));
  int res = single_run(&self->state, SIPLeelen_mainloop, self);
  threadname_set(name);
  return res;
}


int SIPLeelen_start (struct SIPLeelen *self) {
  return_if_fail (self->socket_leelen >= 0 && self->socket_sip >= 0) 255;
  return_nonzero (LeelenDiscovery_start(&self->device));
  return single_start(&self->state, SIPLeelen_mainloop, self);
}


void SIPLeelen_stop (struct SIPLeelen *self) {
  LeelenDiscovery_stop(&self->device);
  single_stop(&self->state);
}


int SIPLeelen_connect (struct SIPLeelen *self) {
  return_if_fail (LeelenDiscovery_connect(&self->device) == 0) 1;

  if likely (self->socket_leelen < 0) {
    union sockaddr_in46 leelen_addr = self->config->addr;
    leelen_addr.sa_port = htons(self->config->voip_src);
    self->socket_leelen = openaddr46(
      &leelen_addr, SOCK_DGRAM, OPENADDR_REUSEADDR);
    return_if_fail (self->socket_leelen >= 0) 2;
  }

  if likely (self->socket_sip < 0) {
    self->socket_sip = openaddr46(&self->addr, SOCK_DGRAM, OPENADDR_REUSEADDR);
    return_if_fail (self->socket_sip >= 0) 4;
  }

  return 0;
}


void SIPLeelen_destroy (struct SIPLeelen *self) {
  SIPLeelen_stop(self);

  LeelenDiscovery_destroy(&self->device);
  free(self->ua);
  osip_release(self->osip);
  if likely (self->sessions != NULL) {
    for (int i = 0; i < self->n_session; i++) {
      if (self->sessions[i] != NULL) {
        SIPLeelenSession_destroy(self->sessions[i]);
      }
    }
    free(self->sessions);
  }
  if likely (self->socket_sip >= 0) {
    close(self->socket_sip);
  }
  if likely (self->socket_leelen >= 0) {
    close(self->socket_leelen);
  }
  mtx_destroy(&self->mtx_sessions);

  single_join(&self->state);
}


int SIPLeelen_init (struct SIPLeelen *self, const struct LeelenConfig *config) {
  return_nonzero (LeelenDiscovery_init(&self->device, config));
  int saved_errno;
  should (mtx_init(&self->mtx_sessions, mtx_plain) == thrd_success) otherwise {
    saved_errno = errno;
    goto fail_mtx;
  };
  should (osip_init(&self->osip) == OSIP_SUCCESS) otherwise {
    saved_errno = errno;
    mtx_destroy(&self->mtx_sessions);
fail_mtx:
    LeelenDiscovery_destroy(&self->device);
    errno = saved_errno;
    return -1;
  };

  osip_set_application_context(self->osip, self);
  osip_set_cb_send_message(self->osip, _SIPLeelen_send);

  SIPLeelen_set_uax_callbacks(self);
  SIPLeelen_set_uas_callbacks(self);

  self->ua = NULL;
  memset(&self->addr, 0, sizeof(self->addr));
  self->addr.sa_family = AF_INET6;
  self->addr.sa_port = htons(SIPLEELEN_PORT);
  self->mtu = SIPLEELEN_MAX_MESSAGE_LENGTH;

  self->client.sa_family = AF_UNSPEC;
  self->sessions = NULL;
  self->n_session = 0;
  self->socket_sip = -1;
  self->socket_leelen = -1;
  self->state = SINGLE_FLAG_INIT;
  return 0;
}
