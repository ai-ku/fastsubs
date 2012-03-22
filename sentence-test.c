#include <stdio.h>
#include "token.h"
#include "lm.h"
#include "procinfo.h"
#include "sentence.h"

int main(int argc, char **argv) {
  char buf[1024];
  Token s[1025];
  g_message_init();
  g_message("Loading model file %s\n", argv[1]);
  LM lm = lm_init(argv[1]);
  g_message("ngram order = %d\n==> Enter sentence:\n", lm->order);
  while(fgets(buf, 1024, stdin)) {
    int n = sentence_from_string(s, buf, 1024, NULL);
    for (int i = 2; i <= n; i++) {
      fprintf(stderr, "%s\t%.9g\n", token_to_string(s[i]), sentence_logp(s, i, lm));
    }
    g_message("==> Enter sentence:\n");
  }
}
