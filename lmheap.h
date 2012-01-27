#ifndef __LMHEAP_H__
#define __LMHEAP_H__
#include "lm.h"
#include "heap.h"

typedef struct _LMheapS {
  GHashTable *logP_heap;
  GHashTable *logB_heap;
  LM lm;
} *LMheap;

LMheap lmheap_init(LM lm);
Hpair *lmheap_logP(LMheap h, Ngram ng);
Hpair *lmheap_logB(LMheap h, Ngram ng);

#endif
