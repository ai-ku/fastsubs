#include <zlib.h>
#include "foreach.h"
#include "minialloc.h"
#include "lm.h"

static LM lm1;

LM lm_init(char *lmfile) {
  LM lm;
  if (lm1 == NULL) {
    lm1 = minialloc(sizeof(struct _LMS));
    lm = lm1;
    lm->logP = g_hash_table_new(ngram_hash, ngram_equal);
    lm->logB = g_hash_table_new(ngram_hash, ngram_equal);
    lm->order = 0;
    lm->nvocab = 0;
  } else {
    g_error("Only one LM is allowed.");
  }

  gfloat *fptr;
  foreach_line(str, lmfile) {
    errno = 0;
    if (*str == '\n' || *str == '\\' || *str == 'n') continue;
    char *s = strtok(str, "\t");
    fptr = minialloc(sizeof(gfloat));
    *fptr = (gfloat) g_ascii_strtod(s, NULL);
    g_assert(errno == 0);
    s = strtok(NULL, "\t\n");
    Ngram ng = ngram_from_string(s);
    if (ngram_size(ng) > lm->order) lm->order = ngram_size(ng);
    for (int i = ngram_size(ng); i > 0; i--) {
      if (ng[i] > lm->nvocab) lm->nvocab = ng[i];
    }
    if (g_hash_table_lookup_extended(lm->logP, ng, NULL, NULL)) 
      g_error("Duplicate ngram");
    g_hash_table_insert(lm->logP, ng, fptr);
    s = strtok(NULL, "\n");
    if (s != NULL) {
      fptr = minialloc(sizeof(gfloat));
      *fptr = (gfloat) g_ascii_strtod(s, NULL);
      g_assert(errno == 0);
      g_hash_table_insert(lm->logB, ng, fptr);
    }
  }
  return lm;
}

gfloat lm_logP(LM lm, Ngram ng) {
  gfloat *p = g_hash_table_lookup(lm->logP, ng);
  return (p == NULL ? SRILM_LOG0 : *p);
}

gfloat lm_logB(LM lm, Ngram ng) {
  gfloat *p = g_hash_table_lookup(lm->logB, ng);
  return (p == NULL ? 0 : *p);
}
