#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <inet46i/in46.h>
#include <osipparser2/osip_port.h>
#include <osipparser2/sdp_message.h>

#include "utils/macro.h"
#include "sdp_message.h"


struct RTPPayloadTypeMap {
  unsigned char type;
  const char *format;
};


static const struct RTPPayloadTypeMap audio_format_maps[] = {
  {0, "PCMU/8000"},
  {3, "GSM/8000"},
  {4, "G723/8000"},
  {5, "DVI4/8000"},
  {6, "DVI4/16000"},
  {7, "LPC/8000"},
  {8, "PCMA/8000"},
  {9, "G722/8000"},
  // {10, "L16/44100"},  // 2 channels
  {11, "L16/44100"},  // 1 channels
  {12, "QCELP/8000"},
  {13, "CN/8000"},
  {14, "MPA/90000"},
  {15, "G728/8000"},
  {16, "DVI4/11025"},
  {17, "DVI4/22050"},
  {18, "G729/8000"},
  {33, "MP2T/90000"},
  {127, "telephone-event/8000"},
};
static const struct RTPPayloadTypeMap video_format_maps[] = {
  {25, "CelB/90000"},
  {26, "JPEG/90000"},
  {28, "nv/90000"},
  {31, "H261/90000"},
  {32, "MP/90000"},
  {33, "MP2T/90000"},
  {34, "H263/90000"},
};


int rtpformat2type (const char *format, int *dynamic_type, int media) {
  const struct RTPPayloadTypeMap *format_maps;
  int size;
  switch (media) {
    case 0:
      format_maps = audio_format_maps;
      size = arraysize(audio_format_maps);
      break;
    case 1:
      format_maps = video_format_maps;
      size = arraysize(video_format_maps);
      break;
    default:
      goto fail;
  }

  for (int i = 0; i < size; i++) {
    return_if (strcmp(format, format_maps[i].format) == 0) format_maps[i].type;
  }

fail:
  int type = *dynamic_type;
  (*dynamic_type)++;
  return type;
}


int sdp_media_add_attribute (
    sdp_media_t *media, int type, const char *field, const char *value) {
  sdp_attribute_t *attr;
  int res = sdp_attribute_init(&attr);
  return_if_fail (res == OSIP_SUCCESS) res;

  char *a_att_field = osip_strdup(field);
  goto_if_fail (a_att_field != NULL) fail;
  attr->a_att_field = a_att_field;

  char *a_att_value;
  if unlikely (type < 0) {
    a_att_value = osip_strdup(value);
  } else {
    int size = sizeof("4294967295") + 1 + strlen(value);
    a_att_value = osip_malloc(size);
    if likely (a_att_value != NULL) {
      snprintf(a_att_value, size, "%d %s", type, value);
    }
  }
  goto_if_fail (a_att_value != NULL) fail;
  attr->a_att_value = a_att_value;

  goto_if_fail (osip_list_add(&media->a_attributes, attr, -1) > 0) fail;
  return OSIP_SUCCESS;

fail:
  sdp_attribute_free(attr);
  return OSIP_NOMEM;
}


int sdp_message_create (
    sdp_message_t **sdp, int af, const void *addr, const char *id,
    const char *name) {
  return_if_fail (af == AF_INET || af == AF_INET6) OSIP_BADPARAMETER;

  int res = sdp_message_init(sdp);
  return_if_fail (res == OSIP_SUCCESS) res;

  {
    char *v_version = osip_strdup("0");
    goto_if_fail (v_version != NULL) fail;
    sdp_message_v_version_set(*sdp, v_version);
  }

  if (af == AF_INET6 && IN6_IS_ADDR_V4MAPPED(addr)) {
    af = AF_INET;
    addr = ((struct in6_addr *) addr)->s6_addr32 + 3;
  }
  const char *addrtype = af == AF_INET6 ? "IP6" : "IP4";
  char host[INET6_ADDRSTRLEN];
  inet_ntop(af, addr, host, sizeof(host));

  {
    char *o_username = osip_strdup("-");
    char *o_sess_id = osip_strdup(id);
    char *o_sess_version = osip_strdup(id);
    char *o_nettype = osip_strdup("IN");
    char *o_addrtype = osip_strdup(addrtype);
    char *o_addr = osip_strdup(host);
    sdp_message_o_origin_set(*sdp, o_username, o_sess_id, o_sess_version,
                              o_nettype, o_addrtype, o_addr);
    goto_if_fail (o_username != NULL) fail;
    goto_if_fail (o_sess_id != NULL) fail;
    goto_if_fail (o_sess_version != NULL) fail;
    goto_if_fail (o_nettype != NULL) fail;
    goto_if_fail (o_addrtype != NULL) fail;
    goto_if_fail (o_addr != NULL) fail;
  }
  {
    char *s_name = osip_strdup(name);
    goto_if_fail (s_name != NULL) fail;
    sdp_message_s_name_set(*sdp, s_name);
  }
  {
    char *c_nettype = osip_strdup("IN");
    char *c_addrtype = osip_strdup(addrtype);
    char *c_addr = osip_strdup(host);
    should (
        c_nettype != NULL && c_addrtype != NULL && c_addr != NULL &&
        sdp_message_c_connection_add(
          *sdp, -1, c_nettype, c_addrtype, c_addr, NULL, NULL
        ) == OSIP_SUCCESS) otherwise {
      osip_free(c_nettype);
      osip_free(c_addrtype);
      osip_free(c_addr);
      goto fail;
    }
  }
  {
    char *t_start_time = osip_strdup("0");
    char *t_stop_time = osip_strdup("0");
    should (
        t_start_time != NULL && t_stop_time != NULL &&
        sdp_message_t_time_descr_add(
          *sdp, t_start_time, t_stop_time) == OSIP_SUCCESS) otherwise {
      osip_free(t_start_time);
      osip_free(t_stop_time);
      goto fail;
    }
  }

  return OSIP_SUCCESS;

fail:
  sdp_message_free(*sdp);
  return OSIP_NOMEM;
}
