#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <inet46i/in46.h>
#include <inet46i/inet46.h>
#include <inet46i/sockaddr46.h>
#include <osip2/osip.h>

#include "leelen2sip.h"
#include "utils/macro.h"
#include "utils/log.h"
#include "ua.h"


static void _LeelenSIP_register (
    int type, osip_transaction_t *tr, osip_message_t *msg) {
  (void) type;

  struct LeelenSIP *self =
    (struct LeelenSIP *) osip_transaction_get_your_instance(tr);
  printf("register! %p\n", (void *) self);
  (void) msg;
}


int LeelenSIP_receive (
    struct LeelenSIP *self, char *buf, int buflen, int socket,
    struct sockaddr *addr) {
  osip_event_t *event = osip_parse(buf, buflen);
  should (event != NULL && event->sip != NULL) otherwise {
    LOGGER(LEELEN2SIP_NAME, LOG_LEVEL_WARNING,
           "Cannot parse SIP message: %.*s", buflen, buf);
    osip_event_free(event);
    return 2;
  }

  if (!MSG_IS_RESPONSE(event->sip) && addr != NULL) {
    void *host;
    in_port_t port = sockaddr46_get(addr, &host);
    char hoststr[INET6_ADDRSTRLEN];
    inet_ntop(
      self->domain == AF_INET6 && IN6_SET_ADDR_V4MAPPED(host) ?
        AF_INET : self->domain, host, hoststr, sizeof(hoststr));
    osip_message_fix_last_via_header(event->sip, hoststr, port);
  }

  if (MSG_IS_REQUEST(event->sip)) {
    osip_transaction_t *tr = osip_create_transaction(self->osip, event);
    should (tr != NULL) otherwise {
      LOGGER(LEELEN2SIP_NAME, LOG_LEVEL_WARNING,
             "Cannot create transaction for SIP message: %.*s", buflen, buf);
      osip_event_free(event);
      return 3;
    }
    osip_transaction_set_your_instance(tr, self);
    osip_transaction_set_out_socket(tr, socket);
  } else {
    should (
        osip_find_transaction_and_add_event(self->osip, event) == OSIP_SUCCESS
    ) otherwise {
      LOGGER(LEELEN2SIP_NAME, LOG_LEVEL_WARNING,
             "Cannot add event of SIP message: %.*s", buflen, buf);
      osip_event_free(event);
      return 3;
    }
  }

  LeelenSIP_execute(self);
  return 0;
}


static int _LeelenSIP_send (
    osip_transaction_t *tr, osip_message_t *msg, char *host, int port,
    int out_socket) {
  char *buf;
  size_t buflen;
  return_if_fail (osip_message_to_str(msg, &buf, &buflen) == 0) 1;

  struct LeelenSIP *self =
    (struct LeelenSIP *) osip_transaction_get_your_instance(tr);
  struct in64_addr addr;
  inet_aton64(host, &addr);
  union sockaddr_in46 sockaddr;
  sockaddr46_set(
    &sockaddr, self->domain, in64_get(self->domain, &addr), port);

  int ret = 0;
  should (sendto(
      out_socket, buf, buflen, 0, &sockaddr.sock, self->domain == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
  ) == (ssize_t) buflen) otherwise {
    perror("sendto");
    ret = 1;
  }
  osip_free(buf);
  return ret;
}


void LeelenSIP_execute (struct LeelenSIP *self) {
  osip_timers_ict_execute(self->osip);
  osip_timers_ist_execute(self->osip);
  osip_timers_nict_execute(self->osip);
  osip_timers_nist_execute(self->osip);
  osip_ict_execute(self->osip);
  osip_ist_execute(self->osip);
  osip_nict_execute(self->osip);
  osip_nist_execute(self->osip);
}


void LeelenSIP_destroy (struct LeelenSIP *self) {
  if (self->ua != NULL) {
    free(self->ua);
  }
  osip_release(self->osip);
}


int LeelenSIP_init (struct LeelenSIP *self, struct LeelenDiscovery *device) {
  return_if_fail (osip_init(&self->osip) == 0) 1;
  osip_set_application_context(self->osip, self);
  osip_set_cb_send_message(self->osip, _LeelenSIP_send);
  osip_set_message_callback(
    self->osip, OSIP_NIST_REGISTER_RECEIVED, _LeelenSIP_register);

  self->ua = NULL;
  self->device = device;
  self->domain = AF_UNSPEC;
  self->socket = -1;
  return 0;
}
