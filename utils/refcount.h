#ifndef UTILS_REFCOUNT_H
#define UTILS_REFCOUNT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include <stdbool.h>

/**
 * @file
 * Reference counting.
 */


/// Reference counting type.
typedef atomic_uint refcount_t;

__attribute__((nonnull))
/**
 * @brief Test if the object should be freed.
 *
 * @param counter Reference counter.
 * @return @c true if the object should be freed.
 */
static inline bool refcount_should_free (refcount_t *counter) {
  unsigned int old_count = 0;
  atomic_compare_exchange_strong(counter, &old_count, 1);
  return __builtin_expect(old_count == 0, 1);
}

__attribute__((nonnull))
/**
 * @brief Decrease reference counter and test if the object should be freed.
 *
 * @param counter Reference counter.
 * @return @c true if the object should be freed.
 */
static inline bool refcount_dec (refcount_t *counter) {
  return __builtin_expect(--*counter == 0, 0) && refcount_should_free(counter);
}

__attribute__((access(read_only, 1)))
/**
 * @brief Reference counter overflow handler.
 *
 * @param self Object.
 * @return 1.
 */
int refcount_inc_overflow (const void *self);

__attribute__((nonnull(1), access(read_only, 2)))
/**
 * @brief Increase reference counter of an object.
 *
 * @param counter Reference counter.
 * @param self Object.
 * @return 0 if reference counter does not overflow, otherwise 1.
 */
static inline int refcount_inc (refcount_t *counter, const void *self) {
  return __builtin_expect(++*counter > 0, 1) ? 0 : refcount_inc_overflow(self);
}


#ifdef __cplusplus
}
#endif

#endif /* UTILS_REFCOUNT_H */
