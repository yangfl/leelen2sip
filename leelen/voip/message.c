#include <endian.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "utils/macro.h"
#include "utils/array.h"
#include "utils/log.h"
#include "../config.h"
#include "../family.h"
#include "protocol.h"
#include "message.h"


#define stricmp(s1, s2) strncmp(s1, s2, sizeof(s2) - 1)
#define remain0(total, used) ((total) < (used) ? 0 : (total) - (used))


unsigned int LeelenMessage_tostring (
    const struct LeelenMessage *self, char *buf, unsigned int len) {
  if likely (len >= 4) {
    LEELEN_MESSAGE_CODE(buf) = htole32(self->code);
  }
  if likely (len >= 8) {
    LEELEN_MESSAGE_ID(buf) = self->id;
  }
  unsigned int cur = LEELEN_MESSAGE_HEADER_SIZE;
  cur += snprintf(buf + cur, remain0(len, cur), "From=%s?%d\nTo=%s\n",
                  self->from.str, self->from_type, self->to.str);

  if (self->audio_formats != NULL && self->audio_formats[0] != NULL) {
    for (int i = 0; self->audio_formats[i] != NULL; i++) {
      cur += snprintf(buf + cur, remain0(len, cur), "Audio=%s\n",
                      self->audio_formats[i]);
    }
    cur += snprintf(buf + cur, remain0(len, cur), "AudioPort=%d\n",
                    self->audio_port);
  }

  if (self->video_formats != NULL && self->video_formats[0] != NULL) {
    for (int i = 0; self->video_formats[i] != NULL; i++) {
      cur += snprintf(buf + cur, remain0(len, cur), "Video=%s\n",
                      self->video_formats[i]);
    }
    cur += snprintf(buf + cur, remain0(len, cur), "VideoPort=%d\n",
                    self->video_port);
  }

  return cur;
}


void LeelenMessage_copy_config (
    struct LeelenMessage *self, const struct LeelenConfig *config) {
  self->from = config->number;
  self->from_type = config->type;
  self->audio_port = config->audio;
  self->video_port = config->video;
}


void LeelenMessage_destroy (struct LeelenMessage *self) {
  strvfree(self->audio_formats);
  strvfree(self->video_formats);
}


int LeelenMessage_init_reply (
    struct LeelenMessage *self, const struct LeelenMessage *request,
    int from_type) {
  self->id = request->id;
  self->code = LEELEN_CODE_OK;
  self->from = request->to;
  self->from_type = from_type;
  self->to = request->from;
  self->audio_formats = NULL;
  self->video_formats = NULL;
  self->audio_port = 0;
  self->video_port = 0;
  return 0;
}


int LeelenMessage_init (
    struct LeelenMessage *self, char *msg,
    bool no_audio_formats, bool no_video_formats) {
  self->code = le32toh(LEELEN_MESSAGE_CODE(msg));
  self->id = LEELEN_MESSAGE_ID(msg);

  self->audio_formats = NULL;
  self->video_formats = NULL;
  int n_audio_format = 0;
  int n_video_format = 0;
  for (char *saved, *line =
        strtok_r(msg + LEELEN_MESSAGE_HEADER_SIZE, "\n", &saved); line != NULL;
       line = strtok_r(NULL, "\n", &saved)) {
    if (stricmp(line, "From=") == 0) {
      line += strlen("From=");
      char *type_sep = strchr(line, '?');
      unsigned int number_len =
        type_sep != NULL ? (unsigned int) (type_sep - line) : strlen(line);
      // number
      if unlikely (number_len >= sizeof(self->to)) {
        LOG(LOG_LEVEL_INFO, "Source phone number '%.*s' too long",
            number_len, line);
        self->from.str[0] = '\0';
      } else {
        memcpy(self->from.str, line, number_len);
        self->from.str[number_len] = '\0';
      }
      // type
      if (type_sep == NULL || type_sep[1] == '\0') {
        LOG(LOG_LEVEL_INFO,
            "From description '%s' does not contain device type", line);
      } else {
        type_sep++;
        char *end;
        self->from_type = strtol(type_sep, &end, 10);
        should (*end == '\0') otherwise {
          LOG(LOG_LEVEL_INFO, "Cannot parse device type '%s'", type_sep);
          self->from_type = LEELEN_DEVICE_UNKNOWN;
        }
      }
    } else if (stricmp(line, "To=") == 0) {
      line += strlen("To=");
      unsigned int number_len = strlen(line);
      if unlikely (number_len >= sizeof(self->from)) {
        LOG(LOG_LEVEL_INFO, "Destination phone number '%.*s' too long",
            number_len, line);
      } else {
        memcpy(self->to.str, line, number_len);
        self->to.str[number_len] = '\0';
      }
    } else if (stricmp(line, "Audio=") == 0) {
      if (!no_audio_formats) {
        line += strlen("Audio=");
        goto_if_fail (
          strvpush(&self->audio_formats, &n_audio_format, line) >= 0) fail;
      }
    } else if (stricmp(line, "AudioPort=") == 0) {
      line += strlen("AudioPort=");
      char *end;
      self->audio_port = strtol(line, &end, 10);
      should (*end == '\0') otherwise {
        LOG(LOG_LEVEL_INFO, "Cannot parse audio port '%s'", line);
        self->audio_port = 0;
      }
    } else if (stricmp(line, "Video=") == 0) {
      if (!no_video_formats) {
        line += strlen("Video=");
        goto_if_fail (
          strvpush(&self->video_formats, &n_video_format, line) >= 0) fail;
      }
    } else if (stricmp(line, "VideoPort=") == 0) {
      line += strlen("VideoPort=");
      char *end;
      self->video_port = strtol(line, &end, 10);
      should (*end == '\0') otherwise {
        LOG(LOG_LEVEL_INFO, "Cannot parse video port '%s'", line);
        self->video_port = 0;
      }
    } else if (stricmp(line, "Resolution=") == 0) {
      line += strlen("Resolution=");
    } else {
      LOG(LOG_LEVEL_INFO, "Unknown description '%s'", line);
    }
  }

  return 0;

fail:
  LeelenMessage_destroy(self);
  return -1;
}
