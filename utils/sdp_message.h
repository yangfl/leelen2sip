#ifndef UTILS_SDPMESSAGE_H
#define UTILS_SDPMESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <osipparser2/sdp_message.h>


#define RTP_PAYLOAD_DYNAMIC_TYPE 96

__attribute__((nonnull, access(read_only, 1), access(read_write, 2)))
int rtpformat2type (const char *format, int *dynamic_type, int media);

__attribute__((nonnull, access(read_write, 1), access(read_only, 3),
               access(read_only, 4)))
int sdp_media_add_attribute (
  sdp_media_t *media, int type, const char *field, const char *value);

__attribute__((nonnull, access(write_only, 1), access(read_only, 3),
               access(read_only, 4), access(read_only, 5)))
int sdp_message_create (
  sdp_message_t **sdp, int af, const void *addr, const char *id,
  const char *name);


#ifdef __cplusplus
}
#endif

#endif /* UTILS_SDPMESSAGE_H */
