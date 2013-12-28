#include <stdlib.h>
#include <assert.h>
#include "dlib.h"
#include "token.h"
#include "ngram.h"
#define MAX_NGRAM_ORDER 1023

Ngram ngram_from_string(char *str) {
  Token ng[1 + MAX_NGRAM_ORDER];
  int ntok = 0;
  fortok(word, str) {
    assert(ntok < MAX_NGRAM_ORDER);
    ng[++ntok] = token_from_string(word);
  }
  ng[0] = ntok;
  return ngram_dup(ng);
}

Ngram ngram_dup(Ngram ng) {
  size_t size = (sizeof(Token) * (ngram_size(ng)+1));
  Ngram ngcopy = dalloc(size);
  memcpy(ngcopy, ng, size);
  return ngcopy;
}

Ngram ngram_cpy(Ngram ngcopy, Ngram ng) {
  size_t size = (sizeof(Token) * (ngram_size(ng)+1));
  memcpy(ngcopy, ng, size);
  return ngcopy;
}

/** Ngram hash functions */

static uint32_t ngram_hash_rnd[1 + MAX_NGRAM_ORDER];

static void init_ngram_hash_rnd() {
  uint32_t r;
  if (*ngram_hash_rnd == 0) {
    srandom(1);
    for (int i = MAX_NGRAM_ORDER; i >= 0; i--) {
      do {
	r = random();
      } while (r == 0);
      ngram_hash_rnd[i] = r;
    }
  }
}

unsigned int ngram_hash(const Ngram ng) {
  if (*ngram_hash_rnd == 0) init_ngram_hash_rnd();
  unsigned int hash = 0;
  for (int i = ngram_size(ng); i > 0; i--) {
    hash += ng[i] * ngram_hash_rnd[i];
  }
  return hash;
}

int ngram_equal(const Ngram a, const Ngram b) {
  if (ngram_size(a) != ngram_size(b)) return 0;
  for (int i = ngram_size(a); i > 0; i--) {
    if (a[i] != b[i]) return 0;
  }
  return 1;
}
