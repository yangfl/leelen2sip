#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/macro.h"
#include "family.h"
#include "number.h"


union LeelenNumberInt {
  struct {
    uint32_t block : 14;
    uint32_t room : 14;
    uint32_t extension : 4;
  };
  uint32_t int_;
};


uint32_t LeelenNumber_toint (const struct LeelenNumber *src) {
  union LeelenNumberInt packed = {
    .block = atoi(src->block),
    .room = atoi(src->room),
    .extension = LeelenNumber_has_extension(src) ? src->extension - '0' : 0xf
  };
  return packed.int_;
}


static char *itoa4 (int value, char str[static 4]) {
  str[3] = '0' + (value % 10);
  value /= 10;
  str[2] = '0' + (value % 10);
  value /= 10;
  str[1] = '0' + (value % 10);
  value /= 10;
  str[0] = '0' + (value % 10);
  return str;
}


char *LeelenNumber_fromint (struct LeelenNumber *dest, uint32_t src) {
  union LeelenNumberInt packed = {.int_ = src};
  itoa4(packed.block, dest->block);
  dest->sep1 = '-';
  itoa4(packed.room, dest->room);
  if (packed.extension >= 10) {
    dest->sep2 = '\0';
  } else {
    dest->sep2 = '-';
    dest->extension = '0' + packed.extension;
    dest->sep3 = '\0';
  }
  return dest->str;
}


union LeelenNumberBytes {
  struct {
    unsigned char extension;
    unsigned char room[2];
    unsigned char block[2];
  };
  uint64_t longlong;
};


static unsigned char int2byte (const char src[2]) {
  return (src[0] - '0') * 10 + (src[1] - '0');
}


uint64_t LeelenNumber_tobytes (const struct LeelenNumber *phone) {
  union LeelenNumberBytes packed = {
    .extension = LeelenNumber_has_extension(phone) ?
                 phone->extension - '0' : 0xff,
    .room[0] = int2byte(phone->room + 2),
    .room[1] = int2byte(phone->room),
    .block[0] = int2byte(phone->block + 2),
    .block[1] = int2byte(phone->block),
  };
  return packed.longlong;
}


static void byte2int (char *dest, uint64_t src) {
  dest[0] = src / 10 + '0';
  dest[1] = src % 10 + '0';
}


char *LeelenNumber_frombytes (struct LeelenNumber *dest, uint64_t src) {
  union LeelenNumberBytes packed = {.longlong = src};
  byte2int(dest->block, packed.block[0]);
  byte2int(dest->block + 2, packed.block[1]);
  dest->sep1 = '-';
  byte2int(dest->room, packed.room[0]);
  byte2int(dest->room + 2, packed.room[1]);
  if (packed.extension == 0xff) {
    dest->sep2 = '\0';
  } else {
    dest->sep2 = '-';
    dest->extension = packed.extension + '0';
    dest->sep3 = '\0';
  }
  return dest->str;
}


int LeelenNumber_init (
    struct LeelenNumber *dest, const char *src,
    const struct LeelenNumber *base) {
  return_if_fail (isdigit(src[0])) 1;

  int n_sep = 0;
  int i_sep[2];
  int src_len = -1;
  for (int i = 0; src[i] != '\0'; i++) {
    // too long
    return_if_fail (i < LEELEN_NUMBER_STRLEN - 1) 1;
    src_len = i;
    if (!isdigit(src[i])) {
      i_sep[n_sep] = i;
      n_sep++;
      // to many separators
      return_if_fail (n_sep <= 2) 1;
    }
  }
  src_len++;
  return_if_fail (src_len > 0) 1;
  // .*-$
  if (n_sep != 0) {
    return_if_fail (src[i_sep[n_sep - 1] + 1] != '\0') 1;
  }

  char block[8] = {'0', '0', '0', '0'};
  char room[8] = {'0', '0', '0', '0'};
  char extension = '\0';

  switch (n_sep) {
    case 2:
      // block-room-extension
      // .*--.*
      return_if_fail (i_sep[0] + 1 != i_sep[1]) 1;
      // .*-.*-..+
      return_if_fail (src[i_sep[1] + 2] == '\0') 1;
      extension = src[i_sep[1] + 1];
      goto block_room;
    case 1:
      if (src[i_sep[0] + 2] != '\0') {
        // block-room
        i_sep[1] = strlen(src);
block_room:
        return_if_fail (i_sep[0] <= 4 && i_sep[1] - i_sep[0] - 1 <= 4) 1;
        memcpy(block + 4 - i_sep[0], src, 4);
        memcpy(room + 4 - (i_sep[1] - i_sep[0] - 1), src + i_sep[0] + 1,
               4);
        break;
      }
      // [block]room-extension
      extension = src[i_sep[0] + 1];
      src_len -= 2;
      return_if_fail (src_len <= 4 || src_len == 8) 1;
      // fall through
    case 0:
      if (src_len <= 5) {
        // room[extension]
        return_if_fail (base != NULL) 2;
        memcpy(block, base, 4);
        if (src_len <= 4) {
          memcpy(room + 4 - src_len, src, 4);
        } else {
          memcpy(room, src, 4);
          extension = src[4];
        }
      } else if (src_len == 8 || src_len == 9) {
        // blockroom[extension]
        memcpy(block, src, 4);
        memcpy(room, src + 4, 4);
        if (src_len == 9) {
          extension = src[8];
        }
      } else {
        return 1;
      }
      break;
  }

  memcpy(dest->block, block, 4);
  dest->sep1 = '-';
  memcpy(dest->room, room, 4);
  if (extension == '\0') {
    dest->sep2 = '\0';
  } else {
    dest->sep2 = '-';
    dest->extension = extension;
    dest->sep3 = '\0';
  }
  return 0;
}
