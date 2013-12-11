#include <stdio.h>
#include "minialloc.h"
#include "token.h"
#include "ngram.h"
#include "lm.h"
#include "lmheap.h"
#include "heap.h"

#define NDOT 100000
#define dot() if (++ndot % NDOT == 0) fprintf(stderr, ".")
static gsize ndot;

static void CPPlmheap_count(
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>* hash,
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>& h
    ) {
  for(auto it = hash->begin(); it != hash->end(); ++ it) {
    Ngram ng = it->first;
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      auto s = h.find(ng);
      if (s == h.end()) {
        Ngram ngcopy = ngram_dup(ng);
        guint32 * iptr = static_cast<guint32*>(minialloc(sizeof(guint32)));
        *iptr = 1;
        h.emplace(ngcopy, iptr);
      } else {
        guint32* iptr = static_cast<guint32*>(s->second);
        (*iptr)++;
      }
      ng[i] = ng_i;
    }
    dot();
  }
}

static void CPPlmheap_alloc(
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>& h
    ) {
  for(auto it = h.begin(); it != h.end(); ++ it) {
    guint32 *iptr = static_cast<guint32*>(it->second);
    Heap heap = static_cast<Heap>(minialloc(sizeof(Hpair) * (1 + (*iptr))));
    heap_size(heap) = 0;
    it->second = heap;
    dot();
  }
} 

static void CPPlmheap_insert(
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>* hash,
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>& h
    ) {
  for(auto it = hash->begin(); it != hash->end(); ++ it) {
    Ngram ng = it->first; 
    gfloat *fptr = static_cast<gfloat*>(it->second);
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      auto heapIt = h.find(ng);
      ng[i] = ng_i;
      g_assert(heapIt != h.end());
      heap_insert_min(heapIt->second, ng_i, *fptr);
    }
    dot();
  }
}

static void CPPlmheap_sort(
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>& hash) {
  for(auto it = hash.begin(); it != hash.end(); ++ it) {
    Heap heap = static_cast<Heap>(it->second);
    g_assert(heap != NULL && heap_size(heap) > 0);
    heap_sort_max(heap);
    dot();
  }
}

std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual> 
CPPlmheap_init(LM lm) {
  g_message("lmheap_init start");
  std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual> CPPh;
  g_message("count logP");
  CPPlmheap_count(lm->CPPlogP, CPPh);
  g_message("alloc logP_heap");
  CPPlmheap_alloc(CPPh);
  g_message("insert logP");
  CPPlmheap_insert(lm->CPPlogP, CPPh);
  g_message("sort logP_heap");
  CPPlmheap_sort(CPPh);
  g_message("lmheap_init done");
  return CPPh;
}

