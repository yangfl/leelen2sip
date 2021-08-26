#include <stddef.h>
#include <string.h>
#include <sys/time.h>  // osip

#include <osip2/osip.h>
#include <osipparser2/sdp_message.h>

#include "utils/macro.h"
#include "utils/array.h"
#include "utils/log.h"
#include "utils/osip.h"
#include "leelen/config.h"
#include "session.h"
#include "sipleelen.h"
#include "transaction.h"


#define TRANSACTION_NAME "SIPLeelenTransaction"


int _SIPLeelen_extract_media_formats (
    osip_message_t *request, char ***audio_formats, char ***video_formats) {
  sdp_message_t *sdp;
  return_if_fail (osip_message_get_sdp(request, &sdp) == OSIP_SUCCESS) 1;

  char **audio_formats_ = NULL;
  int n_audio_format = 0;
  char **video_formats_ = NULL;
  int n_video_format = 0;
  for (int i = 0; !osip_list_eol(&sdp->m_medias, i); i++) {
    sdp_media_t *media = osip_list_get(&sdp->m_medias, i);
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
      while (*sep == ' ') {
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


int _SIPLeelen_init_transaction (int type, osip_transaction_t *tr) {
  struct SIPLeelen *self = tr->your_instance;

  switch (type) {
    case IST: {
      SIPLeelen_lock_sessions(self);
      int i = SIPLeelen_create_session(self);
      SIPTransactionData_get(tr, session) = self->sessions[i];
      SIPLeelen_unlock_sessions(self);
      return_if_fail (i >= 0) -1;
      break;
    }
  }

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

  struct SIPLeelenSession *session = SIPTransactionData_get(tr, session);
  if (session != NULL) {
    SIPLeelen_lock_sessions(self);
    SIPLeelen_remove_session(self, session);
    SIPLeelen_unlock_sessions(self);
    SIPLeelenSession_destroy(session);
  }

  // remove transaction
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
