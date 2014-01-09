#include <stdio.h>
#include "dlib.h"
#include "heap.h"
#include "ngram.h"
#include "lm.h"

int main(int argc, char **argv) {
  msg("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  msg("==> Enter ngram:");
  forline (buf, NULL) {
    Ngram ng = ngram_from_string(buf);
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      Heap p = lm_pheap(lm, ng, 1, ngram_size(ng));
      if (p != NULL) {
	msg("logP[%d]:", i);
	for (int j = 1; j <= heap_size(p); j++) {
	  msg("%g\t%s", p[j].logp, token_to_string(p[j].token));
	}
      }
      p = lm_bheap(lm, ng, 1, ngram_size(ng));
      if (p != NULL) {
	msg("logB[%d]:", i);
	for (int j = 1; j <= heap_size(p); j++) {
	  msg("%g\t%s", p[j].logp, token_to_string(p[j].token));
	}
      }
      ng[i] = ng_i;
    }
    msg("==> Enter ngram:");
  }
  msg("Destroying LM");
  lm_free(lm);
  msg("Freeing tmp space");
  dfreeall();
  msg("done");
}
