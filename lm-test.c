#include <stdio.h>
#include "lm.h"
#include "ngram.h"
#include "procinfo.h"

int main(int argc, char **argv) {
  char buf[1024];
  g_message_init();
  // seed is defaulted to 0
  extern GRand *g_lib_rgen;
  g_lib_rgen = g_rand_new_with_seed(0);
  g_message("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  g_message("ngram order = %d", lm->order);
  g_message("vocab:%d", lm->nvocab);
  g_message("CPPlogP=%d", lm->CPPlogP->size());
  g_message("CPPlogB=%d", lm->CPPlogB->size());
  g_message("==> Enter ngram:");
  while(fgets(buf, 1024, stdin)) {
    Ngram ng = ngram_from_string(buf);
    g_message("logP=%.9g logB=%.9g", lm_CPPlogP(lm, ng), lm_CPPlogB(lm, ng));
    g_message("==> Enter ngram:");
  }
}
