#include <stdio.h>
#include <assert.h>
#include <glib.h>
#include "dlib.h"
#include "token.h"
#include "ngram.h"
#include "lm.h"
#include "heap.h"
#include "lmheap.h"

#define NDOT 100000
#define dot() if (++ndot % NDOT == 0) fprintf(stderr, ".")
static size_t ndot;

#define _lh_init(ng) ((struct NH_s) { ngram_dup(ng), NULL })
D_HASH(_lh, struct NH_s, Ngram, ngram_equal, ngram_hash, d_keyof, _lh_init, d_keyisnull, d_keymknull)

NH_t lmheap_get(LMheap h, Ngram key) {
  return _lhget(h, key, false);
}

LMheap lmheap_init(LM lm) {
  g_message("lmheap_init start");
  LMheap h = _lhnew(0);		// know exactly how much to allocate, should use it

  g_message("count logP");
  forhash (NF_t, nf, lm->logP, d_keyisnull) {
    Ngram ng = nf->key;
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      NH_t lh = _lhget(h, ng, true);
      lh->val = (Heap)(1 + ((size_t)(lh->val)));
      ng[i] = ng_i;
    }
    dot();
  }

  g_message("alloc logP_heap");
  forhash (NH_t, nh, h, d_keyisnull) {
    uint64_t n = ((uint64_t) nh->val);
    Heap heap = dalloc(sizeof(Hpair) * (1 + n));
    heap_size(heap) = 0;
    nh->val = heap;
    dot();
  }

  g_message("insert logP");
  forhash (NF_t, nf, lm->logP, d_keyisnull) {
    Ngram ng = nf->key;
    float f = nf->val;
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      NH_t lh = _lhget(h, ng, false);
      ng[i] = ng_i;
      assert(lh != NULL);
      assert(lh->val != NULL);
      heap_insert_min(lh->val, ng_i, f);
      assert(heap_size(lh->val) > 0);
    }
    dot();
  }
  g_message("sort logP_heap");
  forhash (NH_t, nh, h, d_keyisnull) {
    Heap heap = nh->val;
    assert(heap != NULL && heap_size(heap) > 0);
    heap_sort_max(heap);
    dot();
  }
  g_message("lmheap_init done");
  return h;
}

