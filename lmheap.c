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
    std::unordered_map<Ngram, gfloat*, CPPNgramHash, CPPNgramEqual>* hash,
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
        guint32 * iptr = minialloc(sizeof(guint32));
        *iptr = 1;
        h.emplace(ngcopy, iptr);
      } else {
        guint32* iptr = s->second;
        (*iptr)++;
      }
      ng[i] = ng_i;
    }
    dot();
  }
}

static void lmheap_count(gpointer key, gpointer value, gpointer user_data) {
  Ngram ng = (Ngram) key;
  GHashTable *h = user_data;
  for (int i = ngram_size(ng); i > 0; i--) {
    Token ng_i = ng[i];
    ng[i] = NULLTOKEN;
    guint32* iptr = g_hash_table_lookup(h, ng);
    if (iptr == NULL) {
      Ngram ngcopy = ngram_dup(ng);
      iptr = minialloc(sizeof(guint32));
      *iptr = 1;
      g_hash_table_insert(h, ngcopy, iptr);
    } else {
      (*iptr)++;
    }
    ng[i] = ng_i;
  }
  dot();
}

static void CPPlmheap_alloc(
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>& h
    ) {
  for(auto it = h.begin(); it != h.end(); ++ it) {
    Ngram ng = it->first;
    guint32 *iptr = it->second;
    Heap heap = minialloc(sizeof(Hpair) * (1 + (*iptr)));
    heap_size(heap) = 0;
    it->second = heap;
    dot();
  }
} 

static void lmheap_alloc(gpointer key, gpointer value, gpointer user_data) {
  Ngram ng = (Ngram) key;
  guint32 *iptr = (guint32 *) value;
  GHashTable *h = user_data;
  Heap heap = minialloc(sizeof(Hpair) * (1 + (*iptr)));
  heap_size(heap) = 0;
  g_hash_table_insert(h, ng, heap);
  dot();
}

static void CPPlmheap_insert(
    std::unordered_map<Ngram, gfloat*, CPPNgramHash, CPPNgramEqual>* hash,
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>& h
    ) {
  for(auto it = hash->begin(); it != hash->end(); ++ it) {
    Ngram ng = it->first; 
    gfloat *fptr = (gfloat*) it->second;
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

static void lmheap_insert(gpointer key, gpointer value, gpointer user_data) {
  Ngram ng = (Ngram) key;
  gfloat *fptr = (gfloat*) value;
  GHashTable *h = user_data;
  for (int i = ngram_size(ng); i > 0; i--) {
    Token ng_i = ng[i];
    ng[i] = NULLTOKEN;
    Heap heap = g_hash_table_lookup(h, ng);
    ng[i] = ng_i;
    g_assert(heap != NULL);
    heap_insert_min(heap, ng_i, *fptr);
  }
  dot();
}
static void CPPlmheap_sort(
    std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>& hash) {
  for(auto it = hash.begin(); it != hash.end(); ++ it) {
    Heap heap = it->second;
    g_assert(heap != NULL && heap_size(heap) > 0);
    heap_sort_max(heap);
    dot();
  }
}

static void lmheap_sort(gpointer key, gpointer value, gpointer user_data) {
  Heap heap = (Heap) value;
  g_assert(heap != NULL && heap_size(heap) > 0);
  heap_sort_max(heap);
  dot();
}


LMheap lmheap_init(LM lm) {
  g_message("lmheap_init start");
  LMheap h = g_hash_table_new(ngram_hash, ngram_equal);
  g_message("count logP");
  g_hash_table_foreach(lm->logP, lmheap_count, h);
  g_message("alloc logP_heap");
  g_hash_table_foreach(h, lmheap_alloc, h);
  g_message("insert logP");
  g_hash_table_foreach(lm->logP, lmheap_insert, h);
  g_message("sort logP_heap");
  g_hash_table_foreach(h, lmheap_sort, NULL);
  g_message("lmheap_init done");
  return h;
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

