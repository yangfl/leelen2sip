#ifndef LEELEN_NUMBER_H
#define LEELEN_NUMBER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "family.h"


/**
 * @ingroup leelen
 * @brief Leelen phone number.
 *
 * The phone number is in the format XXXX-XXXX (without extension) or
 * XXXX-XXXX-X (with extension).
 */
struct LeelenNumber {
  union {
    /// full phone number string
    char str[LEELEN_NUMBER_STRLEN];
    struct {
      /// block part
      char block[4];
      /// always '-'
      char sep1;
      /// rome part
      char room[4];
      /// '\0' if no LeelenNumber::extension, otherwise '-'
      char sep2;
      /// extension part if `>= 0 && < 10`
      char extension;
      /// always '\0'
      char sep3;
    };
  };
};

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1)))
/**
 * @memberof LeelenNumber
 * @brief Test if two phone numbers are equal.
 *
 * @param self Phone number.
 * @param other Phone number.
 * @return @c true if equal.
 */
static inline bool LeelenNumber_equal (
    const struct LeelenNumber * __restrict self,
    const struct LeelenNumber * __restrict other) {
  return
    memcmp(self->block, other->block, sizeof(self->block)) == 0 &&
    memcmp(self->room, other->room, sizeof(self->room)) == 0 &&
    self->sep2 == other->sep2 && (
      self->sep2 != '\0' || self->extension == other->extension);
}

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1)))
/**
 * @memberof LeelenNumber
 * @brief Test if a phone number has extension.
 *
 * @param self Phone number.
 * @return @c true if the phone number has extension.
 */
static inline bool LeelenNumber_has_extension (
    const struct LeelenNumber *self) {
  return (self->sep2 | self->extension) != '\0';
}
__attribute__((pure, warn_unused_result, nonnull,
               access(read_only, 1), access(read_only, 2)))
/**
 * @memberof LeelenNumber
 * @brief Test if a phone request should be answered.
 *
 * @param self Phone number.
 * @param request Phone request.
 * @return @c true if the phone request should be answered.
 */
static inline bool LeelenNumber_should_reply (
    const struct LeelenNumber *self, const char *request) {
  return memcmp(self->str, request, 9) == 0;
}

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1)))
/**
 * @memberof LeelenNumber
 * @brief Pack phone number into a single integer.
 *
 * @param phone Phone number.
 * @return Converted integer.
 */
uint32_t LeelenNumber_toint (const struct LeelenNumber *phone);
__attribute__((returns_nonnull, nonnull, access(write_only, 1)))
/**
 * @memberof LeelenNumber
 * @brief Unpack integer into phone number.
 *
 * @param[out] dest Phone number.
 * @param src Integer to unpack.
 * @return `dest->str`.
 */
char *LeelenNumber_fromint (struct LeelenNumber *dest, uint32_t src);
__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1)))
/**
 * @memberof LeelenNumber
 * @brief Convert phone number to 5-byte format.
 *
 * @param phone Phone number.
 * @return 5-byte format phone number.
 */
uint64_t LeelenNumber_tobytes (const struct LeelenNumber *phone);
__attribute__((returns_nonnull, nonnull, access(write_only, 1)))
/**
 * @memberof LeelenNumber
 * @brief Convert 5-byte format to phone number.
 *
 * @param[out] dest Phone number.
 * @param src 5-byte format phone number.
 * @return `dest->str`.
 */
char *LeelenNumber_frombytes (struct LeelenNumber *dest, uint64_t src);

__attribute__((nonnull(1, 2), warn_unused_result, access(write_only, 1),
               access(read_only, 2), access(read_only, 3)))
/**
 * @memberof LeelenNumber
 * @brief Parse phone number, based on the given base.
 *
 * Parse phone number. Accepts:
 *   - block-room[-extension]
 *   - room[-extension] (block number is extracted from the base)
 *   - [block]room[extension]
 * Delimiters can be any non-digit character.
 *
 * @param[out] dest Parsed phone number.
 * @param src Phone number to parse, can be `dest->str`.
 * @param base Base phone number, can be @c NULL.
 * @return 0 if success, otherwise 1.
 */
int LeelenNumber_init (
  struct LeelenNumber *dest, const char *src, const struct LeelenNumber *base);


#ifdef __cplusplus
}
#endif

#endif /* LEELEN_NUMBER_H */
