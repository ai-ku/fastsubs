#include "foreach.h"
#include "sentence.h"

static Token SOS, EOS, UNK;

static void init_special_tokens() {
  SOS = token_from_string("<s>");
  EOS = token_from_string("</s>");
  UNK = token_from_string("<unk>");
}

guint32 sentence_from_string(Sentence st, char *str, int nmax) {
  if (SOS == 0) init_special_tokens();
  guint32 ntok = 0;
  g_assert(ntok < nmax);
  st[++ntok] = SOS;
  foreach_token(word, str) {
    Token wtok = token_try_string(word);
    g_assert(ntok < nmax);
    st[++ntok] = (wtok == 0 ? UNK : wtok);
  }
  g_assert(ntok < nmax);
  st[++ntok] = EOS;
  st[0] = ntok;
  return ntok;
}

gfloat sentence_logp(Sentence s, int j, LM lm) {
  g_assert((j >= 2) && (j <= sentence_size(s))); /* s[1] always SOS */
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
