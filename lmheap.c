#include <stdio.h>
#include "minialloc.h"
#include "token.h"
#include "ngram.h"
#include "lm.h"
#include "lmheap.h"
#include "heap.h"
static gsize dot;

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
  if (++dot % 10000 == 0) fprintf(stderr, ".");
}

static void lmheap_alloc(gpointer key, gpointer value, gpointer user_data) {
  Ngram ng = (Ngram) key;
  guint32 *iptr = (guint32 *) value;
  GHashTable *h = user_data;
  Heap heap = minialloc(sizeof(Hpair) * (1 + (*iptr)));
  heap_size(heap) = 0;
  g_hash_table_insert(h, ng, heap);
  if (++dot % 10000 == 0) fprintf(stderr, ".");
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
  if (++dot % 10000 == 0) fprintf(stderr, ".");
}

static void lmheap_sort(gpointer key, gpointer value, gpointer user_data) {
  Heap heap = (Heap) value;
  g_assert(heap != NULL && heap_size(heap) > 0);
  heap_sort_max(heap);
  if (++dot % 10000 == 0) fprintf(stderr, ".");
}

LMheap lmheap_init(LM lm) {
  g_message("lmheap_init start");
  LMheap h = minialloc(sizeof(struct _LMheapS));
  h->lm = lm;
  h->logP_heap = g_hash_table_new(ngram_hash, ngram_equal);
  h->logB_heap = g_hash_table_new(ngram_hash, ngram_equal);
  g_message("count logP");
  g_hash_table_foreach(lm->logP, lmheap_count, h->logP_heap);
  g_message("count logB");
  g_hash_table_foreach(lm->logB, lmheap_count, h->logB_heap);
  g_message("alloc logP_heap");
  g_hash_table_foreach(h->logP_heap, lmheap_alloc, h->logP_heap);
  g_message("alloc logB_heap");
  g_hash_table_foreach(h->logB_heap, lmheap_alloc, h->logB_heap);
  g_message("insert logP");
  g_hash_table_foreach(lm->logP, lmheap_insert, h->logP_heap);
  g_message("insert logB");
  g_hash_table_foreach(lm->logB, lmheap_insert, h->logB_heap);
  g_message("sort logP_heap");
  g_hash_table_foreach(h->logP_heap, lmheap_sort, NULL);
  g_message("sort logB_heap");
  g_hash_table_foreach(h->logB_heap, lmheap_sort, NULL);
  g_message("lmheap_init done");
  return h;
}

Heap lmheap_logP(LMheap h, Ngram ng) {
  return g_hash_table_lookup(h->logP_heap, ng);
}

Heap lmheap_logB(LMheap h, Ngram ng) {
  return g_hash_table_lookup(h->logB_heap, ng);
}
