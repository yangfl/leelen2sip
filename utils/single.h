#ifndef UTILS_SINGLE_H
#define UTILS_SINGLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

/** @file */


enum single_flag_state {
  /// daemon is about to stop
  SINGLE_STOPPING = -1,
  /// daemon is not running
  SINGLE_STOPPED = 0,
  /// daemon is currently running
  SINGLE_RUNNING = 1,
};

/// type capable of holding a flag used by single_start()
typedef volatile signed char single_flag;

/// ::single_flag initializer
#define SINGLE_FLAG_INIT 0

/**
 * @brief Tese if new daemon thread can be created.
 *
 * @param flag Single flag.
 * @return @c true if new daemon thread can be created.
 */
#define single_still_running(flag) (*(flag) != 0)
/**
 * @brief Tese if daemon thread is currently running.
 *
 * @param flag Single flag.
 * @return @c true if daemon thread is currently running.
 */
#define single_is_running(flag) (*(flag) > 0)

/**
 * @brief Indicate this daemon is stopped.
 *
 * @code{.c}
  while (single_is_running(&flag)) {
    // do something
  }
  // clean up
  single_finish(&flag);
  return 0;
 * @endcode
 *
 * @param[out] flag Single flag.
 * @see single_continue()
 */
#define single_finish(flag) (*(flag) = SINGLE_STOPPED)

__attribute__((always_inline, nonnull))
/**
 * @brief Test whether the daemon should continue running.
 *
 * @code{.c}
  while (single_continue(&flag)) {
    // do something
  }
  return 0;
 * @endcode
 *
 * @param[in,out] flag Single flag.
 * @return @c true if the daemon should continue running, @c false if not and
 *  the daemon is considered to be fully stopped.
 */
static inline bool single_continue (single_flag *flag) {
  if (__builtin_expect(single_is_running(flag), 1)) {
    return true;
  }
  single_finish(flag);
  return false;
}

__attribute__((nonnull, access(read_only, 1)))
/**
 * @brief Block the current thread until the daemon stops.
 *
 * This function does a busy wait, so it should be used only when the daemon is
 * about to quit.
 *
 * @param flag Single flag.
 */
void _single_join (single_flag *flag);

__attribute__((always_inline, nonnull, access(read_only, 1)))
/**
 * @brief Block the current thread until the daemon stops.
 *
 * This function does a busy wait, so it should be used only when the daemon is
 * about to quit.
 *
 * @code{.c}
  single_stop(&flag);
  single_join(&flag);
  // now the daemon is fully stopped
 * @endcode
 *
 * @param flag Single flag.
 */
static inline void single_join (single_flag *flag) {
  if (__builtin_expect(single_is_running(flag), 0)) {
    _single_join(flag);
  }
}

__attribute__((nonnull))
/**
 * @brief Stop the daemon.
 *
 * This function does not wait for the daemon to stop.
 *
 * @param[in,out] flag Single flag.
 */
inline void single_stop (single_flag *flag) {
  signed char running = SINGLE_RUNNING;
  atomic_compare_exchange_strong(flag, &running, SINGLE_STOPPING);
}

__attribute__((nonnull(1, 2)))
/**
 * @brief Start function in foreground.
 *
 * @param[in,out] flag Single flag.
 * @param func Function to start in background.
 * @param arg Argument to pass to the function.
 * @return 0 on success, -1 if daemon already running.
 */
int single_run (single_flag *flag, thrd_start_t func, void *arg);

__attribute__((nonnull(1, 2)))
/**
 * @brief Start function in background, promise that only one thread will be
 *  running at the same time, even if invoked from several threads.
 *
 * @code{.c}
  #include <stdio.h>
  #include <threads.h>
  #include <unistd.h>

  #include "single.h"

  static single_flag flag = SINGLE_FLAG_INIT;

  int daemon_func (void *data) {
    int i = 0;
    while (single_continue(&flag)) {
      printf("Print at %d sec\n", i);
      sleep(1);
      i++;
    }
    return 0;
  }

  int func (void *data) {
    int n = (int) data;
    sleep(n);
    printf("%d: Starting daemon\n", n);
    single_start(&flag, daemon_func, NULL);
    sleep(n * 2);
    printf("%d: Quitting\n", n);
    return 0;
  }

  int main (void) {
    thrd_t t1, t2, t3, t4;
    thrd_create(&t1, func, (void *) 1);
    thrd_create(&t2, func, (void *) 2);
    thrd_create(&t3, func, (void *) 3);
    thrd_create(&t4, func, (void *) 4);

    thrd_join(t1, NULL);
    thrd_join(t2, NULL);
    thrd_join(t3, NULL);
    thrd_join(t4, NULL);

    single_stop(&flag);
    single_join(&flag);
  }
 * @endcode
 *
 * @param[in,out] flag Single flag.
 * @param func Function to start in background.
 * @param arg Argument to pass to the function.
 * @return 0 on success or daemon already running, -1 on error.
 */
int single_start (single_flag *flag, thrd_start_t func, void *arg);

__attribute__((nonnull(1)))
/**
 * @brief Create a new thread executing the function @p func.
 *
 * @param func Function to start in background.
 * @param arg Argument to pass to the function.
 * @return thrd_success on success, error otherwise.
 */
inline int thrd_execute (thrd_start_t func, void *arg) {
  thrd_t thread;
  int res = __builtin_expect(thrd_create(&thread, func, arg), thrd_success);
  if (res == thrd_success) {
    thrd_detach(thread);
  }
  return res;
}


#ifdef __cplusplus
}
#endif

#endif /* UTILS_SINGLE_H */
