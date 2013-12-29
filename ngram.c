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

uint64_t ngram_hash(const Ngram ng) { // fnv1a
  size_t hash = 14695981039346656037ULL;
  for (int i = ngram_size(ng); i >= 0; i--) {
    hash ^= ng[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

bool ngram_equal(const Ngram a, const Ngram b) {
  if (ngram_size(a) != ngram_size(b)) return 0;
  for (int i = ngram_size(a); i > 0; i--) {
    if (a[i] != b[i]) return 0;
  }
  return 1;
}
