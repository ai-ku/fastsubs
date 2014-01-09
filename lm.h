#ifndef __LM_H__
#define __LM_H__
#include "dlib.h"
#include "sentence.h"
#include "heap.h"

#define SRILM_LOG0 -99		/* value returned for items not found */

typedef struct _LMS *LM;

LM lm_init(const char *lmfile);
void lm_free(LM lm);
float lm_logp(LM lm, Sentence s, uint32_t i);
float lm_phash(LM lm, Sentence s, uint32_t i, uint32_t n);
float lm_bhash(LM lm, Sentence s, uint32_t i, uint32_t n);
Heap lm_pheap(LM lm, Sentence s, uint32_t i, uint32_t n);
Heap lm_bheap(LM lm, Sentence s, uint32_t i, uint32_t n);
uint32_t lm_order(LM lm);
uint32_t lm_nvocab(LM lm);

#endif
/** sentence_logp(): Calculates the log probability of the j'th token
 * in sentence s according to model lm 
 */
float sentence_logp(Sentence s, int j, LM lm);

