#include <stdio.h>
#include "lm.h"
#include "ngram.h"
#include "procinfo.h"

int main(int argc, char **argv) {
  char buf[1024];
  g_message_init();
  g_message("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  g_message("ngram order = %d", lm->order);
  g_message("logP=%d", g_hash_table_size(lm->logP));
  g_message("logB=%d", g_hash_table_size(lm->logB));
  g_message("==> Enter ngram:");
  while(fgets(buf, 1024, stdin)) {
    Ngram ng = ngram_from_string(buf);
    g_message("logP=%.9g logB=%.9g", lm_logP(lm, ng), lm_logB(lm, ng));
    g_message("==> Enter ngram:");
  }
}
