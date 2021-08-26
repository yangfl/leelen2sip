#ifndef UTILS_TIMEVAL_H
#define UTILS_TIMEVAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <time.h>

/** @file */


__attribute__((const, warn_unused_result))
/**
 * @brief Test if timeout reached.
 *
 * @param tv_sec_end Whole seconds of end time.
 * @param tv_nsec_end Nanoseconds of end time.
 * @param tv_sec_beg Whole seconds of begin time.
 * @param tv_nsec_beg Nanoseconds of begin time.
 * @param tv_sec_timeout Whole seconds of timeout.
 * @param tv_nsec_timeout Nanoseconds of timeout.
 * @return @c true if timeout reached.
 */
static inline bool timeout_reached (
    time_t tv_sec_end, long tv_nsec_end, time_t tv_sec_beg, long tv_nsec_beg,
    time_t tv_sec_timeout, long tv_nsec_timeout) {
  time_t tv_sec_interval = tv_sec_end - tv_sec_beg;
  long tv_nsec_interval = tv_nsec_end - tv_nsec_beg;
  if (tv_nsec_interval < 0) {
    tv_sec_interval--;
    tv_nsec_interval += 1000000000;
  }
  if (tv_nsec_interval < 0) {
    __builtin_unreachable();
  }
  return
    tv_sec_interval > tv_sec_timeout || (
      tv_sec_interval == tv_sec_timeout && tv_nsec_interval >= tv_nsec_timeout);
}


#ifdef __cplusplus
}
#endif

#endif /* UTILS_TIMEVAL_H */
