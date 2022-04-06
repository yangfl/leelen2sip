#ifndef SIPLEELEN_TRANSACTION_H
#define SIPLEELEN_TRANSACTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <netinet/in.h>
#include <sys/time.h>  // osip

#include <osip2/osip.h>
#include <osipparser2/sdp_message.h>

// #include "session.h"
struct SIPLeelenSession;
// #include "sipleelen.h"
struct SIPLeelen;


/**
 * @ingroup sip
 * @brief Auxiliary structure for osip_transaction_t.
 * @note osip_transaction::your_instance stores SIPLeelen.
 */
struct SIPTransactionData {
  /// session associated with this transaction
  struct SIPLeelenSession *session;
  /// address family of osip_transaction_t::out_socket
  unsigned char out_af;
};

_Static_assert(sizeof(struct SIPTransactionData) <= sizeof(void *) + 5 * 4,
               "SIPTransactionData cannot fit into reversed zone");

/**
 * @memberof SIPTransactionData
 * @brief Get SIPTransactionData attribute in osip_transaction_t.
 *
 * @param tr Transaction.
 * @param name SIPTransactionData member name.
 * @return SIPTransactionData attribute.
 */
#define SIPTransactionData_get(tr, name) \
  ((struct SIPTransactionData *) (&(tr)->reserved1))->name


__attribute__((nonnull(1, 2), access(read_only, 2), access(read_only, 4)))
/**
 * @relates SIPTransactionData
 * @brief Get audio and video description from SDP in request.
 *
 * @param sdp OSIP SDP.
 * @param media Media type.
 * @param port Port number.
 * @param formats Array of formats.
 * @return 0 on success, -1 if out of memory.
 */
int _SIPLeelen_encode_media_formats (
  sdp_message_t *sdp, const char *media, in_port_t port, char * const *formats);
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
