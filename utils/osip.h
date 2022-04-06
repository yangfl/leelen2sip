#ifndef UTILS_OSIP_H
#define UTILS_OSIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <sys/time.h>  // osip

#include <osip2/osip.h>
#include <osip2/osip_dialog.h>
#include <osipparser2/sdp_message.h>


// fsm

extern const char * const osip_fsm_type_names[4];

// message

__attribute__((nonnull))
bool osip_message_has_sdp (osip_message_t *message);
__attribute__((nonnull))
int osip_message_get_sdp (osip_message_t *message, sdp_message_t **sdp);

__attribute__((nonnull))
int osip_message_set_now (osip_message_t *sip);
__attribute__((nonnull))
int osip_message_set_status_code_and_reason_phrase (
  osip_message_t *sip, int status);

__attribute__((nonnull(1, 2), access(write_only, 1), access(read_only, 2),
               access(read_only, 4)))
int osip_message_response (
  osip_message_t **response, const osip_message_t *request, int status_code,
  const char *ua);
__attribute__((nonnull(1, 2, 3), access(write_only, 1), access(read_only, 3),
               access(read_only, 4)))
int osip_message_request (
  osip_message_t **request, osip_dialog_t *dialog, const char *method,
  const char *ua);

// transaction

__attribute__((nonnull, warn_unused_result))
bool osip_transaction_match (
  osip_transaction_t *transaction, osip_message_t *message);

__attribute__((nonnull))
static inline int osip_transaction_send_sipmessage (
    osip_transaction_t *transaction, osip_message_t *sip, bool now) {
  osip_event_t *event = osip_new_outgoing_sipmessage(sip);
  if (__builtin_expect(event == NULL, 0)) {
    return OSIP_UNDEFINED_ERROR;
  }

  if (now) {
    event->transactionid = transaction->transactionid;
    // osip_transaction_execute returns 1 if event got consumed
    osip_transaction_execute(transaction, event);
    return OSIP_SUCCESS;
  } else {
    // BUG of osip: osip_transaction_add_event may fail when OOM, but the return
    // value is missing
    return osip_transaction_add_event(transaction, event);
  }
}

__attribute__((warn_unused_result, nonnull(1), access(read_only, 3)))
int osip_transaction_response (
  osip_transaction_t *transaction, int status_code, const char *ua, bool now);

// dialog

__attribute__((nonnull(1), access(read_only, 2)))
int osip_dialog_set_local_tag (osip_dialog_t *dialog, const char *local_tag);


#ifdef __cplusplus
}
#endif

#endif /* UTILS_OSIP_H */
