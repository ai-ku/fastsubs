#include <stdio.h>
#include "foreach.h"
#include "sentence.h"

#define SOSTAG "<s>"
#define EOSTAG "</s>"
#define UNKTAG "<unk>"

static Token SOS, EOS, UNK;

static void init_special_tokens() {
  SOS = token_from_string(SOSTAG);
  EOS = token_from_string(EOSTAG);
  UNK = token_from_string(UNKTAG);
}

guint32 sentence_from_string(Sentence st, char *str, int nmax, char **w) {
  if (SOS == 0) init_special_tokens();
  guint32 ntok = 0;
  g_assert(ntok < nmax);
  st[++ntok] = SOS;
  if (w != NULL) w[ntok] = SOSTAG;
  foreach_token(word, str) {
    Token wtok = token_try_string(word);
    if ((wtok == SOS) || (wtok == EOS)) continue;
    g_assert(ntok < nmax);
    st[++ntok] = (wtok == 0 ? UNK : wtok);
    if (w != NULL) w[ntok] = word;
  }
  g_assert(ntok < nmax);
  st[++ntok] = EOS;
  if (w != NULL) w[ntok] = EOSTAG;
  st[0] = ntok;
  return ntok;
}

gfloat sentence_logp(Sentence s, int j, LM lm) {
  g_assert((j >= 1) && (j <= sentence_size(s))); 
  if (j == 1) return (s[j] == SOS ? 0 : SRILM_LOG0); /* s[1] always SOS */
  if (s[j] == SOS) return (j == 1 ? 0 : SRILM_LOG0); /* SOS is only in s[1] */
  int i = j - lm->order;
  if (i < 0) i = 0;
  gfloat ll = 0;
  while (i < j) {
    Token si = s[i];
    s[i] = (guint32) (j - i);	/* ngram order */
    gfloat lp = lm_logP(lm, &s[i]);
    if (lp != SRILM_LOG0) {
      ll += lp;
      s[i] = si;
      break;
    } else {
      s[i]--;
      ll += lm_logB(lm, &s[i]);
      s[i] = si;
    }
    i++;
  }
  g_assert(ll < 0);
  g_assert(ll > SRILM_LOG0);
  return ll;
}

void sentence_print(Sentence s) {
  for (int i = 1; i <= sentence_size(s); i++) {
    printf("%s ", token_to_string(s[i]));
  }
  printf("\n");
}
