#ifndef SIPLEELEN_TRANSACTION_H
#define SIPLEELEN_TRANSACTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <sys/time.h>  // osip

#include <osip2/osip.h>
#include <osipparser2/sdp_message.h>

// #include "session.h"
struct SIPLeelenSession;
// #include "sipleelen.h"
struct SIPLeelen;


/**
 * @ingroup sip
 * @brief Auxiliary structure for osip_dialog_t.
 * @note osip_transaction::your_instance stores SIPLeelen.
 */
struct SIPTransactionData {
  /// if not @c NULL, free this session after transaction ends
  struct SIPLeelenSession *session;
  /// address family of osip_dialog_t::out_socket
  unsigned char out_af;
};

_Static_assert(sizeof(struct SIPTransactionData) <= 6 * 4,
               "SIPTransactionData cannot fit into reversed zone");

/**
 * @memberof SIPTransactionData
 * @brief Get SIPTransactionData attribute in osip_dialog_t.
 *
 * @param tr osip_dialog_t object.
 * @param name SIPTransactionData member name.
 * @return SIPTransactionData attribute.
 */
#define SIPTransactionData_get(tr, name) \
  ((struct SIPTransactionData *) (&(tr)->reserved1))->name


__attribute__((nonnull(1), access(write_only, 2), access(write_only, 3)))
/**
 * @relates SIPTransactionData
 * @brief Get audio and video description from SDP in request.
 *
 * @param request OSIP Request.
 * @param[out] audio_formats Parsed audio description. Can be @c NULL.
 * @param[out] video_formats Parsed video description. Can be @c NULL.
 * @return 0 on success, 1 if no SDP in request, -1 if out of memory.
 */
int _SIPLeelen_extract_media_formats (
  osip_message_t *request, char ***audio_formats, char ***video_formats);

__attribute__((nonnull))
/**
 * @relates SIPTransactionData
 * @brief Callback called when a SIP transaction is created.
 *
 * @param type Transaction type.
 * @param tr Transaction.
 * @return 0 on success, error otherwise.
 */
int _SIPLeelen_init_transaction (int type, osip_transaction_t *tr);

__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Add callbacks for transaction termination.
 *
 * @param self LEELEN2SIP object.
 * @return 0.
 */
int SIPLeelen_set_uax_callbacks (struct SIPLeelen *self);


#ifdef __cplusplus
}
#endif

#endif /* SIPLEELEN_TRANSACTION_H */
