#include <stdio.h>
#include <glib.h>
#include "dlib.h"
#include "lm.h"
#include "ngram.h"
#include "procinfo.h"

int main(int argc, char **argv) {
  char buf[1024];
  g_message_init();
  g_message("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  g_message("ngram order = %d", lm->order);
  g_message("logP=%llu/%llu", len(lm->logP), cap(lm->logP));
  g_message("logB=%llu/%llu", len(lm->logB), cap(lm->logB));
  g_message("==> Enter ngram:");
  while(fgets(buf, 1024, stdin)) {
    Ngram ng = ngram_from_string(buf);
    g_message("logP=%.9g logB=%.9g", lm_logP(lm, ng), lm_logB(lm, ng));
    g_message("==> Enter ngram:");
  }
}
