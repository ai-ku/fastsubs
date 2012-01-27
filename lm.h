#ifndef __LM_H__
#define __LM_H__
#include <glib.h>
#include "ngram.h"

typedef struct _LMS {
  GHashTable *logP;
  GHashTable *logB;
  guint order;
  guint nvocab;
} *LM; 

LM lm_init(char *lmfile);
gfloat lm_logP(LM lm, Ngram ng);
gfloat lm_logB(LM lm, Ngram ng);

#define SRILM_LOG0 -99		/* value returned for items not found */

#endif
