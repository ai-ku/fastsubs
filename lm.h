#ifndef __LM_H__
#define __LM_H__
#include <glib.h>
#include "ngram.h"
#include <unordered_map>
#include <string>
#include <memory>

struct CPPNgramHash {
    size_t operator() (const Ngram& x) const { 
      return static_cast<int>(ngram_hash(x)); 
    }
};

struct CPPNgramEqual { 
  bool operator() (const Ngram& n1, const Ngram& n2) const {
   return static_cast<bool>(ngram_equal(n1, n2));
  }
};

typedef struct _LMS {
  GHashTable *logP;
  GHashTable *logB;
  guint order;
  guint nvocab;
  std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>* CPPlogP; 
  std::unordered_map<Ngram, gpointer, CPPNgramHash, CPPNgramEqual>* CPPlogB; 
} *LM; 

LM lm_init(char *lmfile);
gfloat lm_logP(LM lm, Ngram ng);
gfloat lm_logB(LM lm, Ngram ng);
gfloat lm_CPPlogP(LM lm, Ngram ng);
gfloat lm_CPPlogB(LM lm, Ngram ng);

#define SRILM_LOG0 -99		/* value returned for items not found */

#endif
