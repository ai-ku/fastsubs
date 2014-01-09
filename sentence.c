#include <assert.h>
#include "dlib.h"
#include "sentence.h"

Token SOS, EOS, UNK;

static void init_special_tokens() {
  SOS = token_from_string(SOSTAG);
  EOS = token_from_string(EOSTAG);
  UNK = token_from_string(UNKTAG);
}

uint32_t sentence_from_string(Sentence st, char *str, int nmax, char **w) {
  if (SOS == 0) init_special_tokens();
  uint32_t ntok = 0;
  assert(ntok < nmax);
  st[++ntok] = SOS;
  if (w != NULL) w[ntok] = SOSTAG;
  fortok (word, str) {
    Token wtok = token_try_string(word);
    if ((wtok == SOS) || (wtok == EOS)) continue;
    assert(ntok < nmax);
    st[++ntok] = (wtok == 0 ? UNK : wtok);
    if (w != NULL) w[ntok] = word;
  }
  assert(ntok < nmax);
  st[++ntok] = EOS;
  if (w != NULL) w[ntok] = EOSTAG;
  st[0] = ntok;
  return ntok;
}

#include <stdio.h>

void sentence_print(Sentence s) {
  for (int i = 1; i <= sentence_size(s); i++) {
    printf("%s ", token_to_string(s[i]));
  }
  printf("\n");
}
