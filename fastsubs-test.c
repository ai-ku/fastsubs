#include <stdio.h>
#include "procinfo.h"
#include "fastsubs.h"
#include "heap.h"
#include "minialloc.h"

int main(int argc, char **argv) {
  char buf[1024];
  Token s[1025];
  g_message_init();
  g_message("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  Hpair *subs = minialloc(lm->nvocab * sizeof(Hpair));
  g_message("ngram order = %d\n==> Enter sentence:\n", lm->order);
  while(fgets(buf, 1024, stdin)) {
    int n = sentence_from_string(s, buf, 1024, NULL);
    for (int i = 2; i <= n; i++) {
      int nsubs = fastsubs(subs, s, i, lm, 0, 100);
      printf("%s:\n", token_to_string(s[i]));
      for (int j = 0; j < nsubs; j++) {
	printf("%s\t%.4f\n", token_to_string(subs[j].token), subs[j].logp);
      }
    }
    g_message("==> Enter sentence:\n");
  }
  minialloc_free_all();
}
