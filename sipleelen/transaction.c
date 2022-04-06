#include <ctype.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>  // osip

#include <osip2/osip.h>
#include <osipparser2/sdp_message.h>

#include "utils/macro.h"
#include "utils/array.h"
#include "utils/log.h"
#include "utils/sdp_message.h"
#include "utils/osip.h"
#include "leelen/config.h"
#include "session.h"
#include "sipleelen.h"
#include "transaction.h"



int _SIPLeelen_encode_media_formats (
    sdp_message_t *sdp, const char *media, in_port_t port,
    char * const *formats) {
  return_if_fail (port != 0) 0;

  sdp_media_t *med;
  return_if_fail (sdp_media_init(&med) == OSIP_SUCCESS) 1;

  {
    char *m_media = osip_strdup(media);
    goto_if_fail (m_media != NULL) fail;
    med->m_media = m_media;
  }
  {
    char *m_port = osip_malloc(sizeof("65535"));
    goto_if_fail (m_port != NULL) fail;
    med->m_port = m_port;
    snprintf(m_port, sizeof("65535"), "%u", port);
  }
  {
    char *m_proto = osip_strdup("RTP/AVP");
    goto_if_fail (m_proto != NULL) fail;
    med->m_proto = m_proto;
  }

  int media_type =
    strcmp(media, "audio") == 0 ? 0 :
    strcmp(media, "video") == 0 ? 1 :
    2;

  for (int dynamic_type = RTP_PAYLOAD_DYNAMIC_TYPE; *formats != NULL;
       formats++) {
    const char *format = *formats;
    int type = rtpformat2type(format, &dynamic_type, media_type);
    continue_if_fail (type != 127);

    char *m_payload = osip_malloc(sizeof("4294967295"));
    goto_if_fail (m_payload != NULL) fail;
    snprintf(m_payload, sizeof("4294967295"), "%d", type);
    should (osip_list_add(&med->m_payloads, m_payload, -1) > 0) otherwise {
      osip_free(m_payload);
      goto fail;
    }

    goto_if_fail (sdp_media_add_attribute(
      med, type, "rtpmap", format) == OSIP_SUCCESS) fail;
  }

  goto_if_fail (sdp_media_add_attribute(
    med, 127, "rtpmap", "telephone-event/8000") == OSIP_SUCCESS) fail;
  goto_if_fail (sdp_media_add_attribute(
    med, 127, "fmtp", "0-15") == OSIP_SUCCESS) fail;

  goto_if_fail (osip_list_add(&sdp->m_medias, med, -1) > 0) fail;
  return 0;

fail:
  sdp_media_free(med);
  return 1;
}


int _SIPLeelen_extract_media_formats (
    osip_message_t *request, char ***audio_formats, char ***video_formats) {
  sdp_message_t *sdp;
  return_if_fail (osip_message_get_sdp(request, &sdp) == OSIP_SUCCESS) 1;

  char **audio_formats_ = NULL;
  int n_audio_format = 0;
  char **video_formats_ = NULL;
  int n_video_format = 0;

  for (int i = 0;; i++) {
    sdp_media_t *media = osip_list_get(&sdp->m_medias, i);
    break_if_fail (media != NULL);

    continue_if_not (media->m_media != NULL && media->m_proto != NULL);
    continue_if_not (strcmp(media->m_proto, "RTP/AVP") == 0);

    bool is_audio = strcmp(media->m_media, "audio") == 0;
    bool is_video = strcmp(media->m_media, "video") == 0;
    should (is_audio || is_video) otherwise {
      LOG(LOG_LEVEL_INFO, "Unknown media type '%s'", media->m_media);
      continue;
    }
    continue_if_not ((is_audio ? audio_formats : video_formats) != NULL);

    for (int j = 0; !osip_list_eol(&media->a_attributes, j); j++) {
      sdp_attribute_t *a = osip_list_get(&media->a_attributes, j);
      continue_if_not (a->a_att_field != NULL && a->a_att_value != NULL);
      continue_if_not (strcmp(a->a_att_field, "rtpmap") == 0);

      const char *sep = strchr(a->a_att_value, ' ');
      continue_if_not (sep != NULL);
      while (isspace(*sep)) {
        sep++;
      }
      continue_if_not (*sep != '\0');

      should (strvpush(
          is_audio ? &audio_formats_ : &video_formats_,
          is_audio ? &n_audio_format : &n_video_format, sep
      ) >= 0) otherwise {
        strvfree(audio_formats_);
        strvfree(video_formats_);
        sdp_message_free(sdp);
        return -1;
      }
    }
  }

  if (audio_formats != NULL) {
    *audio_formats = audio_formats_;
  }
  if (video_formats != NULL) {
    *video_formats = video_formats_;
  }
  sdp_message_free(sdp);
  return 0;
}


/**
 * @relates SIPTransactionData
 * @brief Callback called when a SIP transaction is terminated.
 *
 * @param type Transaction type.
 * @param tr Transaction.
 */
static void _SIPLeelen_kill_transaction (int type, osip_transaction_t *tr) {
  struct SIPLeelen *self = tr->your_instance;

  LOG(LOG_LEVEL_DEBUG, "Transaction %d: Terminate %s transaction",
      tr->transactionid, osip_fsm_type_names[type]);

  struct SIPLeelenSession *session = tr->reserved1;
  if (session != NULL) {
    osip_transaction_t *cur_tr = tr;
    atomic_compare_exchange_strong(&session->transaction, &cur_tr, NULL);
    SIPLeelen_decref_session(self, session, -1, 1);
  }

  osip_remove_transaction(self->osip, tr);
}


int SIPLeelen_set_uax_callbacks (struct SIPLeelen *self) {
  osip_set_kill_transaction_callback(
    self->osip, OSIP_ICT_KILL_TRANSACTION, _SIPLeelen_kill_transaction);
  osip_set_kill_transaction_callback(
    self->osip, OSIP_IST_KILL_TRANSACTION, _SIPLeelen_kill_transaction);
  osip_set_kill_transaction_callback(
    self->osip, OSIP_NICT_KILL_TRANSACTION, _SIPLeelen_kill_transaction);
  osip_set_kill_transaction_callback(
    self->osip, OSIP_NIST_KILL_TRANSACTION, _SIPLeelen_kill_transaction);
  return 0;
}
