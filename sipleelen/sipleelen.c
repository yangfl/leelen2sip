#include <errno.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>  // osip
#include <sys/types.h>

#include <inet46i/in46.h>
#include <inet46i/inet46.h>
#include <inet46i/recvfromto.h>
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


struct SIPLeelenSession *SIPLeelen_new_session (
    struct SIPLeelen *self, int lock) {
  if (lock > 0) {
    mtx_lock(&self->mtx_sessions);
  }

  struct SIPLeelenSession *session = NULL;
  int i = PTRVCREATE(
    &self->sessions, &self->n_session, struct SIPLeelenSession);
  if likely (i >= 0) {
    if unlikely (SIPLeelenSession_init(self->sessions[i], self) != 0) {
      ptrvsteal(&self->sessions, &self->n_session, i);
      free(self->sessions[i]);
    } else {
      session = self->sessions[i];
    }
  }

  if (lock != 0) {
    mtx_unlock(&self->mtx_sessions);
  }
  return session;
}


/**
 * @memberof SIPLeelen
 * @private
 * @brief Remove a session from @p self->sessions.
 *
 * @param self LEELEN2SIP object.
 * @param session LEELEN2SIP session.
 * @param index Index of @p session in @p self->sessions, or @c -1 if unknown.
 */
static void SIPLeelen_remove_session (
    struct SIPLeelen *self, struct SIPLeelenSession *session, int index) {
  if (index < 0) {
    index = ptrvfind(self->sessions, self->n_session, session);
    return_if_fail (index >= 0);
  }
  ptrvsteal(&self->sessions, &self->n_session, index);
}


/**
 * @memberof SIPLeelen
 * @brief Destroy a session in @p self->sessions.
 *
 * @param self LEELEN2SIP object.
 * @param session LEELEN2SIP session.
 * @param index Index of @p session in @p self->sessions, or @c -1 if unknown.
 * @param lock =0: Don't perform locking/unlocking; >0: Perform
 *  locking/unlocking; <0: Don't perform locking, but perform unlocking.
 */
static void SIPLeelen_free_session (
    struct SIPLeelen *self, struct SIPLeelenSession *session, int index,
    int lock) {
  LOG(LOG_LEVEL_DEBUG, "Dialog " PRI_LEELEN_ID ": Destroying session",
      session->leelen.id);
  if (lock > 0) {
    mtx_lock(&self->mtx_sessions);
  }
  SIPLeelenSession_destroy(session);
  free(session);
  SIPLeelen_remove_session(self, session, index);
  if (lock != 0) {
    mtx_unlock(&self->mtx_sessions);
  }
}


/**
 * @memberof SIPLeelen
 * @brief Destroy a session in @p self->sessions when necessary.
 *
 * @param self LEELEN2SIP object.
 * @param session LEELEN2SIP session.
 * @param index Index of @p session in @p self->sessions, or @c -1 if unknown.
 * @param lock =0: Don't perform locking/unlocking; >0: Perform
 *  locking/unlocking; <0: Don't perform locking, but perform unlocking.
 */
static void SIPLeelen_may_free_session (
    struct SIPLeelen *self, struct SIPLeelenSession *session, int index,
    int lock) {
  unsigned int old_refcount = 0;
  atomic_compare_exchange_strong(&session->refcount, &old_refcount, 1);
  return_if_fail (old_refcount == 0);
  SIPLeelen_free_session(self, session, index, lock);
}


void SIPLeelen_decref_session (
    struct SIPLeelen *self, struct SIPLeelenSession *session, int index,
    int lock) {
  return_if_not (--session->refcount == 0);
  SIPLeelen_may_free_session(self, session, index, lock);
}


int SIPLeelen_receive (
    struct SIPLeelen *self, char *buf, int len, int sockfd,
    const struct sockaddr *src, const void *recv_dst) {
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
      osip_event_free(event);
      return 2;
    }
  }

  if (osip_find_transaction_and_add_event(self->osip, event) == OSIP_SUCCESS) {
    osip_ict_execute(self->osip);
    osip_ist_execute(self->osip);
    osip_nict_execute(self->osip);
    osip_nist_execute(self->osip);
  } else {
    should (MSG_IS_REQUEST(event->sip)) otherwise {
      LOG(LOG_LEVEL_WARNING, "Cannot find transaction for response");
      osip_event_free(event);
      return 2;
    }

    // find session
    struct SIPLeelenSession *session = NULL;
    mtx_lock(&self->mtx_sessions);
    forindex (int, i, self->sessions, self->n_session) {
      struct SIPLeelenSession *ses = self->sessions[i];
      osip_transaction_t *orig_tr = ses->transaction;
      if (ses->sip != NULL) {
        continue_if_not (
          osip_dialog_match_as_uas(ses->sip, event->sip) == OSIP_SUCCESS);
      } else if (orig_tr != NULL) {
        continue_if_not (osip_transaction_match(orig_tr, event->sip));
      } else {
        continue;
      }
      session = ses;
      LOG(LOG_LEVEL_DEBUG, "Receive request %s, matched dialog %08x",
          event->sip->sip_method, session->leelen.id);
      goto found_session;
    }
    LOG(LOG_LEVEL_DEBUG, "Receive request %s", event->sip->sip_method);
found_session:
    mtx_unlock(&self->mtx_sessions);

    // create
    osip_transaction_t *tr = osip_create_transaction(self->osip, event);
    osip_event_free(event);
    should (tr != NULL) otherwise {
      LOG(LOG_LEVEL_WARNING, "Cannot create transaction for SIP message: %.*s",
          len, buf);
      if (session != NULL) {
        SIPLeelen_decref_session(self, session, -1, 1);
      }
      return 2;
    }
    int trid = tr->transactionid;

    if (session == NULL && tr->ctx_type == IST) {
      // create session
      session = SIPLeelen_new_session(self, 1);
      should (session != NULL) otherwise {
        LOG_PERROR(
          LOG_LEVEL_WARNING, "Transaction %d: Cannot create session for IST",
          trid);
        osip_transaction_free(tr);
        return 2;
      }
      session->ours = *(union in46_addr *) recv_dst;
    }

    // init data
    tr->your_instance = self;
    tr->out_socket = sockfd;
    tr->reserved1 = session;
    SIPTransactionData_get(tr, out_af) = src->sa_family;

    if (session != NULL) {
      SIPLeelenSession_incref(session);
      osip_transaction_t *old_tr = NULL;
      atomic_compare_exchange_strong(&session->transaction, &old_tr, tr);
    }

    LOGEVENT (LOG_LEVEL_DEBUG) {
      char *from;
      osip_from_to_str(tr->from, &from);
      LOGEVENT_LOG("Transaction %d: Create %s transaction for %s",
                   trid, osip_fsm_type_names[tr->ctx_type], from);
      osip_free(from);
    }

    for (osip_event_t *event; ;) {
      event = osip_fifo_tryget(tr->transactionff);
      break_if_fail (event != NULL);
      should (osip_transaction_execute(tr, event) != OSIP_SUCCESS) otherwise {
        LOG(LOG_LEVEL_WARNING, "Transaction %d: Kill event?", trid);
        if (session != NULL) {
          osip_transaction_t *cur_tr = tr;
          atomic_compare_exchange_strong(&session->transaction, &cur_tr, NULL);
          SIPLeelen_decref_session(self, session, -1, 1);
        }
        osip_transaction_free(tr);
        break;
      }
    }
  }

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

  mtx_lock(&self->mtx_sessions);

  // find session
  forindex (int, i, self->sessions, self->n_session) {
    struct SIPLeelenSession *session = self->sessions[i];
    continue_if_not (session->leelen.id == id);
    single_join(&session->invite_state);
    LOG(LOG_LEVEL_DEBUG,
        "Dialog " PRI_LEELEN_ID ": Matched, SIP dialog %s", session->leelen.id,
        session->sip == NULL ? "(no SIP dialog)" : session->sip->call_id);
    mtx_unlock(&self->mtx_sessions);
    // dispatch
    return SIPLeelenSession_receive(session, buf, sockfd, src);
  }

  // create session
  should (code == LEELEN_CODE_CALL || code == LEELEN_CODE_VIEW ||
          code == LEELEN_CODE_VOICE_MESSAGE) otherwise {
    LOG(LOG_LEVEL_WARNING,
        "Dialog " PRI_LEELEN_ID ": Initial code " PRI_LEELEN_CODE
        " is not an invitation", id, code);
    mtx_unlock(&self->mtx_sessions);
    return 1;
  }

  struct SIPLeelenSession *session = SIPLeelen_new_session(self, -1);
  should (session != NULL) otherwise {
    LOG_PERROR(
      LOG_LEVEL_WARNING, "Cannot create session for dialog " PRI_LEELEN_ID, id);
    return -1;
  }
  LeelenDialog_init(&session->leelen, self->leelen.config, src, NULL, id);

  // dispatch
  int ret = SIPLeelenSession_receive(session, buf, sockfd, src);
  should (ret == 0) otherwise {
    SIPLeelen_free_session(self, session, -1, 1);
    return ret;
  }
  LOG(LOG_LEVEL_DEBUG, "Dialog " PRI_LEELEN_ID ": Created new session", id);
  return 0;
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


/**
 * @memberof SIPLeelen
 * @private
 * @brief Thread to handle events.
 *
 * @param arg LEELEN2SIP object.
 * @return 0.
 */
static int SIPLeelen_mainloop (void *arg) {
  threadname_set("SIPLeelen");

  struct SIPLeelen *self = arg;

  struct pollfd pollfds[] = {
    {.fd = self->socket_leelen, .events = POLLIN},
    {.fd = self->socket_sip, .events = POLLIN},
  };

  for (unsigned int t = 1; ; t++) {
    // for every ~800ms
    if (t % 8 == 0) {
      time_t now = time(NULL);

      // process SIP timeout
      osip_timers_ict_execute(self->osip);
      osip_timers_ist_execute(self->osip);
      osip_timers_nict_execute(self->osip);
      osip_timers_nist_execute(self->osip);

      // process session timeout
      mtx_lock(&self->mtx_sessions);
      forindex (int, i, self->sessions, self->n_session) {
        struct SIPLeelenSession *session = self->sessions[i];
        // does nothing if thread is running
        continue_if (single_still_running(&session->invite_state));

        leelen_id_t id = session->leelen.id;
        int state = session->leelen.state;

        if (state == LEELEN_DIALOG_CONNECTING) {
          // does nothing if waiting for ack
          continue_if (!LeelenDialog_ack_timeout(&session->leelen));
          osip_transaction_t *tr = session->transaction;
          int trid = tr->transactionid;
          LOG(LOG_LEVEL_DEBUG, "Transaction %d: Dial timeout", trid);

          // need 2 references to keep session alive, we have 1
          SIPLeelenSession_incref(session);

          // end LEELEN dialog
          should (SIPLeelenSession_bye_leelen(session) == 0) otherwise {
            LOG_PERROR(
              LOG_LEVEL_WARNING, "Transaction %d: Cannot close LEELEN session",
              trid);
            SIPLeelen_decref_session(self, session, i, 0);
          }

          // end SIP transaction
          should (osip_transaction_response(
              tr, 404, self->ua, true) == OSIP_SUCCESS) otherwise {
            LOG(LOG_LEVEL_WARNING,
                "Transaction %d: Out of memory, reply not sent", trid);
            SIPLeelen_decref_session(self, session, i, 0);
          }
        } else if (state == LEELEN_DIALOG_CONNECTED) {
          // does nothing if transaction is in progress or session still alive
          continue_if (session->transaction != NULL);
          continue_if (!LeelenDialog_dialog_timeout(&session->leelen, now));
          LOG(LOG_LEVEL_DEBUG, "Dialog " PRI_LEELEN_ID ": Session timeout", id);

          int res = SIPLeelenSession_bye(session, true);
          should ((res & 1) == 0) otherwise {
            SIPLeelen_decref_session(self, session, i, 0);
          }
        } else {
          if (state == LEELEN_DIALOG_DISCONNECTING) {
            // does nothing if waiting for ack
            continue_if (session->leelen.last_sent.tv_sec +
                         2 * session->leelen.timeout / 1000 < now);
          }
          LOG(LOG_LEVEL_DEBUG, "Dialog " PRI_LEELEN_ID ": Session end", id);
          ptrvsteal(&self->sessions, &self->n_session, i);
          if (SIPLeelenSession_decref(session)) {
            LOG(LOG_LEVEL_DEBUG, "Dialog " PRI_LEELEN_ID
                ": Session destroyed", id);
          }
        }
      }
      mtx_unlock(&self->mtx_sessions);

      // process SIP events
      osip_ict_execute(self->osip);
      osip_ist_execute(self->osip);
      osip_nict_execute(self->osip);
      osip_nist_execute(self->osip);
    }

    // poll
    int pollres = poll(pollfds, 2, 100);
    break_if_fail (single_continue(&self->state));
    should (pollres >= 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "poll() failed");
      continue;
    }
    continue_if (likely (pollres <= 0));

    // process received packets
    for (int i = 0; i < 2; i++) {
      continue_if (pollfds[i].revents == 0);

      // read incoming packet
      char buf[i == 0 ? self->leelen.config->mtu : self->mtu];
      union sockaddr_in46 src;
      socklen_t srclen = sizeof(src);
      union sockaddr_in46 dst;
      struct in_addr recv_dst;
      int buflen = i == 0 ?
        recvfrom(pollfds[i].fd, buf, sizeof(buf), 0, &src.sock, &srclen) :
        recvfromto(pollfds[i].fd, buf, sizeof(buf), 0, &src.sock, &srclen,
                   &dst, &recv_dst);
      should (buflen >= 0) otherwise {
        LOG_PERROR(LOG_LEVEL_WARNING, "recvfrom() failed");
        continue;
      }
      continue_if_fail (buflen > 0);

      // process
      if (i == 0) {
        SIPLeelen_receive_leelen(self, buf, buflen, pollfds[i].fd, &src.sock);
      } else {
        SIPLeelen_receive(
          self, buf, buflen, pollfds[i].fd, &src.sock,
          src.sa_family == AF_INET6 ?
            (void *) &dst.v6.sin6_addr : (void *) &recv_dst);
      }
    }
  }

  return 0;
}


int SIPLeelen_run (struct SIPLeelen *self) {
  return_if_fail (self->socket_leelen >= 0 && self->socket_sip >= 0) 255;
  return_nonzero (LeelenDiscovery_start(&self->leelen));

  char name[THREADNAME_SIZE];
  threadname_get(name, sizeof(name));
  int res = single_run(&self->state, SIPLeelen_mainloop, self);
  threadname_set(name);
  return res;
}


int SIPLeelen_start (struct SIPLeelen *self) {
  return_if_fail (self->socket_leelen >= 0 && self->socket_sip >= 0) 255;
  return_nonzero (LeelenDiscovery_start(&self->leelen));
  return single_start(&self->state, SIPLeelen_mainloop, self);
}


void SIPLeelen_stop (struct SIPLeelen *self) {
  LeelenDiscovery_stop(&self->leelen);
  single_stop(&self->state);
}


int SIPLeelen_connect (struct SIPLeelen *self) {
  return_if_fail (LeelenDiscovery_connect(&self->leelen) == 0) 1;

  if likely (self->socket_leelen < 0) {
    union sockaddr_in46 leelen_addr = self->leelen.config->addr;
    leelen_addr.sa_port = htons(self->leelen.config->voip_src);
    self->socket_leelen = openaddr46(
      &leelen_addr, SOCK_DGRAM, OPENADDR_REUSEADDR);
    return_if_fail (self->socket_leelen >= 0) 2;
  }

  if likely (self->socket_sip < 0) {
    int sockfd = openaddr46(&self->addr, SOCK_DGRAM, OPENADDR_REUSEADDR);
    return_if_fail (sockfd >= 0) 4;

    bool v6 = self->addr.sa_family == AF_INET6;
    const int on = 1;
    should (setsockopt(
        sockfd, v6 ? IPPROTO_IPV6 : IPPROTO_IP,
        v6 ? IPV6_RECVPKTINFO : IP_PKTINFO, &on, sizeof(on)) == 0) otherwise {
      int saved_errno = errno;
      close(sockfd);
      errno = saved_errno;
      return 4;
    }

    self->socket_sip = sockfd;
  }

  return 0;
}


void SIPLeelen_destroy (struct SIPLeelen *self) {
  SIPLeelen_stop(self);

  LeelenDiscovery_destroy(&self->leelen);
  free(self->ua);
  osip_release(self->osip);
  free(self->client);
  if likely (self->sessions != NULL) {
    forindex (int, i, self->sessions, self->n_session) {
      SIPLeelenSession_destroy(self->sessions[i]);
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
  return_nonzero (LeelenDiscovery_init(&self->leelen, config));
  int saved_errno;
  should (mtx_init(&self->mtx_sessions, mtx_plain) == thrd_success) otherwise {
    saved_errno = errno;
    goto fail_mtx;
  }
  should (osip_init(&self->osip) == OSIP_SUCCESS) otherwise {
    saved_errno = errno;
    mtx_destroy(&self->mtx_sessions);
fail_mtx:
    LeelenDiscovery_destroy(&self->leelen);
    errno = saved_errno;
    return -1;
  }

  osip_set_application_context(self->osip, self);
  osip_set_cb_send_message(self->osip, _SIPLeelen_send);

  SIPLeelen_set_uax_callbacks(self);
  SIPLeelen_set_uas_callbacks(self);

  self->ua = NULL;
  memset(&self->addr, 0, sizeof(self->addr));
  self->addr.sa_family = AF_INET6;
  self->addr.sa_port = htons(SIPLEELEN_PORT);
  self->mtu = SIPLEELEN_MAX_MESSAGE_LENGTH;

  self->client = NULL;
  self->clients.sa_family = AF_UNSPEC;
  self->sessions = NULL;
  self->n_session = 0;
  self->socket_sip = -1;
  self->socket_leelen = -1;
  self->state = SINGLE_FLAG_INIT;
  return 0;
}
