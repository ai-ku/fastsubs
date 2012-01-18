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

gdouble sentence_logp(Sentence s, int j, LM lm) {
  g_assert((j >= 1) && (j <= sentence_size(s)));
  if (j==1) return 0;		/* First token is always SOS */
  int i = j - lm_ngram_order(lm);
  if (i < 0) i = 0;
  gdouble ll = 0;
  while (i < j) {
    Token si = s[i];
    s[i] = (guint32) (j - i);	/* ngram order */
    gdouble lp = lm_logP(lm, &s[i]);
    if (lp != SRILM_LOG0) {
      ll += lp;
      s[i] = si;
      break;
    } else {
      s[i]--;
      gdouble lb = lm_logB(lm, &s[i]);
      if (lb != SRILM_LOG0) {
	ll += lb;
      }
      s[i] = si;
    }
    i++;
  }
  g_assert(ll < 0);
  g_assert(ll > SRILM_LOG0);
  return ll;
}
