#include <stdio.h>
#include "minialloc.h"
#include "token.h"
#include "ngram.h"
#include "lm.h"
#include "lmheap.h"
static gsize dot;

static void lmheap_count(gpointer key, gpointer value, gpointer user_data) {
  Ngram ng = (Ngram) key;
  GHashTable *h = user_data;
  for (int i = ngram_size(ng); i > 0; i--) {
    Token ng_i = ng[i];
    ng[i] = NULLTOKEN;
    guint32* iptr = g_hash_table_lookup(h, ng);
    if (iptr == NULL) {
      Ngram ngcopy = ngram_copy(ng);
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
  LMpair *heap = minialloc(sizeof(LMpair) * (1 + (*iptr)));
  heap_size(heap) = 0;
  g_hash_table_insert(h, ng, heap);
  if (++dot % 10000 == 0) fprintf(stderr, ".");
}

/* heap is an array of token-logp pairs.  The first element is used to
   store the count n, so the actual elements go from 1..n.  With a one
   based binary heap parent is at i/2 and children are at 2i, 2i+1.
   heap[1] has the smallest logp value. */

static void heap_insert(LMpair* heap, Token key, gfloat value) {
  guint n = ++heap_size(heap);
  while (n > 1) {
    guint p = n/2;
    if (heap[p].logp <= value) break;
    heap[n] = heap[p];
    n = p;
  }
  heap[n].token = key;
  heap[n].logp = value;
}

/* this sorts the heapified array so heap[1] as the largest logp value. */

static void heap_sort(LMpair *heap) {
  guint p, n, end; 		/* parent, child and end-of-heap index */
  end = heap_size(heap);
  while (end > 1) {
    LMpair copy = heap[end];
    heap[end] = heap[1];
    end--;
    p = 1;
    while ((n=2*p) <= end) {
      if ((n < end) && (heap[n+1].logp < heap[n].logp)) n++;
      if (copy.logp < heap[n].logp) break;
      heap[p] = heap[n];
      p = n;
    }
    heap[p] = copy;
  }
}

static void lmheap_insert(gpointer key, gpointer value, gpointer user_data) {
  Ngram ng = (Ngram) key;
  gfloat *fptr = (gfloat*) value;
  GHashTable *h = user_data;
  for (int i = ngram_size(ng); i > 0; i--) {
    Token ng_i = ng[i];
    ng[i] = NULLTOKEN;
    LMpair* heap = g_hash_table_lookup(h, ng);
    ng[i] = ng_i;
    g_assert(heap != NULL);
    heap_insert(heap, ng_i, *fptr);
  }
  if (++dot % 10000 == 0) fprintf(stderr, ".");
}

static void lmheap_sort(gpointer key, gpointer value, gpointer user_data) {
  LMpair *heap = (LMpair *) value;
  g_assert(heap != NULL && heap_size(heap) > 0);
  heap_sort(heap);
  if (++dot % 10000 == 0) fprintf(stderr, ".");
}

LMheap lmheap_init(LM lm) {
  g_message("lmheap_init start");
  LMheap h = g_new0(struct _LMheapS, 1);
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

void lmheap_free(LMheap h) {
  if (h != NULL) {
    if (h->logP_heap != NULL) g_hash_table_destroy(h->logP_heap);
    if (h->logB_heap != NULL) g_hash_table_destroy(h->logB_heap);
    g_free(h);
  }
}
