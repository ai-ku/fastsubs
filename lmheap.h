#ifndef __LMHEAP_H__
#define __LMHEAP_H__
#include "lm.h"
#include "heap.h"

typedef GHashTable *LMheap;

LMheap lmheap_init(LM lm);
std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual> CPPlmheap_init(LM lm); 
 
#endif
