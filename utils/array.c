#include <stddef.h>

#include "array.h"


extern inline size_t ptrvlen (const void *arr);
extern inline int ptrvfind (const void *arr, int len, const void *ele);
extern inline int ptrvrealloc (
  void * __restrict parr, int * __restrict plen, int new_len);
extern inline int ptrvgrow (
  void * __restrict parr, int * __restrict plen, int diff);
extern inline int ptrvshrink (void * __restrict parr, int * __restrict plen);
extern inline int ptrvpush (
  void * __restrict parr, int * __restrict plen,
  const void * __restrict ele);
extern inline int strvpush (
  void * __restrict parr, int * __restrict plen,
  const char * __restrict str);
extern inline int ptrvreserve (void * __restrict parr, int * __restrict plen);
extern inline int ptrvinsert (
  void * __restrict parr, int * __restrict plen,
  const void * __restrict ele);
extern inline int ptrvcreate (
  void * __restrict parr, int * __restrict plen, size_t str);
extern inline void ptrvsteal (
  void * __restrict parr, int * __restrict plen, int i);
extern inline void ptrvtrysteal (
  void * __restrict parr, int * __restrict plen, int i);
extern inline void ptrvtake (
  void *parr, int * __restrict plen, const void *ele);
extern inline void strvfree (void *arr);
extern inline void ptrvfree (void *arr, int len, void (*destroyer) (void *));
