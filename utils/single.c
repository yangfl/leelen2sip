#include <stdatomic.h>
#include <threads.h>

#include "utils/macro.h"
#include "single.h"


extern inline void single_stop (single_flag *flag);
extern inline int thrd_execute (thrd_start_t func, void *arg);


void _single_join (single_flag *flag) {
  while unlikely (single_still_running(flag)) {
    thrd_yield();
  }
}


bool single_acquire (single_flag *flag) {
  while (1) {
    // if stopped, try to start
    signed char old_flag = SINGLE_STOPPED;
    // test if lock acquired; in that case `flag == SINGLE_STOPPED`
    return_if (likely (atomic_compare_exchange_weak(
      flag, &old_flag, SINGLE_RUNNING))) true;
    // return if already running
    return_if_fail (!single_is_running(&old_flag)) false;
    // wait fully stopped
    while unlikely (*flag < 0) {
      thrd_yield();
    }
  }
}


int single_run (single_flag *flag, thrd_start_t func, void *arg) {
  return_if_fail (single_acquire(flag)) -1;
  return func(arg);
}


int single_start (single_flag *flag, thrd_start_t func, void *arg) {
  return_if_fail (single_acquire(flag)) 0;
  thrd_t thread;
  should (thrd_create(&thread, func, arg) == thrd_success) otherwise {
    single_finish(flag);
    return -1;
  }
  thrd_detach(thread);
  return 0;
}
