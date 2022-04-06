#define USE_LOG

#ifndef USE_LOG
#include <stdio.h>
#include <stdlib.h>
#endif

#ifdef USE_LOG
#include "log.h"
#endif
#include "refcount.h"


int refcount_inc_overflow (const void *self) {
#ifdef USE_LOG
  LOG(LOG_LEVEL_CRITICAL, "%p: reference counter overflow", self);
#else
  fprintf(stderr, "%p: reference counter overflow", self);
  exit(255);
#endif
  return 1;
}
