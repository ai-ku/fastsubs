#ifndef __LM_H__
#define __LM_H__
#include "dlib.h"
#include "ngram.h"
#include "heap.h"

#define SRILM_LOG0 -99		/* value returned for items not found */

typedef struct _LMS *LM;

LM lm_init(const char *lmfile);
void lm_free(LM lm);
float lm_logP(LM lm, Ngram ng);
float lm_logB(LM lm, Ngram ng);
Heap lm_logP_heap(LM lm, Ngram ng);
Heap lm_logB_heap(LM lm, Ngram ng);
uint32_t lm_order(LM lm);
uint32_t lm_nvocab(LM lm);

#endif
