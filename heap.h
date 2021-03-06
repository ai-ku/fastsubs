#ifndef __HEAP_H__
#define __HEAP_H__
#include "token.h"

/* heap is an array of token-logp pairs. */
typedef struct _Hpair {
  Token token;
  float logp;
} Hpair, *Heap;

/* The first element is used to store the count n, so the actual
   elements go from 1..n.  */
#define heap_size(h) ((h)[0].token)
#define heap_top(h) ((h)[1])

/* Note that size is restricted to uint32_t.  We could define size as
   (((uint64_t *)(h))[0]) which relies on Hpair being 8 bytes.
   However since this is specifically a heap for tokens, which are
   represented by uint32_t ids there is no need to have a larger size.
   */

/* The min/max suffixes indicate which element ends up at heap[1] */
/* heap_insert_min, heap_delete_min go together and can be followed by heap_sort_max. */
/* heap_insert_max, heap_delete_max go together and can be followed by heap_sort_min. */

/* This inserts a new element so heap[1] has the min/max logp value. */
void heap_insert_min(Heap heap, Token key, float value);
void heap_insert_max(Heap heap, Token key, float value);

/* This deletes the top element of a min/max heap and fixes the rest of the heap. */
Hpair heap_delete_min(Heap heap);
Hpair heap_delete_max(Heap heap);

/* this sorts the min/max heapified array so heap[1] has the max/min
   logp value.  Note that the order of heap[1] is reversed. */
void heap_sort_max(Heap heap);
void heap_sort_min(Heap heap);

#endif
