#include <endian.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <inet46i/sockaddr46.h>
#include <osipparser2/sdp_message.h>

#include "utils/macro.h"
#include "utils/array.h"
#include "utils/log.h"
#include "utils/osip.h"
#include "utils/sdp_message.h"
#include "utils/single.h"
#include "leelen/config.h"
#include "leelen/voip/protocol.h"
#include "../leelen2sip.h"
#include "session.h"
#include "sipleelen.h"
#include "transaction.h"
#include "uac.h"


int SIPLeelenSession_receive (
    struct SIPLeelenSession *self, char *msg, int sockfd,
    const struct sockaddr *src) {
  leelen_id_t id = self->leelen.id;
  enum LeelenCode code = le32toh(LEELEN_MESSAGE_CODE(msg));
  return_if_fail (code != LEELEN_CODE_OPEN_GATE) 255;

  // verify peer address
  should (sockaddr_same(&self->leelen.theirs.sock, src)) otherwise {
    LOGEVENT (LOG_LEVEL_INFO) {
      char s_src[SOCKADDR_STRLEN];
      sockaddr_toa(src, s_src, sizeof(s_src));
      char s_theirs[SOCKADDR_STRLEN];
      sockaddr_toa(&self->leelen.theirs.sock, s_theirs, sizeof(s_theirs));
      LOGEVENT_LOG(
        "Dialog " PRI_LEELEN_ID
        ": Got message from %s, but old address is %s", id, s_src, s_theirs);
    }
    self->leelen.theirs = *(union sockaddr_in46 *) src;
  }

  // process message
  enum LeelenDialogState old_state = self->leelen.state;
  char **audio_formats;
  char **video_formats;
  switch (LeelenDialog_receive(
      &self->leelen, msg, sockfd, &audio_formats, &video_formats)) {
    case -1:
      LOG_PERROR(
        LOG_LEVEL_WARNING,
        "Dialog " PRI_LEELEN_ID ": Cannot process LEELEN SIP message", id);
      return 1;
    case 254:
      LOG(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_ID
          ": LEELEN ACK got, but timeout reached", id);
      return 1;
  }

  int status_code;
  osip_transaction_t *tr = self->transaction;

  switch (code) {
    default:
      goto neutral;
    case LEELEN_CODE_BYE:
      // no need to handle disconnected session
      goto_if (old_state == LEELEN_DIALOG_DISCONNECTED) end;

      should (old_state != LEELEN_DIALOG_CONNECTING) otherwise {
        should (tr != NULL && tr->ctx_type == IST) otherwise {
          LOG(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_ID
              ": connection establish without SIP transaction", id);
          goto neutral;
        }
        status_code = 487;
        break;
      }

      should (SIPLeelenSession_bye_sip(self, true) == 0) otherwise {
        LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_ID
            ": Cannot create SIP BYE transaction", id);
      }
      goto end;
    case LEELEN_CODE_CALL:
    case LEELEN_CODE_VIEW:
    case LEELEN_CODE_VOICE_MESSAGE:
    case LEELEN_CODE_ACCEPTED:
      goto_if_fail (old_state != LEELEN_DIALOG_DISCONNECTING) fail;
      goto_if_fail (old_state != LEELEN_DIALOG_CONNECTING) connecting;
      goto_if_fail (old_state != LEELEN_DIALOG_CONNECTED) end;

      goto_if_fail (self->device->client != NULL &&
                    self->device->clients.sa_family != AF_UNSPEC) fail;

      // create dialog

      // create transaction
      SIPLeelenSession_incref(self);

      goto end;
    case LEELEN_CODE_OK: {
      // only handle connection event
      goto_if_fail (old_state == LEELEN_DIALOG_CONNECTING) end;

connecting:
      should (tr != NULL && tr->ctx_type == IST) otherwise {
        LOG(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_ID
            ": connection establish without SIP transaction", id);
        goto fail;
      }

      osip_message_t *request = tr->orig_request;
      status_code = 500;

      // open sockets
      in_port_t audio;
      if unlikely (audio_formats == NULL) {
        audio = 0;
      }
      in_port_t video;
      if (video_formats == NULL) {
        video = 0;
      }
      should (SIPLeelenSession_connect(
          self, audio_formats == NULL ? NULL : &audio,
          video_formats == NULL ? NULL : &video) == 0) otherwise {
        LOG_PERROR(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_ID
                   ": Cannot open sockets", id);
        break;
      }

      // set local tag
      {
        osip_generic_param_t *tag;
        if likely (osip_to_get_tag(request->to, &tag) != OSIP_SUCCESS) {
          char *local_tag = osip_malloc(9);
          goto_if_fail (local_tag != NULL) fail_ist_oom;
          snprintf(local_tag, 9, PRI_LEELEN_ID, id);
          should (osip_to_set_tag(
              request->to, local_tag) == OSIP_SUCCESS) otherwise {
            free(local_tag);
            goto fail_ist_oom;
          }
        }
      }

      int res;

      // prepare sdp
      sdp_message_t *sdp;
      char s_id[sizeof("4294967295")];
      snprintf(s_id, sizeof(s_id), "%" PRIu32, id);
      goto_if_fail (sdp_message_create(
        &sdp, SIPTransactionData_get(tr, out_af), &self->ours, s_id,
        LEELEN2SIP_NAME
      ) == OSIP_SUCCESS) fail_ist_oom;

      if (audio != 0) {
        goto_if_fail (_SIPLeelen_encode_media_formats(
          sdp, "audio", audio, audio_formats) == 0) fail_ist_sdp;
      }
      if (video != 0) {
        goto_if_fail (_SIPLeelen_encode_media_formats(
          sdp, "video", video, video_formats) == 0) fail_ist_sdp;
      }

      char *body;
      res = sdp_message_to_str(sdp, &body);
      sdp_message_free(sdp);
      if (0) {
fail_ist_sdp:
        sdp_message_free(sdp);
        goto fail_ist_oom;
      }
      should (res == OSIP_SUCCESS) otherwise {
        goto_if (res == OSIP_NOMEM) fail_ist_oom;
        LOG(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_ID
            ": Cannot create SDP message: %d", id, res);
        break;
      }

      // create response
      osip_message_t *response;
      goto_if_fail (osip_message_response(
        &response, request, 200, self->device->ua
      ) == OSIP_SUCCESS) fail_ist_body;

      res = osip_message_set_body(response, body, strlen(body));
      osip_free(body);
      if (0) {
fail_ist_body:
        osip_free(body);
        goto fail_ist_oom;
      }
      goto_if_fail (res == OSIP_SUCCESS) fail_ist_response;

      goto_if_fail (osip_message_set_content_type(
        response, "application/sdp") == OSIP_SUCCESS) fail_ist_response;
      goto_if_fail (osip_message_set_expires(
        response, "5;refresher=uac") == OSIP_SUCCESS) fail_ist_response;

      // create dialog
      if (self->sip == NULL) {
        goto_if_fail (osip_dialog_init_as_uas(
          &self->sip, request, response) == OSIP_SUCCESS) fail_ist_response;
      }

      // send reply
      goto_if_fail (osip_transaction_send_sipmessage(
          tr, response, true) == OSIP_SUCCESS) fail_ist_response;

      if (0) {
fail_ist_response:
        osip_message_free(response);
        goto fail_ist_oom;
      }

      goto end;

fail_ist_oom:
      LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_ID ": Out of memory", id);
      break;
    }
  }

  should (osip_transaction_response(
      tr, status_code, self->device->ua, true) == OSIP_SUCCESS) otherwise {
    LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_ID
        ": Out of memory, reply not sent", id);
    goto fail;
  }

  int res = status_code >= 400;
  should (res != 0) otherwise {
fail:
    // end LEELEN dialog
    should (SIPLeelenSession_bye_leelen(self) == 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_ID
                 ": Cannot close LEELEN session", id);
    }
neutral:
    res = 1;
  }

  if (0) {
end:
    res = 0;
  }
  strvfree(audio_formats);
  strvfree(video_formats);
  return res;
}


/**
 * @relates SIPTransactionData
 * @private
 * @brief Process 1XX or 2XX response.
 *
 * @param type Transaction type.
 * @param tr Transaction.
 * @param request OSIP Request.
 */
static void _SIPLeelen_ict_connect (
    int type, osip_transaction_t *tr, osip_message_t *request) {
}


/**
 * @relates SIPTransactionData
 * @private
 * @brief Process 3456XX response.
 *
 * @param type Transaction type.
 * @param tr Transaction.
 * @param request OSIP Request.
 */
static void _SIPLeelen_ict_terminate (
    int type, osip_transaction_t *tr, osip_message_t *request) {
  (void) type;
  struct SIPLeelen *self = tr->your_instance;
  int trid = tr->transactionid;
  struct SIPLeelenSession *session = tr->reserved1;
  leelen_id_t id = session->leelen.id;

  should (SIPLeelenSession_bye_leelen(session) == 0) otherwise {
    LOG_PERROR(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_ID
                ": Cannot close LEELEN session", id);
  }
}


int SIPLeelen_set_uac_callbacks (struct SIPLeelen *self) {
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_1XX_RECEIVED, _SIPLeelen_ict_connect);
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_2XX_RECEIVED, _SIPLeelen_ict_connect);
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_2XX_RECEIVED_AGAIN, _SIPLeelen_ict_connect);
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_3XX_RECEIVED, _SIPLeelen_ict_terminate);
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_4XX_RECEIVED, _SIPLeelen_ict_terminate);
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_5XX_RECEIVED, _SIPLeelen_ict_terminate);
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_6XX_RECEIVED, _SIPLeelen_ict_terminate);
  osip_set_message_callback(
    self->osip, OSIP_ICT_STATUS_3456XX_RECEIVED_AGAIN,
    _SIPLeelen_ict_terminate);
  return 0;
}
