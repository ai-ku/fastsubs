#include <stdio.h>
#include "token.h"
#include "lm.h"
#include "procinfo.h"
#include "sentence.h"

int main(int argc, char **argv) {
  char buf[1024];
  Token s[1025];
  g_message_init();
  g_message("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  g_message("ngram order = %d\n==> Enter sentence:", lm->order);
  while(fgets(buf, 1024, stdin)) {
    int n = sentence_from_string(s, buf, 1024, NULL);
    double total = 0;
    for (int i = 2; i <= n; i++) {
      double logp_i = sentence_logp(s, i, lm);
      total += logp_i;
      fprintf(stderr, "%s\t%.9g\n", token_to_string(s[i]), logp_i);
    }
    g_message("sentence_logp=%.9g\n==> Enter sentence:\n", total);
  }
}
