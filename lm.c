#include <zlib.h>
#include "foreach.h"
#include "minialloc.h"
#include "lm.h"
static LM lm1;

LM lm_init(char *lmfile) {
  LM lm;
  if (lm1 == NULL) {
    lm1 = static_cast<LM>(minialloc(sizeof(struct _LMS)));
    lm = lm1;
    lm->CPPlogP = new std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>();
    lm->CPPlogB = new std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>();
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
    fptr = static_cast<gfloat*>(minialloc(sizeof(gfloat)));
    *fptr = static_cast<gfloat>(g_ascii_strtod(s, NULL));
    g_assert(errno == 0);
    s = strtok(NULL, "\t\n");
    Ngram ng = ngram_from_string(s);
    if (ngram_size(ng) > lm->order) lm->order = ngram_size(ng);
    for (int i = ngram_size(ng); i > 0; i--) {
      if (ng[i] > lm->nvocab) lm->nvocab = ng[i];
    }
    if (lm->CPPlogP->find(ng) != lm->CPPlogP->end())
      g_error("Duplicate ngram");
    lm->CPPlogP->emplace(ng, fptr);
    s = strtok(NULL, "\n");
    if (s != NULL) {
      fptr = static_cast<gfloat*>(minialloc(sizeof(gfloat)));
      *fptr = static_cast<gfloat>(g_ascii_strtod(s, NULL));
      g_assert(errno == 0);
      lm->CPPlogB->emplace(ng, fptr);
    }
  }
  return lm;
}

gfloat lm_CPPlogP(LM lm, Ngram ng) {
  auto p = lm->CPPlogP->find(ng);
  return (p == lm->CPPlogP->end() ? SRILM_LOG0 : *(static_cast<gfloat*>(p->second)));
}

gfloat lm_CPPlogB(LM lm, Ngram ng) {
  auto p = lm->CPPlogB->find(ng);
  return (p == lm->CPPlogB->end() ? 0 : *(static_cast<gfloat*>(p->second)));
}
