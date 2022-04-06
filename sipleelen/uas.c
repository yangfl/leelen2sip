#include <poll.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>  // osip

#include <inet46i/sockaddr46.h>
#include <inet46i/socket46.h>
#include <osip2/osip.h>
#include <osip2/osip_dialog.h>

#include "utils/macro.h"
#include "utils/array.h"
#include "utils/log.h"
#include "utils/osip.h"
#include "utils/single.h"
#include "utils/threadname.h"
#include "leelen/config.h"
#include "leelen/number.h"
#include "leelen/discovery/host.h"
#include "leelen/voip/dialog.h"
#include "leelen/voip/message.h"
#include "leelen/voip/protocol.h"
#include "session.h"
#include "sipleelen.h"
#include "transaction.h"
#include "uas.h"


/**
 * @relates SIPTransactionData
 * @private
 * @brief INVITE main thread.
 *
 * @param arg Transaction.
 */
static int _SIPLeelen_ist_invite_process (void *arg) {
  osip_transaction_t *tr = arg;
  osip_message_t *request = tr->orig_request;
  struct SIPLeelen *self = tr->your_instance;
  int trid = tr->transactionid;
  struct SIPLeelenSession *session = tr->reserved1;
  int status_code;

  threadname_format("INVITE %d", trid);

  // discover if new session
  if (session->leelen.id == 0) {
    // discovery
    struct LeelenHost host;
    int res = LeelenDiscovery_discovery(
      &self->leelen, &host, session->number.str);
    should (res == 0) otherwise {
      switch (res) {
        case -1:
          LOG_PERROR(
            LOG_LEVEL_WARNING, "Transaction %d: Cannot send discovery", trid);
          status_code = 500;
          break;
        case 254:
          LOG(LOG_LEVEL_DEBUG, "Transaction %d: Cannot find %s", trid,
              session->number.str);
          status_code = 404;
          break;
        default:
          LOG(LOG_LEVEL_WARNING, "Transaction %d: Internal error: %d",
              trid, res);
          status_code = 500;
          break;
      }
      goto reply;
    }
    LOGEVENT (LOG_LEVEL_DEBUG) {
      char s_addr[SOCKADDR_STRLEN];
      sockaddr_toa(&host.sock, s_addr, sizeof(s_addr));
      LOGEVENT_LOG(
        "Transaction %d: %s is at %s", trid, session->number.str, s_addr);
    }

    // convert address if needed
    if (self->leelen.config->addr.sa_family != host.sa_family) {
      if (self->leelen.config->addr.sa_family == AF_INET6) {
        sockaddr_to6(&host.sock);
      } else {
        should (sockaddr_to4(&host.sock) != NULL) otherwise {
          LOG(LOG_LEVEL_INFO,
              "Transaction %d: Cannot connect to IPv6 address from IPv4 socket",
              trid);
        }
        LeelenHost_destroy(&host);
        status_code = 500;
        goto reply;
      }
    }

    // connect
    LeelenDialog_init(
      &session->leelen, self->leelen.config, &host.sock, &session->number, 0);
    LeelenHost_destroy(&host);
    LOG(LOG_LEVEL_DEBUG, "Transaction %d: Start LEELEN dialog " PRI_LEELEN_ID,
        trid, session->leelen.id);
  }

  // parse SDP
  char **audio_formats;
  char **video_formats;
  switch (_SIPLeelen_extract_media_formats(
      request, &audio_formats, &video_formats)) {
    case 1:
      LOG(LOG_LEVEL_INFO, "Transaction %d: Cannot parse SDP", trid);
      status_code = 400;
      goto reply;
    case -1:
      LOG(LOG_LEVEL_INFO,
          "Transaction %d: Out of memory during parsing SDP", trid);
      status_code = 500;
      goto reply;
  }

  // send invite
  session->transaction = tr;
  int res = LeelenDialog_send(
    &session->leelen, LEELEN_CODE_CALL, self->socket_leelen,
    audio_formats, video_formats);
  free(audio_formats);
  free(video_formats);
  should (res == 0) otherwise {
    LOG_PERROR(
      LOG_LEVEL_WARNING, "Transaction %d: Cannot send LEELEN invite", trid);
    status_code = 500;
    goto reply;
  }

  int ret = 0;

  // send fail SIP message
  if (0) {
reply:
    should (osip_transaction_response(
        tr, status_code, self->ua, true) == OSIP_SUCCESS) otherwise {
      LOG(LOG_LEVEL_WARNING,
          "Transaction %d: Out of memory, reply not sent", trid);
    }
    ret = 1;
  }

  single_finish(&session->invite_state);
  SIPLeelenSession_decref(session);
  return ret;
}


/**
 * @relates SIPTransactionData
 * @private
 * @brief Process INVITE request.
 *
 * @param type Transaction type.
 * @param tr Transaction.
 * @param request OSIP Request.
 */
static void _SIPLeelen_ist_invite (
    int type, osip_transaction_t *tr, osip_message_t *request) {
  (void) type;

  struct SIPLeelen *self = tr->your_instance;
  int trid = tr->transactionid;
  struct SIPLeelenSession *session = tr->reserved1;
  int status_code;

  should (!SIPLeelenSession_established(session)) otherwise {
    LOG(LOG_LEVEL_DEBUG, "Transaction %d: re-INVITE, reject", trid);
    status_code = 488;
    goto reply;
  }

  should (osip_message_has_sdp(request)) otherwise {
    LOG(LOG_LEVEL_DEBUG, "Transaction %d: INVITE does not has SDP", trid);
    status_code = 400;
    goto reply;
  }

  // format phone number
  struct LeelenNumber number;
  {
    const char *username = osip_uri_get_username(osip_to_get_url(request->to));
    should (username != NULL) otherwise {
      LOGEVENT (LOG_LEVEL_INFO) {
        char *uri;
        osip_uri_to_str(osip_to_get_url(request->to), &uri);
        LOGEVENT_LOG(
          "Transaction %d: INVITE URL %s has no username", trid, uri);
        osip_free(uri);
      }
      status_code = 400;
      goto reply;
    }
    should (LeelenNumber_init(
        &number, username, &self->leelen.config->number) == 0) otherwise {
      LOG(LOG_LEVEL_DEBUG, "Transaction %d: Cannot parse %s", trid, username);
      status_code = 410;
      goto reply;
    }
    LOG(LOG_LEVEL_DEBUG, "Transaction %d: Calling %s (%s)",
        trid, username, number.str);
  }

  // start thread if not
  if unlikely (single_is_running(&session->invite_state)) {
    should (LeelenNumber_equal(&session->number, &number)) otherwise {
      LOG(LOG_LEVEL_INFO,
          "Transaction %d: Received phone number %s differs from previous %s",
          trid, number.str, session->number.str);
      status_code = 400;
      goto reply;
    }
  } else {
    single_join(&session->invite_state);

    session->number = number;

    SIPLeelenSession_incref(session);
    should (single_start(
        &session->invite_state, _SIPLeelen_ist_invite_process, tr
    ) == 0) otherwise {
      SIPLeelenSession_decref(session);
      LOG_PERROR(
        LOG_LEVEL_WARNING, "Transaction %d: Cannot start thread", trid);
      status_code = 500;
      goto reply;
    }
  }
  status_code = 100;

  // send SIP message
reply:
  should (osip_transaction_response(
      tr, status_code, self->ua, false) == OSIP_SUCCESS) otherwise {
    LOG(LOG_LEVEL_WARNING, "Transaction %d: Out of memory", trid);
  }
}


/**
 * @relates SIPTransactionData
 * @private
 * @brief Process BYE or CANCEL request.
 *
 * @param type Transaction type.
 * @param tr Transaction.
 * @param request OSIP Request.
 */
static void _SIPLeelen_nist_bye (
    int type, osip_transaction_t *tr, osip_message_t *request) {
  (void) type;
  (void) request;

  struct SIPLeelen *self = tr->your_instance;
  int trid = tr->transactionid;
  struct SIPLeelenSession *session = tr->reserved1;
  int status_code;

  should (session != NULL) otherwise {
    // no session matched
    status_code = 481;
    goto reply;
  }

  status_code = 200;

  if (session->leelen.state == LEELEN_DIALOG_CONNECTING) {
    osip_transaction_t *orig_tr = session->transaction;

    // CANCEL if only INVITE not finished, or BYE if only INVITE finished
    should (
        (type == OSIP_NIST_CANCEL_RECEIVED &&
         orig_tr->state == IST_PROCEEDING) ||
        (type == OSIP_NIST_BYE_RECEIVED && orig_tr->state != IST_PROCEEDING)
    ) otherwise {
      // Section 15.1.1, close anyway
      status_code = 481;
    }

    // only send SIP message when we have not reached IST_COMPLETED
    if (orig_tr->state == IST_PROCEEDING) {
      should (osip_transaction_response(
          orig_tr, 487, self->ua, false) == OSIP_SUCCESS) otherwise {
        LOG(LOG_LEVEL_WARNING, "Transaction %d: Out of memory",
            orig_tr->transactionid);
      }
    }
  }

  // end LEELEN session
  should (SIPLeelenSession_bye_leelen(session) == 0) otherwise {
    LOG_PERROR(LOG_LEVEL_WARNING,
               "Transaction %d: Cannot close LEELEN session", trid);
  }

  // send SIP message
reply:
  should (osip_transaction_response(
      tr, status_code, self->ua, false) == OSIP_SUCCESS) otherwise {
    LOG(LOG_LEVEL_WARNING, "Transaction %d: Out of memory", trid);
  }
}


/**
 * @relates SIPTransactionData
 * @private
 * @brief Process REGISTER or OPTIONS request.
 *
 * @param type Transaction type.
 * @param tr Transaction.
 * @param request OSIP Request.
 */
static void _SIPLeelen_register (
    int type, osip_transaction_t *tr, osip_message_t *request) {
  struct SIPLeelen *self = tr->your_instance;
  int trid = tr->transactionid;

  osip_message_t *response;
  goto_if_fail (osip_message_response(
    &response, request, 200, self->ua) == OSIP_SUCCESS) fail;
  goto_if_fail (osip_message_set_allow(
    response, "INVITE, ACK, CANCEL, OPTIONS, BYE") == OSIP_SUCCESS) fail;
  switch (type) {
    case OSIP_NIST_REGISTER_RECEIVED:
      goto_if_fail (osip_message_set_expires(
        response, STR(LEELEN2SIP_EXPIRES)) == OSIP_SUCCESS) fail;
      break;
    case OSIP_NIST_OPTIONS_RECEIVED:
      goto_if_fail (osip_message_set_accept(
        response, "application/sdp") == OSIP_SUCCESS) fail;
      goto_if_fail (osip_message_set_content_type(
        response, "application/sdp") == OSIP_SUCCESS) fail;
      break;
    default:
      __builtin_unreachable();
  }
  goto_if_fail (
    osip_transaction_send_sipmessage(tr, response, false) == OSIP_SUCCESS) fail;

  if (type == OSIP_NIST_REGISTER_RECEIVED) {
    // save client URL
    char *url;
    if likely (osip_uri_to_str(tr->from->url, &url) == OSIP_SUCCESS) {
      free(atomic_exchange(&self->client, strdup(url)));
      osip_free(url);
    }
    // save client ip
    char *host;
    int port;
    osip_response_get_destination(request, &host, &port);
    sockaddr46_aton(host, &self->clients);
    self->clients.sa_port = htons(port);
  }

  if (0) {
fail:
    LOG(LOG_LEVEL_WARNING, "Transaction %d: Out of memory", trid);
    osip_message_free(response);
  }
  // bug
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
  return;
#pragma GCC diagnostic pop
}


int SIPLeelen_set_uas_callbacks (struct SIPLeelen *self) {
  osip_set_message_callback(
    self->osip, OSIP_NIST_REGISTER_RECEIVED, _SIPLeelen_register);
  osip_set_message_callback(
    self->osip, OSIP_NIST_OPTIONS_RECEIVED, _SIPLeelen_register);

  osip_set_message_callback(
    self->osip, OSIP_IST_INVITE_RECEIVED, _SIPLeelen_ist_invite);
  osip_set_message_callback(
    self->osip, OSIP_IST_INVITE_RECEIVED_AGAIN, _SIPLeelen_ist_invite);

  osip_set_message_callback(
    self->osip, OSIP_NIST_BYE_RECEIVED, _SIPLeelen_nist_bye);
  osip_set_message_callback(
    self->osip, OSIP_NIST_CANCEL_RECEIVED, _SIPLeelen_nist_bye);
  return 0;
}
