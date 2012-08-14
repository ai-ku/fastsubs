/* -*- mode: C; mode: Outline-minor; outline-regexp: "/[*][*]+"; -*- */
const char *rcsid = "$Id: subs.c,v 1.4 2010/05/02 07:36:07 dyuret Exp $";
const char *help = "subs lmfile < text > output";

/** Header 
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <glib.h>
#include "foreach.h"
#include "procinfo.h"
#include "ghashx.h"

#define MAX_SENTENCE_LEN 1024
#define MAX_NGRAM_ORDER 5
#define SRILM_LOG0 -99
#define NULLTOK 0

/** Tokens are represented as guint32 integers (GQuarks).  The data
 *  types like GQuark, GString etc. are defined in glib2.
 */

typedef GQuark Token;
#define token_from_string g_quark_from_string
#define token_try_string g_quark_try_string
#define token_to_string g_quark_to_string

Token UNK;
Token SOS;
Token EOS;

void init_special_tokens() {
  SOS = token_from_string("<s>");
  EOS = token_from_string("</s>");
  UNK = token_from_string("<unk>");
}

/** Sentences are represented as Token arrays surrounded by SOS and
    EOS tags and terminated by a NULLTOK tag.  NULLTOK is necessary
    because we may try replacing the EOS tag.  The token count
    returned by sentence_from_string includes the SOS and EOS tags but
    excludes the NULLTOK.
 */

typedef Token *Sentence;

int sentence_from_string(Sentence st, char *str, int nmax) {
  if (SOS == 0) init_special_tokens();
  int ntok = 0;
  st[ntok++] = SOS;
  foreach_token(word, str) {
    g_assert(ntok < nmax);
    Token wtok = token_try_string(word);
    st[ntok++] = (wtok == 0 ? UNK : wtok);
  }
  g_assert(ntok < nmax-1);
  st[ntok++] = EOS;
  st[ntok] = NULLTOK;
  return ntok;
}

/** Ngrams are represented as NULLTOK terminated Token arrays.
 */

typedef Token *Ngram;

int ngram_size(Ngram ng) {
  int n = 0;
  while (ng[n] != NULLTOK) n++;
  return n;
}

#define ngram_copy(p) g_memdup(p, (ngram_size(p)+1)*sizeof(Token))
void ngram_free(gpointer p) { if (p != NULL) g_free(p); }

int ngram_from_string(Ngram ng, char *str, int nmax) {
  int ntok = 0;
  foreach_token(word, str) {
    g_assert(ntok < nmax);
    ng[ntok++] = token_from_string(word);
  }
  g_assert(ntok < nmax);
  ng[ntok] = NULLTOK;
  return ntok;
}

char *ngram_to_gstring(GString *g, Ngram ng) {
  g_string_truncate(g, 0);
  for (int n = 0; ng[n] != NULLTOK; n++) {
    if (n > 0) g_string_append_c(g, ' ');
    g_string_append(g, token_to_string(ng[n]));
  }
  return g->str;
}

/** Ngram hash functions
 */

static guint ngram_hash_rnd[MAX_NGRAM_ORDER];

void init_ngram_hash_rnd() {
  if (ngram_hash_rnd[0] == 0) {
    for (int i = MAX_NGRAM_ORDER-1; i >= 0; i--) {
      ngram_hash_rnd[i] = 1 + g_random_int();
    }
  }
}

guint ngram_hash_function(Ngram ng) {
  guint hash = 0;
  for (int i = 0; ng[i] != NULLTOK; i++) {
    hash += ng[i] * ngram_hash_rnd[i];
  }
  return hash;
}

gboolean ngram_equal(Ngram a, Ngram b) {
  for (int i = 0; ; i++) {
    if (a[i] != b[i]) return 0;
    if (a[i] == NULLTOK) break;
  }
  return 1;
}

/** LogProb structure has two entries: the first entry represents the
 * base 10 logarithm of the conditional probability of the last token
 * given the other tokens of the N-gram.  The second entry is the
 * base-10 logarithm of the backoff weight for (n+1)-grams starting
 * with that ngram.
 */

typedef struct LogProbS {
  gdouble logp;
  gdouble bow;
} *LogProb;

LogProb logprob_new(gdouble logp, gdouble bow) { 
  LogProb lp = g_new(struct LogProbS, 1); 
  lp->logp = logp;
  lp->bow = bow;
  return lp;
}
void logprob_free(gpointer p) { if (p != NULL) g_free(p); }


/** The language model is represented as a structure with three
 * elements.  Lookup is an ngram hash where the keys are Ngrams and
 * values are LogProb structures.  Order is the maximum order of
 * ngrams in the lm.  Maxtoken is the token with the largest guint32
 * value in the lm.
 */

typedef struct LMS {
  Hash lookup;
  guint order;
  Token maxtoken;
} *LM;

LM lm_new() {
  init_ngram_hash_rnd();
  LM lm = g_new(struct LMS, 1);
  lm->order = 0;
  lm->maxtoken = 0;
  lm->lookup = g_hash_table_new_full((guint(*)(gconstpointer)) ngram_hash_function,
				     (gboolean(*)(gconstpointer,gconstpointer)) ngram_equal,
				     ngram_free, logprob_free);
  return lm;
}

void lm_free(LM lm) {
  if (lm != NULL) {
    if (lm->lookup != NULL) 
      g_hash_table_destroy(lm->lookup);
    g_free(lm);
  }
}

/* read LM from ARPA backoff N-gram model file format 
 */
LM read_lm(char *lmfile) {
  Token ng[MAX_NGRAM_ORDER + 1];
  LM lm = lm_new();
  Hash ht = lm->lookup;
  foreach_line(str, lmfile) {
    if (*str == '\n' || *str == '\\' || *str == 'n') continue;
    errno = 0;
    LogProb lp = logprob_new(0, 0);
    char *s = strtok(str, "\t");
    lp->logp = strtod(s, NULL);
    g_assert(errno == 0);
    s = strtok(NULL, "\t\n");
    int n = ngram_from_string(ng, s, MAX_NGRAM_ORDER);
    if (n > lm->order) lm->order = n;
    if (hgot(ht, ng)) g_error("Duplicate ngram");
    s = strtok(NULL, "\n");
    if (s != NULL) lp->bow = strtod(s, NULL);
    g_assert(errno == 0);
    hset(ht, ngram_copy(ng), lp);
    for (int i = 0; i < n; i++) {
      if (ng[i] > lm->maxtoken) lm->maxtoken = ng[i];
    }
  }
  return lm;
}

/** Probabilities
 */

gdouble logL(Token *s, int j, LM lm) {
  gdouble ll = 0;
  Token sj = s[j];
  Token sj1 = s[j+1];
  s[j+1] = 0;
  int i = j - lm->order + 1;
  if (i < 0) i = 0;
  while (i <= j) {
    LogProb lp = hget(lm->lookup, &s[i]);
    if (lp != NULL) {
      ll += lp->logp;
      break;
    } else {
      s[j] = 0;
      lp = hget(lm->lookup, &s[i]);
      if (lp != NULL) {
	ll += lp->bow;
      }
      s[j] = sj;
    }
    i++;
  }
  s[j+1] = sj1;
  g_assert(ll < 0);
  /* This one fails for the SOS token: */
  //g_assert(ll > SRILM_LOG0);
  return ll;
}

void logL_all(Token *s, int j, LM lm, gdouble *ll) {
  Token sj = s[j];
  for (Token w = lm->maxtoken; w > 0; w--) {
    s[j] = w;
    ll[w] = logL(s, j, lm);
  }
  s[j] = sj;
}

gdouble logL2(Token *s, int j, LM lm) {
  gdouble ll = 0;
  for (int k = j; k < j + lm->order; k++) {
    if (s[k] == NULLTOK) break;
    ll += logL(s, k, lm);
  }
  return ll;
}

void logL2_all(Token *s, int j, LM lm, gdouble *ll) {
  Token sj = s[j];
  for (Token w = lm->maxtoken; w > 0; w--) {
    s[j] = w;
    ll[w] = logL2(s, j, lm);
  }
  s[j] = sj;
}

/** Main
 */

int main(int argc, char *argv[]) {
  static Token s[MAX_SENTENCE_LEN];
  g_message_init();
  g_message("%s", rcsid);
  if (argc != 2) g_error("%s", help);
  char *lmfile = argv[1];

  g_message("Reading language model from %s...", lmfile);
  LM lm = read_lm(lmfile);

  gdouble *ll = g_new0(gdouble, 1 + lm->maxtoken);

  g_message("Reading input from stdin...");
  foreach_line(str, "") {
    int n = sentence_from_string(s, str, MAX_SENTENCE_LEN);
    for (int i = 1; i < n; i++) {
      logL2_all(s, i, lm, ll);
      printf("%s_%g", token_to_string(s[i]), ll[s[i]]);
      for (Token w = 1; w <= lm->maxtoken; w++) {
	if (w != s[i]) {
	  printf(" %s_%g", token_to_string(w), ll[w]);
	}
      }
      printf("\n");
    }
  }
  g_message("done");
  lm_free(lm);
  g_free(ll);
}

