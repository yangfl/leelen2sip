#include <endian.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/socket.h>

#include <inet46i/sockaddr46.h>
#include <osipparser2/sdp_message.h>

#include "utils/macro.h"
#include "utils/array.h"
#include "utils/log.h"
#include "utils/osip.h"
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
        "Dialog " PRI_LEELEN_CODE
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
        "Dialog " PRI_LEELEN_CODE ": Cannot process LEELEN SIP message", id);
      return 1;
    case 254:
      LOG(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_CODE
          ": LEELEN ACK got, but timeout reached", id);
      return 1;
  }

  int status_code;
  osip_transaction_t *tr = atomic_exchange(&self->transaction, NULL);

  switch (code) {
    case LEELEN_CODE_OK: {
      if (old_state == LEELEN_DIALOG_DISCONNECTING) {
        // destroy SIPLeelenSession
        if (tr != NULL) {
          SIPTransactionData_get(tr, session) = self;
        }
        // no need to reply
        goto end;
      }

      goto_if_fail (old_state == LEELEN_DIALOG_CONNECTING) end;

      should (tr != NULL) otherwise {
        LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_CODE
            ": connection establish without SIP transaction?", id);
        goto fail;
      }

      osip_message_t *request = tr->orig_request;
      status_code = 500;

      // get my ip address
      char *host;
      {
        osip_via_t *via = osip_list_get(&request->vias, 0);
        should (via != NULL && via->host != NULL) otherwise {
          LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_CODE ": No VIA?", id);
          break;
        }
        host = via->host;
      }

      // open sockets
      in_port_t audio;
      in_port_t video;
      should (SIPLeelenSession_connect(self, &audio, &video) == 0) otherwise {
        LOG_PERROR(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_CODE
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
      goto_if_fail (sdp_message_init(&sdp) == OSIP_SUCCESS) fail_ist_oom;

      {
        char *v_version = osip_strdup("0");
        goto_if_fail (v_version != NULL) fail_ist_sdp;
        sdp_message_v_version_set(sdp, v_version);
      }
      {
        char *o_username = osip_strdup("-");
        char *o_sess_id = osip_malloc(sizeof("4294967295"));
        if likely (o_sess_id != NULL) {
          snprintf(o_sess_id, sizeof("4294967295"), "%u", id);
        }
        char *o_sess_version = osip_strdup(o_sess_id);
        char *o_nettype = osip_strdup("IN");
        char *o_addrtype = osip_strdup(
          strchr(host, ':') != NULL ? "IP6" : "IP4");
        char *o_addr = osip_strdup(host);
        sdp_message_o_origin_set(sdp, o_username, o_sess_id, o_sess_version,
                                 o_nettype, o_addrtype, o_addr);
        goto_if_fail (
          o_username != NULL && o_sess_id != NULL && o_sess_version != NULL &&
          o_nettype != NULL && o_addrtype != NULL && o_addr != NULL
        ) fail_ist_sdp;
      }
      {
        char *s_name = osip_strdup(LEELEN2SIP_NAME);
        goto_if_fail (s_name != NULL) fail_ist_sdp;
        sdp_message_s_name_set(sdp, s_name);
      }
      {
        char *t_start_time = osip_strdup("0");
        char *t_stop_time = osip_strdup("0");
        sdp_message_t_time_descr_add(sdp, t_start_time, t_stop_time);
        goto_if_fail (t_start_time != NULL && t_stop_time != NULL) fail_ist_sdp;
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
        LOG(LOG_LEVEL_INFO, "Dialog " PRI_LEELEN_CODE
            ": Cannot create SDP message: %d", id, res);
        break;
      }

      puts(body);
      exit(0);

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
      LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_CODE
          ": Out of memory", id);
      break;
    }
    case LEELEN_CODE_CALL:
    case LEELEN_CODE_VIEW:
    case LEELEN_CODE_VOICE_MESSAGE:
    case LEELEN_CODE_ACCEPTED:
    default:
      goto fail;
  }

  should (osip_transaction_response(
      tr, status_code, self->device->ua, true) == OSIP_SUCCESS) otherwise {
    LOG(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_CODE
        ": Out of memory, reply not sent", id);
    goto fail;
  }

  int res = status_code >= 400;
  should (res != 0) otherwise {
fail:
    res = 1;

    // end LEELEN dialog
    should (LeelenDialog_may_bye(
        &self->leelen, self->device->socket_leelen) >= 0) otherwise {
      LOG_PERROR(LOG_LEVEL_WARNING, "Dialog " PRI_LEELEN_CODE
                 ": Cannot close LEELEN session", id);
    }

    // destroy SIPLeelenSession
    if (tr != NULL) {
      SIPTransactionData_get(tr, session) = self;
    }
  }

  if (0) {
end:
    res = 0;
  }
  strvfree(audio_formats);
  strvfree(video_formats);
  return res;
}
