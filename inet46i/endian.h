#ifndef INET46I_ENDIAN_H
#define INET46I_ENDIAN_H

#include <stdint.h>

/**
 * @file
 * Macro version of @c htons() and @c htonl().
 */


/**
 * @def lhtons(x)
 * @brief Macro version of @c htons().
 */

/**
 * @def lhtonl(x)
 * @brief Macro version of @c htonl().
 */

#define __inet46i_move_byte(type, x, from, to) \
  ((((type) (x) >> ((from) * 8)) & 0xff) << ((to) * 8))
#if __BYTE_ORDER == __BIG_ENDIAN
  #define lhtons(x) (x)
  #define lhtonl(x) (x)
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #define lhtons(x) ( \
    __inet46i_move_byte(uint16_t, x, 0, 1) | \
    __inet46i_move_byte(uint16_t, x, 1, 0)   )
  #define lhtonl(x) ( \
    __inet46i_move_byte(uint32_t, x, 0, 3) | \
    __inet46i_move_byte(uint32_t, x, 1, 2) | \
    __inet46i_move_byte(uint32_t, x, 2, 1) | \
    __inet46i_move_byte(uint32_t, x, 3, 0)   )
#endif


#endif /* INET46I_ENDIAN_H */
