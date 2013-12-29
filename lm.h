#ifndef __LM_H__
#define __LM_H__
#include "dlib.h"
#include "ngram.h"

typedef struct _LMS {
  darr_t logP;
  darr_t logB;
  uint32_t order;
  uint32_t nvocab;
} *LM; 

LM lm_init(char *lmfile);
float lm_logP(LM lm, Ngram ng);
float lm_logB(LM lm, Ngram ng);

#define SRILM_LOG0 -99		/* value returned for items not found */

typedef struct NF_s { Ngram key; float val; } *NF_t;

#endif
