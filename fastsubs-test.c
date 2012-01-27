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
    int n = sentence_from_string(s, buf, 1024);
    for (int i = 1; i <= n; i++) {
      int nsubs = fastsubs(subs, s, i, lm, 0, 10);
      printf("%d. %s:", i, token_to_string(s[i]));
      for (int j = 0; j < nsubs; j++) {
	printf(" %s%g", token_to_string(subs[j].token), subs[j].logp);
      }
      printf("\n");
    }
    g_message("==> Enter sentence:\n");
  }
  minialloc_free_all();
}
