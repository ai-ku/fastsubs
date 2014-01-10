#include <stdio.h>
#include "dlib.h"
#include "lm.h"
#include "ngram.h"

int main(int argc, char **argv) {
  msg("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  msg("==> Enter ngram:");
  forline (buf, NULL) {
    Ngram ng = ngram_from_string(buf);
    msg("logP=%.9g logB=%.9g", lm_phash(lm, ng, 1, ngram_size(ng)), lm_bhash(lm, ng, 1, ngram_size(ng)));
    msg("==> Enter ngram:");
  }
  msg("Free lm");
  lm_free(lm);
  msg("Free tmp space");
  dfreeall();
  msg("Free symtable");
  symtable_free();
  msg("done");
}
