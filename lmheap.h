#ifndef __LMHEAP_H__
#define __LMHEAP_H__
#include "lm.h"

typedef struct _LMheapS {
  GHashTable *logP_heap;
  GHashTable *logB_heap;
  LM lm;
} *LMheap;

typedef struct _LMpair {
  Token token;
  gfloat logp;
} LMpair;

LMheap lmheap_init(LM lm);
void lmheap_free(LMheap h);
#define heap_size(h) ((h)[0].token)

#endif
