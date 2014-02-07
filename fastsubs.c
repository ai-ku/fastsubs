#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "dlib.h"
#include "sentence.h"
#include "heap.h"
#include "lm.h"
#include "fastsubs.h"

/** FASTSUBS: An Efficient and Exact Procedure for Finding the Most
Likely Lexical Substitutes Based on an N-gram Language Model. Deniz
Yuret. 2012. IEEE Signal Processing Letters, vol 19, no 11, pp.
725-728.

The goal is to return a list of most likely words for a given context
according to a language model.  For the context ab_de according to a
backed-off trigram model the score for word X is (eq.5 in paper):

 l(abXde) = (abX | [ab]+bX | [ab]+[b]+X)
          + (bXd | [bX]+Xd | [bX]+[X]+d)
          + (Xde | [Xd]+de | [Xd]+[d]+e)

In general, for an n-gram model, this is a sum of n terms, each of
which has n alternatives (except close to the beginning or the end of
the sentence).  Each alternative is a sum of (log) back-off weights
(denoted with brackets) and log probability values (denoted without
brackets) given by the n-gram model.  X represents the target word.

The code works by recursively building upper-bound queues for terms,
sums, and alternatives.  An upper bound queue is a type of priority
queue where we have upper bounds rather than exact values for
elements.  We keep popping elements and computing actual values until
we find one that is higher than the upper bound of all the remaining
elements. 
*/


/** ngm_q represents a log-probability or log-backoff-weight queue for
an ngram pattern such as [bX] or de.  The pattern is in s[start..end].
s[target] is set to NULLTOKEN.  If the pattern has X, set heap to
sorted array of X's from lm_pheap / lm_bheap.  If the pattern has no X
set val to constant value from lm_phash or lm_bhash.  If not found or
we run out of X's in heap use (NULLTOKEN, SRILM_LOG0) for logP,
(NULLTOKEN, 0) for logB.  The bow terms with X like [bX] are zero for
unseen words, so we do not bother placing any word in the queue that
has a negative bow value.  This was actually mishandled in the paper
and previous versions of the code where bow terms were assumed to be
negative or zero.  There are in fact positive bow terms and they
should be handled.  The logp values for unseen words are SRILM_LOG0,
so logp queues need all the words observed in the LM.  The bow and
logp terms without X like [a], [ab] are constants.  The top term is
heap[index], or if we have a constant term or we have run out of heap
elements heap==NULL and the top term is (NULLTOKEN, val).  */

typedef enum { LOGP, LOGB } ngm_type;

typedef struct ngm_s {
  Heap heap;
  u32 index;
  float val;
} *ngm_q;

static ngm_q ngm_init(ngm_type type, Sentence s, u32 target, u32 start, u32 end, LM lm) {
  u32 len = end - start + 1;
#ifndef NDEBUG
  u32 order = lm_order(lm);
  u32 ssize = sentence_size(s);
  assert(target >= 1);
  assert(target <= ssize);
  assert(start >= 1);
  assert(start <= end);
  assert(end <= (type == LOGP ? ssize : ssize - 1));
  assert(len >= 1);
  assert(len <= (type == LOGP ? order : order - 1));
#endif
  ngm_q q = _d_malloc(sizeof(struct ngm_s));
  *q = (struct ngm_s) { NULL, 1, (type == LOGP ? SRILM_LOG0 : 0) };
  if (target < start || target > end) {
    q->val = (type == LOGP ?
	      lm_phash(lm, s, start, len) :
	      lm_bhash(lm, s, start, len));
  } else {
    q->heap = (type == LOGP ? 
	       lm_pheap(lm, s, start, len) :
	       lm_bheap(lm, s, start, len));
  }
  return q;
}

static void ngm_free(ngm_q q) { _d_free(q); }

static Hpair ngm_top(ngm_q q) {
  if (q->heap == NULL) {
    return ((Hpair) { NULLTOKEN, q->val });
  } else {
    return q->heap[q->index];
  }
}

static Hpair ngm_pop(ngm_q q) {
  Hpair hp;
  if (q->heap == NULL) {
    hp = (Hpair) { NULLTOKEN, q->val };
  } else {
    hp = q->heap[q->index++];
    if (q->index > heap_size(q->heap)) {
      q->heap = NULL;
    }
  }
  return hp;
}


/** sum_q represents a queue for sums such as [aXb]+[Xb]+bc which
gives the logP of the ngram aXbc backed-off to bc.  The ngram is in
s[ngram_start..ngram_end], the position of the target word is marked
with s[target]=NULLTOKEN, and the beginning of the substring
backed-off to is given with logp_start.  Each term is an ngm_q of type
LOGB (with brackets) or LOGP (without brackets).  The top X maximizing
the sum may not be one that maximizes any of the individual terms.
However its value is guaranteed to be less than or equal to the sum of
the top values of individual terms.  When a sum_q is popped, it
returns a random word (actually the top X from one of the terms) and
the upper bound value obeying the upper bound queue contract. */

typedef struct sum_s {
  ngm_q *terms;			// ngram logp and bow terms
  u32 nterm;			// number of terms
  u32 topterm;			// top returns token of this term
} *sum_q;

static sum_q sum_init(Sentence s, u32 target, u32 ngram_start, u32 ngram_end, u32 logp_start, LM lm) {
#ifndef NDEBUG
  u32 order = lm_order(lm);
  u32 ssize = sentence_size(s);
  assert(target >= 1);
  assert(target <= ssize);
  assert(ngram_start >= 1);
  assert(ngram_end >= ngram_start);
  assert(ngram_end <= ssize);
  assert(ngram_end - ngram_start + 1 <= order);
  assert(logp_start >= ngram_start);
  assert(logp_start <= ngram_end);
#endif
  sum_q q = _d_malloc(sizeof(struct sum_s));
  *q = (struct sum_s) { _d_malloc(sizeof(ngm_q) * (logp_start - ngram_start + 1)), 0, 0 };
  q->terms[q->nterm++] = ngm_init(LOGP, s, target, logp_start, ngram_end, lm);
  for (u32 bow_start = ngram_start; bow_start < logp_start; bow_start++) 
    q->terms[q->nterm++] = ngm_init(LOGB, s, target, bow_start, ngram_end - 1, lm);
  for (q->topterm = 0; q->topterm < q->nterm; q->topterm++)
    if (ngm_top(q->terms[q->topterm]).token != NULLTOKEN) break;
  // if all terms return NULLTOKEN we set q->topterm = q->nterm
  return q;
}

static void sum_free(sum_q q) { 
  for (int i = 0; i < q->nterm; i++)
    ngm_free(q->terms[i]);
  _d_free(q->terms); 
  _d_free(q); 
}

static Hpair sum_top(sum_q q) {
  Hpair hp = (Hpair) { NULLTOKEN, 0.0 };
  for (u32 i = 0; i < q->nterm; i++)
    hp.logp += ngm_top(q->terms[i]).logp;
  if (q->topterm < q->nterm)
    hp.token = ngm_top(q->terms[q->topterm]).token;
  return hp;
}

static Hpair sum_pop(sum_q q) {
  Hpair hp = sum_top(q);
  if (hp.token != NULLTOKEN) {
    ngm_pop(q->terms[q->topterm]);
    u32 save = q->topterm;
    for (q->topterm = (q->topterm + 1) % q->nterm;
	 q->topterm != save;
	 q->topterm = (q->topterm + 1) % q->nterm) {
      if (ngm_top(q->terms[q->topterm]).token != NULLTOKEN)
	break;
    }
    if (ngm_top(q->terms[q->topterm]).token == NULLTOKEN)
      q->topterm = q->nterm;
  }
  return hp;
}

/** alt_q represents an alternative queue for ngram s[start..end] with
target s[target].  e.g. If the ngram is abXc we have the following
four alternative sums: abXc, [abX]+bXc, [abX]+[bX]+Xc,
[abX]+[bX]+[X]+c.  The top word in the queue is the one that gives
the highest value with one of these alternatives.  */

typedef struct alt_s {
  sum_q *terms;			// alternative sum terms
  u32 nterm;			// number of terms
  u32 maxterm;			// term with highest logp
} *alt_q;

static alt_q alt_init(Sentence s, u32 target, u32 ngram_start, u32 ngram_end, LM lm) {
#ifndef NDEBUG
  u32 order = lm_order(lm);
  u32 ssize = sentence_size(s);
  assert(target >= 1);
  assert(target <= ssize);
  assert(ngram_start >= 1);
  assert(ngram_start <= ngram_end);
  assert(ngram_end <= ssize);
  assert(ngram_end - ngram_start + 1 <= order);
#endif
  alt_q q = _d_malloc(sizeof(struct alt_s));
  q->nterm = ngram_end - ngram_start + 1;
  q->terms = _d_malloc(sizeof(sum_q) * q->nterm);
  q->maxterm = 0;
  for (int i = 0; i < q->nterm; i++) {
    q->terms[i] = sum_init(s, target, ngram_start, ngram_end, ngram_start + i, lm);
    if (sum_top(q->terms[i]).logp > sum_top(q->terms[q->maxterm]).logp)
      q->maxterm = i;
  }
  return q;
}

static void alt_free(alt_q q) { 
  for (int i = 0; i < q->nterm; i++)
    sum_free(q->terms[i]);
  _d_free(q->terms); 
  _d_free(q); 
}

static Hpair alt_top(alt_q q) {
  return sum_top(q->terms[q->maxterm]);
}

static Hpair alt_pop(alt_q q) {
  Hpair hp = sum_pop(q->terms[q->maxterm]);
  for (int i = 0; i < q->nterm; i++) {
    if (sum_top(q->terms[i]).logp > sum_top(q->terms[q->maxterm]).logp)
      q->maxterm = i;
  }
  return hp;
}

/** root_q: Represents the top level queue for target word s[target].
 * If target is X in abcXdef and order=4, root represents the sum of
 * three alt nodes for abcX, bcXd, cXde, Xdef.  If X is close to the
 * beginning of the sentence (aXbcd), we still have lm_order alt
 * terms (aX, aXb, aXbc, Xbcd) although some shorter.  If X is close
 * to the end of the sentence (abcXd) we may have less than lm_order
 * alt terms (abcX, bcXd) */

typedef struct root_s {
  alt_q *terms;			// alt terms
  u32 nterm;			// number of terms
  u32 topterm;			// top returns token of this term
} *root_q;

static root_q root_init(Sentence s, u32 target, LM lm) {
  u32 order = lm_order(lm);
  u32 ssize = sentence_size(s);
#ifndef NDEBUG
  assert(target >= 1);
  assert(target <= ssize);
#endif
  root_q q = _d_malloc(sizeof(struct root_s));
  q->nterm = ssize - target + 1;
  if (q->nterm > order) q->nterm = order;
  q->terms = _d_malloc(sizeof(alt_q) * q->nterm);
  for (u32 i = 0; i < q->nterm; i++) {
    u32 ngram_end = target + i;
    u32 ngram_start = (order < ngram_end) ? ngram_end - order + 1 : 1;
    q->terms[i] = alt_init(s, target, ngram_start, ngram_end, lm);
  }
  for (q->topterm = 0; q->topterm < q->nterm; q->topterm++)
    if (alt_top(q->terms[q->topterm]).token != NULLTOKEN) break;
  // if all terms return NULLTOKEN we set q->topterm = q->nterm
  return q;
}

static void root_free(root_q q) {
  for (int i = 0; i < q->nterm; i++)
    alt_free(q->terms[i]);
  _d_free(q->terms);
  _d_free(q);
}

static Hpair root_top(root_q q) {
  Hpair hp = (Hpair) { NULLTOKEN, 0.0 };
  for (u32 i = 0; i < q->nterm; i++)
    hp.logp += alt_top(q->terms[i]).logp;
  if (q->topterm < q->nterm)
    hp.token = alt_top(q->terms[q->topterm]).token;
  return hp;
}

static Hpair root_pop(root_q q) {
  Hpair hp = root_top(q);
  if (hp.token != NULLTOKEN) {
    alt_pop(q->terms[q->topterm]);
    u32 save = q->topterm;
    for (q->topterm = (q->topterm + 1) % q->nterm;
	 q->topterm != save;
	 q->topterm = (q->topterm + 1) % q->nterm) {
      if (alt_top(q->terms[q->topterm]).token != NULLTOKEN)
	break;
    }
    if (alt_top(q->terms[q->topterm]).token == NULLTOKEN)
      q->topterm = q->nterm;
  }
  return hp;
}

/** Helpers */

// Token hash table to detect repeat tokens
D_HASH(tokhash_, Token, Token, d_eqmatch, d_ident, d_ident, d_ident, d_iszero, d_mkzero)

// calculate the real logp of a token
static float real_logp(Token tok, Sentence s, u32 target, LM lm) {
  u32 order = lm_order(lm);
  u32 ssize = sentence_size(s);
#ifndef NDEBUG
  assert(tok != NULLTOKEN);
  assert(target >= 1);
  assert(target <= ssize);
#endif  
  Token s_target = s[target];
  s[target] = tok;
  float logp = 0;		// TODO: this should do internal sum with double
  for (u32 i = 0; i < order; i++) {
    u32 ngram_end = target + i;
    if (ngram_end > ssize) break;
    logp += lm_logp(lm, s, ngram_end);
  }
  s[target] = s_target;
  return logp;
}

/** fastsubs(): Fills the pre-allocated subs array (should have nvocab
elements to be safe) with substitutes of word j in sentence s
according to LMheap h.  The substitutes are sorted by descending
probability and the Hpair structure pairs them with their
(unnormalized) log10_probability.  The number of substitutes found is
controlled by plimit which lower bounds the total (normalized)
probability of the returned subs, or nlimit which lower bounds the
number of returned subs.  Both limits have to be satisfied, so just
pass 0 for a limit you do not care about.  The number of entries
placed in the subs array is returned.
 */

u32 fastsubs(Hpair *subs, Sentence s, u32 target, LM lm, double plimit, u32 nlimit) {
  u32 nvocab = lm_nvocab(lm);
#ifndef NDEBUG
  u32 ssize = sentence_size(s);
  assert(target >= 1);
  assert(target <= ssize);
#endif
  Heap heap = _d_malloc(sizeof(Hpair) * (1 + nvocab));
  heap_size(heap) = 0;		// word heap with real scores
  darr_t hash = tokhash_new(0);	// word hash to avoid repetition
  Token s_target = s[target];
  s[target] = NULLTOKEN;
  root_q root = root_init(s, target, lm); // upper bound queue
  s[target] = s_target;
#ifndef NDEBUG
  double last_logp = 0.0;
#endif
  u32 nsubs = 0;
  double psum = 0.0;
  while (nsubs < nvocab - 1) { /* The -1 is for <s> */
    Hpair rtop = root_top(root);
    while ((heap_size(heap) == 0) || heap_top(heap).logp < rtop.logp) {
      // fs_niter++;
      Hpair rpop = root_pop(root);
      rtop = root_top(root);
      assert(rpop.token != NULLTOKEN);
      if (!tokhash_get(hash, rpop.token, false)) {
	tokhash_get(hash, rpop.token, true);
	rpop.logp = real_logp(rpop.token, s, target, lm);
	heap_insert_max(heap, rpop.token, rpop.logp);
      }
    }
    Hpair hpop = heap_delete_max(heap);
#ifndef NDEBUG
    assert(hpop.token != NULLTOKEN);
    assert(hpop.logp <= last_logp);
    last_logp = hpop.logp;
#endif
    subs[nsubs++] = hpop;
    bool nlimit_ok = (nsubs >= nlimit);
    bool plimit_ok = false;
    if (plimit < 1.0) {
      double lastp = pow(10.0, hpop.logp);
      psum += lastp;
      double maxrest = lastp * (nvocab - nsubs);
      plimit_ok = (plimit <= (psum/(psum + maxrest)));
    }
    if (nlimit_ok || plimit_ok) break;
  }
  root_free(root);
  tokhash_free(hash);
  _d_free(heap);
  return nsubs;
}

