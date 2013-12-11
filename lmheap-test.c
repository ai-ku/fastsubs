#include <stdio.h>
#include "lm.h"
#include "lmheap.h"
#include "procinfo.h"
#include "minialloc.h"
#include "heap.h"

int main(int argc, char **argv) {
  char buf[1024];
  g_message_init();
  // seed is defaulted to 0
  extern GRand *g_lib_rgen;
  g_lib_rgen = g_rand_new_with_seed(0);
  g_message("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  g_message("order=%d", lm->order);
  g_message("logP=%d", lm->CPPlogP->size());
  g_message("logB=%d", lm->CPPlogB->size());
  g_message("Creating LMheap");
//  LMheap h = lmheap_init(lm);
//  g_message("logP_heap=%d", g_hash_table_size(h));
  auto CPPh = CPPlmheap_init(lm);
  g_message("logP_heap=%d", CPPh.size());
  g_message("==> Enter ngram:");
  while(fgets(buf, 1024, stdin)) {
    Ngram ng = ngram_from_string(buf);
    for (int i = ngram_size(ng); i > 0; i--) {
      Token ng_i = ng[i];
      ng[i] = NULLTOKEN;
      g_message("logP[%d]:", i);
//      Heap p = g_hash_table_lookup(h, ng);
//      if (p != NULL) {
//        for (int j = 1; j <= heap_size(p); j++) {
//          g_message("%g\t%s", p[j].logp, token_to_string(p[j].token));
//        }
//      }
      auto it = CPPh.find(ng); 
      if (it != CPPh.end()) {
        Heap CPPp = it->second; 
        for (int j = 1; j <= heap_size(CPPp); j++) {
          g_message("%g\t%s", CPPp[j].logp, token_to_string(CPPp[j].token));
        }
      }
      ng[i] = ng_i;
    }
    g_message("==> Enter ngram:");
  }

  g_message("Destroying LMheap");
  minialloc_free_all();
  g_message("done");
}
