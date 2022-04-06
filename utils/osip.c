#include <time.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>  // osip

#include <osip2/osip.h>
#include <osipparser2/sdp_message.h>

#include "macro.h"
#include "osip.h"


const char * const osip_fsm_type_names[4] = {"ICT", "IST", "NICT", "NIST"};


bool osip_message_has_sdp (osip_message_t *message) {
  osip_content_type_t *ctt = osip_message_get_content_type(message);
  return ctt != NULL && ctt->type != NULL && ctt->subtype != NULL && (
      strcasecmp(ctt->type, "multipart") == 0 || (
        strcasecmp(ctt->type, "application") == 0 &&
        strcasecmp(ctt->subtype, "sdp") == 0)
    ) && osip_list_size(&message->bodies) > 0;
}


int osip_message_get_sdp (osip_message_t *message, sdp_message_t **sdp) {
  for (int i = 0;; i++) {
    osip_body_t *body = osip_list_get(&message->bodies, i);
    break_if_fail (body != NULL);

    int res = sdp_message_init(sdp);
    return_if_fail (res == OSIP_SUCCESS) res;
    return_if (sdp_message_parse(
      *sdp, body->body) == OSIP_SUCCESS) OSIP_SUCCESS;
    sdp_message_free(*sdp);
  }
  return OSIP_UNDEFINED_ERROR;
}


int osip_message_set_now (osip_message_t *sip) {
  time_t now = time(NULL);
  struct tm tm;
  gmtime_r(&now, &tm);
  char buf[32];
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
  return osip_message_set_date(sip, buf);
}


int osip_message_set_status_code_and_reason_phrase (
    osip_message_t *sip, int status) {
  sip->status_code = status;

  {
    char *reason_phrase = osip_strdup(osip_message_get_reason(status));
    return_if_fail (reason_phrase != NULL) OSIP_NOMEM;
    sip->reason_phrase = reason_phrase;
  }

  return OSIP_SUCCESS;
}


int osip_message_response (
    osip_message_t **response, const osip_message_t *request, int status_code,
    const char *ua) {
  return_if_fail (request->from != NULL) OSIP_SYNTAXERROR;
  return_if_fail (request->to != NULL) OSIP_SYNTAXERROR;
  return_if_fail (request->call_id != NULL) OSIP_SYNTAXERROR;
  return_if_fail (request->cseq != NULL) OSIP_SYNTAXERROR;

  int ret = osip_message_init(response);
  return_if_fail (ret == OSIP_SUCCESS) ret;

  goto_if_fail (osip_message_set_status_code_and_reason_phrase(
    *response, status_code) == OSIP_SUCCESS) fail;
  goto_if_fail (osip_list_clone(
    &request->vias, &(*response)->vias,
    (int (*) (void *, void **)) osip_via_clone) == OSIP_SUCCESS) fail;
  goto_if_fail (osip_from_clone(
    request->from, &(*response)->from) == OSIP_SUCCESS) fail;
  goto_if_fail (
    osip_to_clone(request->to, &(*response)->to) == OSIP_SUCCESS) fail;

#if 0
  if (status_code > 100 && dialog != NULL && dialog->local_tag != NULL) {
    osip_generic_param_t *tag;
    if (osip_to_get_tag((*response)->to, &tag) != OSIP_SUCCESS) {
      char *local_tag_ = osip_strdup(dialog->local_tag);
      goto_if_fail (local_tag_ != NULL) fail;
      osip_to_set_tag((*response)->to, local_tag_);
    }
  }
#endif

  goto_if_fail (osip_call_id_clone(
    request->call_id, &(*response)->call_id) == OSIP_SUCCESS) fail;
  goto_if_fail (
    osip_cseq_clone(request->cseq, &(*response)->cseq) == OSIP_SUCCESS) fail;
  goto_if_fail (osip_list_clone(
    &request->contacts, &(*response)->contacts,
    (int (*) (void *, void **)) osip_contact_clone) == OSIP_SUCCESS) fail;

  goto_if_fail (
    osip_message_set_now(*response) == OSIP_SUCCESS) fail;
  goto_if_fail (
    osip_message_set_max_forwards(*response, "70") == OSIP_SUCCESS) fail;
  if (ua != NULL) {
    goto_if_fail (
      osip_message_set_user_agent(*response, ua) == OSIP_SUCCESS) fail;
    goto_if_fail (
      osip_message_set_server(*response, ua) == OSIP_SUCCESS) fail;
  }
  return OSIP_SUCCESS;

fail:
  osip_message_free(*response);
  *response = NULL;
  return OSIP_NOMEM;
}


int osip_message_request (
    osip_message_t **request, osip_dialog_t *dialog, const char *method,
    const char *ua) {
  return_if_fail (dialog->local_uri != NULL) OSIP_SYNTAXERROR;
  return_if_fail (dialog->remote_uri != NULL) OSIP_SYNTAXERROR;
  return_if_fail (dialog->remote_contact_uri != NULL) OSIP_SYNTAXERROR;
  return_if_fail (dialog->call_id != NULL) OSIP_SYNTAXERROR;

  int res = osip_message_init(request);
  return_if_fail (res == OSIP_SUCCESS) res;

  {
    char *method_ = osip_strdup(method);
    goto_if_fail (method_ != NULL) fail;
    (*request)->sip_method = method_;
  }

  {
    char via[64];
    snprintf(
      via, 64, "SIP/2.0/UDP 0:0;rport;branch=z9hG4bK%u",
      osip_build_random_number());
    goto_if_fail (osip_message_set_via(*request, via) == OSIP_SUCCESS) fail;
  }
  goto_if_fail (osip_from_clone(
    dialog->local_uri, &(*request)->from) == OSIP_SUCCESS) fail;
  goto_if_fail (
    osip_to_clone(dialog->remote_uri, &(*request)->to) == OSIP_SUCCESS) fail;

  goto_if_fail (osip_message_set_call_id(
    *request, dialog->call_id) == OSIP_SUCCESS) fail;
  goto_if_fail (osip_cseq_init(&(*request)->cseq) == OSIP_SUCCESS) fail;
  {
    static const int number_size = sizeof("-2147483647");
    char *number = osip_malloc(number_size);
    goto_if_fail (number != NULL) fail;
    snprintf(number, number_size, "%d", dialog->local_cseq);
    osip_cseq_set_number((*request)->cseq, number);
  }
  {
    char *method_ = osip_strdup(method);
    goto_if_fail (method_ != NULL) fail;
    osip_cseq_set_method((*request)->cseq, method_);
  }
  if (strcmp(method, "ACK") != 0) {
    dialog->local_cseq++;
  }
  if (osip_list_eol(&dialog->route_set, 0)) {
    // The UAC must put the remote target URI (to field) in the req_uri
    goto_if_fail (osip_uri_clone(
      dialog->remote_contact_uri->url, &(*request)->req_uri
    ) == OSIP_SUCCESS) fail;
  } else {
    // fill the request-uri, and the route headers
    /* if the pre-existing route set contains a "lr" (compliance
       with bis-08) then the req_uri should contains the remote target
       URI */
    osip_list_iterator_t it;
    // bug: should be "const osip_list_t *"
    osip_route_t *route = osip_list_get_first(&dialog->route_set, &it);

    osip_uri_param_t *lr_param;
    osip_uri_uparam_get_byname(route->url, "lr", &lr_param);

    if (lr_param != NULL) {
      /* the remote target URI is the req_uri! */
      goto_if_fail (osip_uri_clone(
        dialog->remote_contact_uri->url, &(*request)->req_uri
      ) == OSIP_SUCCESS) fail;
      /* "[request] MUST includes a Route header field containing
         the route set values in order." */
      for (; route != NULL; route = osip_list_get_next(&it)) {
        osip_route_t *route_;
        goto_if_fail (osip_route_clone(route, &route_) == OSIP_SUCCESS) fail;
        should (osip_list_add(
            &(*request)->routes, route_, -1) == OSIP_SUCCESS) otherwise {
          osip_route_free(route_);
          goto fail;
        }
      }
    } else {
      /* if the first URI of route set does not contain "lr", the req_uri
         is set to the first uri of route set */
      goto_if_fail (osip_uri_clone(
        route->url, &(*request)->req_uri) == OSIP_SUCCESS) fail;
      /* add the route set */
      /* "The UAC MUST add a route header field containing
         the remainder of the route set values in order. */
      /* yes it is, skip first */
      while (1) {
        route = osip_list_get_next(&it);
        break_if_fail (route != NULL);
        osip_route_t *route_;
        goto_if_fail (osip_route_clone(route, &route_) == OSIP_SUCCESS) fail;
        should (osip_list_add(
            &(*request)->routes, route_, -1) == OSIP_SUCCESS) otherwise {
          osip_route_free(route_);
          goto fail;
        }
      }
      /* The UAC MUST then place the remote target URI into
         the route header field as the last value */
      goto_if_fail (osip_route_init(&route) == OSIP_SUCCESS) fail;
      osip_uri_free(route->url);
      goto_if_fail (osip_uri_clone(
        dialog->remote_contact_uri->url, &route->url) == OSIP_SUCCESS) fail;
      should (osip_list_add(
          &(*request)->routes, route, -1) == OSIP_SUCCESS) otherwise {
        osip_route_free(route);
        goto fail;
      }
    }
  }

  goto_if_fail (
    osip_message_set_now(*request) == OSIP_SUCCESS) fail;
  goto_if_fail (
    osip_message_set_max_forwards(*request, "70") == OSIP_SUCCESS) fail;
  if (ua != NULL) {
    goto_if_fail (
      osip_message_set_user_agent(*request, ua) == OSIP_SUCCESS) fail;
  }

  return OSIP_SUCCESS;

fail:
  osip_message_free(*request);
  *request = NULL;
  return OSIP_NOMEM;
}


bool osip_transaction_match (
    osip_transaction_t *transaction, osip_message_t *message) {
  osip_generic_param_t *tr_br;
  osip_via_param_get_byname(transaction->topvia, "branch", &tr_br);

  osip_via_t *via = osip_list_get(&message->vias, 0);
  return_if_fail (via != NULL) false;
  osip_generic_param_t *msg_br;
  osip_via_param_get_byname(via, "branch", &msg_br);

  return_if_fail ((tr_br != NULL) == (msg_br != NULL)) false;

  return tr_br != NULL ?
    tr_br->gvalue != NULL && msg_br->gvalue != NULL &&
      strcmp(tr_br->gvalue, msg_br->gvalue) == 0 :
    osip_call_id_match(transaction->callid, message->call_id) == OSIP_SUCCESS &&
      osip_to_tag_match(transaction->to, message->to) == OSIP_SUCCESS &&
      osip_from_tag_match(transaction->from, message->from) == OSIP_SUCCESS &&
      osip_via_match(transaction->topvia, via) == OSIP_SUCCESS;
}


int osip_transaction_response (
    osip_transaction_t *transaction, int status_code, const char *ua,
    bool now) {
  osip_message_t *response;
  int res = osip_message_response(
    &response, transaction->orig_request, status_code, ua);
  return_if_fail (res == OSIP_SUCCESS) res;
  goto_if_fail (osip_transaction_send_sipmessage(
    transaction, response, now) == OSIP_SUCCESS) fail;
  return OSIP_SUCCESS;

fail:
  osip_message_free(response);
  return OSIP_NOMEM;
}


int osip_dialog_set_local_tag (osip_dialog_t *dialog, const char *local_tag) {
  osip_free(dialog->local_tag);

  if (local_tag == NULL) {
    // create one
    static const int local_tag_len = sizeof("ffffffff");
    dialog->local_tag = osip_malloc(local_tag_len);
    return_if_fail (dialog->local_tag != NULL) OSIP_NOMEM;
    snprintf(dialog->local_tag, local_tag_len, "%u",
             osip_build_random_number());
  } else {
    dialog->local_tag = osip_strdup(dialog->local_tag);
    return_if_fail (dialog->local_tag != NULL) OSIP_NOMEM;
  }
  return OSIP_SUCCESS;
}
