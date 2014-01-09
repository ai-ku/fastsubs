#include <stdio.h>
#include "fastsubs.h"
#include "heap.h"
#include "dlib.h"
#define SMAX 1000

int main(int argc, char **argv) {
  Token s[1+SMAX];
  msg("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  Hpair *subs = dalloc(lm_nvocab(lm) * sizeof(Hpair));
  msg("ngram order = %d\n==> Enter sentence:\n", lm_order(lm));
  forline (buf, NULL) {
    int n = sentence_from_string(s, buf, SMAX, NULL);
    for (int i = 2; i <= n; i++) {
      int nsubs = fastsubs(subs, s, i, lm, 1, 10);
      printf("%s:\n", sym2str(s[i]));
      for (int j = 0; j < nsubs; j++) {
	printf("%s\t%.4f\n", sym2str(subs[j].token), subs[j].logp);
      }
    }
    msg("==> Enter sentence:\n");
  }
  dfreeall();
}
