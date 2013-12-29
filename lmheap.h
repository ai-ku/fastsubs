#ifndef __LMHEAP_H__
#define __LMHEAP_H__
#include "lm.h"
#include "heap.h"

typedef darr_t LMheap;
typedef struct NH_s { Ngram key; Heap val; } *NH_t;

LMheap lmheap_init(LM lm);
void lmheap_free(LMheap lh);
NH_t lmheap_get(LMheap h, Ngram key);

#endif
