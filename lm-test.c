#include <stdio.h>
#include "dlib.h"
#include "lm.h"
#include "ngram.h"

int main(int argc, char **argv) {
  char buf[1024];
  msg("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  msg("ngram order = %d", lm->order);
  msg("logP=%llu/%llu", len(lm->logP), cap(lm->logP));
  msg("logB=%llu/%llu", len(lm->logB), cap(lm->logB));
  msg("==> Enter ngram:");
  while(fgets(buf, 1024, stdin)) {
    Ngram ng = ngram_from_string(buf);
    msg("logP=%.9g logB=%.9g", lm_logP(lm, ng), lm_logB(lm, ng));
    msg("==> Enter ngram:");
  }
}
