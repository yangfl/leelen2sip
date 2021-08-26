#ifndef UTILS_ARRAY_H
#define UTILS_ARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "macro.h"


#define forindex(type, i, arr, len) \
  for (type i = 0; i < (len); i++) \
    if unlikely ((arr)[i] == NULL) { } else

__attribute__((warn_unused_result, nonnull, access(read_only, 1)))
inline size_t ptrvlen (const void *arr) {
  for (int i = 0;; i++) {
    return_if_fail (((void **) arr)[i] != NULL) i;
  }
}

__attribute__((warn_unused_result, nonnull(1), access(read_only, 1),
               access(read_only, 3)))
inline int ptrvfind (const void *arr, int len, const void *ele) {
  for (int i = 0; i < len; i++) {
    return_if (((void **) arr)[i] == ele) i;
  }
  return -1;
}

__attribute__((warn_unused_result, nonnull, access(write_only, 2)))
inline int ptrvrealloc (
    void * __restrict parr, int * __restrict plen, int new_len) {
  void **arr = realloc(*(void ***) parr, (new_len + 1) * sizeof(void *));
  return_if_fail (arr != NULL) -1;
  *(void ***) parr = arr;
  *plen = new_len;
  arr[new_len] = NULL;
  return new_len;
}

__attribute__((warn_unused_result, nonnull))
inline int ptrvgrow (void * __restrict parr, int * __restrict plen, int diff) {
  return ptrvrealloc(parr, plen, *plen + diff);
}

__attribute__((nonnull))
inline int ptrvshrink (void * __restrict parr, int * __restrict plen) {
  return_if_not (*plen > 0 && (*(void ***) parr)[*plen] == NULL) *plen;
  int new_len;
  for (new_len = *plen - 1;
       new_len > 0 && (*(void ***) parr)[new_len] == NULL; new_len--) { }
  return ptrvrealloc(parr, plen, new_len);
}

__attribute__((warn_unused_result, nonnull, access(read_only, 3)))
inline int ptrvpush (
    void * __restrict parr, int * __restrict plen,
    const void * __restrict ele) {
  int i = *plen;
  return_if_fail (ptrvgrow(parr, plen, 1) >= 0) -1;
  (*(void ***) parr)[i] = (void *) ele;
  return i;
}

__attribute__((warn_unused_result, nonnull, access(read_only, 3)))
inline int strvpush (
    void * __restrict parr, int * __restrict plen,
    const char * __restrict str) {
  char *str_ = strdup(str);
  return_if_fail (str_ != NULL) -1;
  int i = ptrvpush(parr, plen, str_);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
  should (i >= 0) otherwise {
    free(str_);
  }
#pragma GCC diagnostic pop
  return i;
}

__attribute__((warn_unused_result, nonnull))
inline int ptrvreserve (void * __restrict parr, int * __restrict plen) {
  for (int i = 0; i < *plen; i++) {
    return_if ((*(void ***) parr)[i] == NULL) i;
  }
  int i = *plen;
  return_if_fail (ptrvgrow(parr, plen, 1) >= 0) -1;
  return i;
}

__attribute__((warn_unused_result, nonnull, access(read_only, 3)))
inline int ptrvinsert (
    void * __restrict parr, int * __restrict plen,
    const void * __restrict ele) {
  int i = ptrvreserve(parr, plen);
  if likely (i >= 0) {
    (*(void ***) parr)[i] = (void *) ele;
  }
  return i;
}

__attribute__((warn_unused_result, nonnull))
inline int ptrvcreate (
    void * __restrict parr, int * __restrict plen, size_t size) {
  void *ele = malloc(size);
  return_if_fail (ele != NULL) -1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  int i = ptrvinsert(parr, plen, ele);
#pragma GCC diagnostic pop
  should (i >= 0) otherwise {
    free(ele);
    return -1;
  }
  return i;
}

#define PTRVCREATE(parr, plen, type) ptrvcreate(parr, plen, sizeof(type))

__attribute__((nonnull))
inline void ptrvsteal (void * __restrict parr, int * __restrict plen, int i) {
  return_if_fail (i >= 0);
  (*(void ***) parr)[i] = NULL;
  if (i == *plen - 1) {
    ptrvshrink(parr, plen);
  }
}

__attribute__((nonnull, access(read_only, 3)))
inline void ptrvtake (void *parr, int * __restrict plen, const void *ele) {
  ptrvsteal(parr, plen, ptrvfind(*(void ***) parr, *plen, ele));
}

inline void strvfree (void *arr) {
  return_if_fail (arr != NULL);
  for (int i = 0; ((void **) arr)[i] != NULL; i++) {
    promise (((void **) arr)[i] != ((void **) arr)[i + 1]);
    free(((void **) arr)[i]);
  }
  free(arr);
}

inline void ptrvfree (void *arr, int len, void (*destroyer) (void *)) {
  return_if_fail (arr != NULL);
  forindex (int, i, (void **) arr, len) {
    void *ele = ((void **) arr)[i];
    if (destroyer != NULL) {
      destroyer(ele);
    }
    free(ele);
  }
  free(arr);
}

#define PTRVFREE(arr, len, destroyer) \
  ptrvfree(arr, len, (void (*) (void *)) destroyer)


#ifdef __cplusplus
}
#endif

#endif /* UTILS_ARRAY_H */
