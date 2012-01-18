#include <stdio.h>
#include "lm.h"
#include "ngram.h"
#include "procinfo.h"

int main(int argc, char **argv) {
  char buf[1024];
  g_message_init();
  g_message("Loading model file %s\n", argv[1]);
  LM lm = lm_init(argv[1]);
  g_message("ngram order = %d\n==> Enter ngram:\n", lm_ngram_order(lm));
  while(fgets(buf, 1024, stdin)) {
    Ngram ng = ngram_from_string(buf);
    g_message("logP=%.9g logB=%.9g\n", lm_logP(lm, ng), lm_logB(lm, ng));
    ngram_free(ng);
    g_message("==> Enter ngram:\n");
  }
}
