#include <stdio.h>
#include "lm.h"
#include "lmheap.h"
#include "dlib.h"
#include "heap.h"

int main(int argc, char **argv) {
  char buf[1024];
  msg("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  msg("order=%d", lm->order);
  msg("logP=%zu/%zu", len(lm->logP), cap(lm->logP));
  msg("logB=%zu/%zu", len(lm->logB), cap(lm->logB));
  msg("Creating LMheap");
  LMheap h = lmheap_init(lm);
  msg("logP_heap=%zu/%zu", len(h), cap(h));

  msg("==> Enter ngram:");
  while(fgets(buf, 1024, stdin)) {
    Ngram ng = ngram_from_string(buf);
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      msg("logP[%d]:", i);
      NH_t nh = lmheap_get(h, ng);
      if (nh != NULL) {
	Heap p = nh->val;
	for (int j = 1; j <= heap_size(p); j++) {
	  msg("%g\t%s", p[j].logp, token_to_string(p[j].token));
	}
      }
      ng[i] = ng_i;
    }
    msg("==> Enter ngram:");
  }

  msg("Destroying LMheap");
  dfreeall();
  msg("done");
}
