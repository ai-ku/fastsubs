#include <stdlib.h>
#include <assert.h>
#include "dlib.h"
#include "ngram.h"
#include "sentence.h"
#include "heap.h"
#include "lm.h"

struct _LMS {
  uint32_t order;
  uint32_t nvocab;
  darr_t phash;
  darr_t bhash;
  darr_t pheap;
  darr_t bheap;
};

/* Define ngram-logp/logb hash */
typedef struct NFS { Ngram key; float val; } *NF; // elements are ngram-float pairs
#define _nf_init(ng) ((struct NFS) { (ng), 0 }) // ng must be already allocated
D_HASH(_nf_, struct NFS, Ngram, ngram_equal, ngram_hash, d_keyof, _nf_init, d_keyisnull, d_keymknull)

/* Define ngram-tokenHeap hash */
typedef struct NHS { Ngram key; Heap val; } *NH; // elements are ngram-heap pairs
#define _nh_init(ng) ((struct NHS) { ngram_dup(ng), NULL }) // no need for this dup, fix using ptr-hole
D_HASH(_nh_, struct NHS, Ngram, ngram_equal, ngram_hash, d_keyof, _nh_init, d_keyisnull, d_keymknull)

static void lm_read(LM lm, const char *lmfile);
static void lm_pheap_init(LM lm);
static void lm_bheap_init(LM lm);

LM lm_init(const char *lmfile) {
  LM lm = _d_malloc(sizeof(struct _LMS));
  lm->phash = _nf_new(0);
  lm->bhash = _nf_new(0);
  lm->pheap = _nh_new(0);
  lm->bheap = _nh_new(0);
  lm->order = 0;
  lm->nvocab = 0;
  lm_read(lm, lmfile);
  lm_pheap_init(lm);
  lm_bheap_init(lm);
  dbg("lm_init done: NF=%zu logP=%zu/%zu logB=%zu/%zu NH=%zu pheap=%zu/%zu bheap=%zu/%zu", 
      sizeof(struct NFS), len(lm->phash), cap(lm->phash), len(lm->bhash), cap(lm->bhash),
      sizeof(struct NHS), len(lm->pheap), cap(lm->pheap), len(lm->bheap), cap(lm->bheap)
      );
  return lm;
}

void lm_free(LM lm) {
  _nf_free(lm->phash);
  _nf_free(lm->bhash);
  _nh_free(lm->pheap);
  _nh_free(lm->bheap);
  _d_free(lm);
}

float lm_logp(LM lm, Sentence s, uint32_t i) {
  assert((i >= 1) && (i <= sentence_size(s))); 
  if (i == 1) return (s[i] == SOS ? 0 : SRILM_LOG0); /* s[1] always SOS */
  if (s[i] == SOS) return (i == 1 ? 0 : SRILM_LOG0); /* SOS is only in s[1] */
  float logp = 0; // TODO: this should do internal sum in double
  uint32_t len = lm_order(lm);
  if (i < len) len = i;
  while (len >= 1) {
    uint32_t start = i - len + 1;
    float lp = lm_phash(lm, s, start, len);
    if (lp != SRILM_LOG0) {
      logp += lp;
      break;
    } else {
      assert(len > 1);
      logp += lm_bhash(lm, s, start, len-1);
      len--;
    }
  }
  assert(logp < 0);
  assert(logp > SRILM_LOG0);
  return logp;
}

float lm_phash(LM lm, Sentence s, uint32_t i, uint32_t n) {
  uint32_t save = s[i-1];
  s[i-1] = n;
  NF p = _nf_get(lm->phash, &s[i-1], false);
  s[i-1] = save;
  return (p == NULL ? SRILM_LOG0 : p->val);
}

float lm_bhash(LM lm, Sentence s, uint32_t i, uint32_t n) {
  uint32_t save = s[i-1];
  s[i-1] = n;
  NF p = _nf_get(lm->bhash, &s[i-1], false);
  s[i-1] = save;
  return (p == NULL ? 0 : p->val);
}

Heap lm_pheap(LM lm, Sentence s, uint32_t i, uint32_t n) {
  uint32_t save = s[i-1];
  s[i-1] = n;
  NH p = _nh_get(lm->pheap, &s[i-1], false);
  s[i-1] = save;
  return (p == NULL ? NULL : p->val);
}

Heap lm_bheap(LM lm, Sentence s, uint32_t i, uint32_t n) {
  uint32_t save = s[i-1];
  s[i-1] = n;
  NH p = _nh_get(lm->bheap, &s[i-1], false);
  s[i-1] = save;
  return (p == NULL ? NULL : p->val);
}

uint32_t lm_order(LM lm) {
  return lm->order;
}

uint32_t lm_nvocab(LM lm) {
  return lm->nvocab;
}

static void lm_read(LM lm, const char *lmfile) {
  forline(str, lmfile) {
    errno = 0;
    if (*str == '\n' || *str == '\\' || *str == 'n') continue;
    char *tok[] = { NULL, NULL, NULL };
    size_t ntok = split(str, '\t', tok, 3);
    assert(ntok >= 2);
    float f = strtof(tok[0], NULL);
    assert(errno == 0);
    if (ntok == 2) {
      size_t len = strlen(tok[1]);
      if (tok[1][len-1] == '\n') tok[1][len-1] = '\0';
    }
    Ngram ng = ngram_from_string(tok[1]);
    if (ngram_size(ng) > lm->order) 
      lm->order = ngram_size(ng);
    for (int i = ngram_size(ng); i > 0; i--) {
      if (ng[i] > lm->nvocab) lm->nvocab = ng[i];
    }
    _nf_get(lm->phash, ng, true)->val = f;
    if (ntok == 3) {
      f = (float) strtof(tok[2], NULL);
      assert(errno == 0);
      _nf_get(lm->bhash, ng, true)->val = f;
    }
  }
}

static void lm_pheap_init(LM lm) {
  //dbg("count logP");
  forhash (NF, nf, lm->phash, d_keyisnull) {
    Ngram ng = nf->key;
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      NH nh = _nh_get(lm->pheap, ng, true);
      nh->val = (Heap)(1 + ((size_t)(nh->val)));
      ng[i] = ng_i;
    }
  }

  //dbg("alloc logP_heap");
  forhash (NH, nh, lm->pheap, d_keyisnull) {
    size_t n = ((size_t) nh->val);
    Heap heap = dalloc(sizeof(Hpair) * (1 + n));
    heap_size(heap) = 0;
    nh->val = heap;
  }

  //dbg("insert logP");
  forhash (NF, nf, lm->phash, d_keyisnull) {
    Ngram ng = nf->key;
    float f = nf->val;
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      NH nh = _nh_get(lm->pheap, ng, false);
      ng[i] = ng_i;
      heap_insert_min(nh->val, ng_i, f);
    }
  }

  //dbg("sort logP_heap");
  forhash (NH, nh, lm->pheap, d_keyisnull) {
    Heap heap = nh->val;
    assert(heap != NULL && heap_size(heap) > 0);
    heap_sort_max(heap);
  }
}

static void lm_bheap_init(LM lm) {
  //dbg("count logB");
  forhash (NF, nf, lm->bhash, d_keyisnull) {
    Ngram ng = nf->key;
    float f = nf->val;
    if (f <= 0) continue;	// we only need positive cases, unseen words have 0 logB
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      NH nh = _nh_get(lm->bheap, ng, true);
      nh->val = (Heap)(1 + ((size_t)(nh->val)));
      ng[i] = ng_i;
    }
  }

  //dbg("alloc logB_heap");
  forhash (NH, nh, lm->bheap, d_keyisnull) {
    size_t n = ((size_t) nh->val);
    Heap heap = dalloc(sizeof(Hpair) * (1 + n));
    heap_size(heap) = 0;
    nh->val = heap;
  }

  //dbg("insert logB");
  forhash (NF, nf, lm->bhash, d_keyisnull) {
    Ngram ng = nf->key;
    float f = nf->val;
    if (f <= 0) continue;	// we only need positive cases, unseen words have 0 logB
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      NH nh = _nh_get(lm->bheap, ng, false);
      ng[i] = ng_i;
      heap_insert_min(nh->val, ng_i, f);
    }
  }

  //dbg("sort logB_heap");
  forhash (NH, nh, lm->bheap, d_keyisnull) {
    Heap heap = nh->val;
    assert(heap != NULL && heap_size(heap) > 0);
    heap_sort_max(heap);
  }
}
