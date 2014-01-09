#include <stdio.h>
#include "token.h"
#include "lm.h"
#include "sentence.h"
#define SMAX 1000

int main(int argc, char **argv) {
  Token s[1+SMAX];
  msg("Loading model file %s", argv[1]);
  LM lm = lm_init(argv[1]);
  msg("ngram order = %d\n==> Enter sentence:", lm_order(lm));
  forline (buf, NULL) {
    int n = sentence_from_string(s, buf, SMAX, NULL);
    double total = 0;
    for (int i = 2; i <= n; i++) {
      double logp_i = lm_logp(lm, s, i);
      total += logp_i;
      fprintf(stderr, "%s\t%.9g\n", token_to_string(s[i]), logp_i);
    }
    msg("sentence_logp=%.9g\n==> Enter sentence:\n", total);
  }
}
