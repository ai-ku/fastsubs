#define DEBUG 0
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "minialloc.h"
#include "sentence.h"
#include "lmheap.h"
#include "lm.h"
#include "fastsubs.h"
#include "heap.h"

guint fs_niter = 0;		/* number of pops for performance analysis */

static LM lm1;
static LMheap lmheap1;
static FSnode rootnode;

static FSnode fs_alloc();

static void fs_init(FSnode n, Sentence s, int target);
static void fs_init_alt(FSnode n, Sentence s, int ngram_start, int target, int ngram_end);
static void fs_init_logP(FSnode n, Sentence s, int logp_start, int logp_end, gfloat offset);

static Hpair fs_pop(FSnode n, Sentence s, int j);
static Hpair fs_pop_alt(FSnode n);
static Hpair fs_pop_logp(FSnode n);

static Hpair fs_top(FSnode n, Sentence s, int j);
static Hpair fs_top_alt(FSnode n);
static Hpair fs_top_logp(FSnode n);

static gfloat fs_lookup(Sentence s, int i, int k, gboolean bow);
#define fs_lookup_logP(s,i,k) fs_lookup(s,i,k,FALSE)
#define fs_lookup_logB(s,i,k) fs_lookup(s,i,k,TRUE)

#define hget(h,k) g_hash_table_lookup((h),GUINT_TO_POINTER(k))
#define hset(h,k) g_hash_table_insert((h),GUINT_TO_POINTER(k),(h))


int fastsubs(Hpair *subs, Sentence s, int j, LM lm, gdouble plimit, guint nlimit) {
  g_assert((j >= 1) && (j <= sentence_size(s)));
  if (lm1 == NULL) {
    lm1 = lm;
    lmheap1 = lmheap_init(lm);
    rootnode = fs_alloc(lm->order);
  } else {
    g_assert(lm == lm1);
  }
  fs_init(rootnode, s, j);
#if DEBUG
  fs_print_node(rootnode, NULL);
#endif
  int nsubs = 0;
  gdouble psum = 0.0;
  while (nsubs < lm->nvocab) {
    Hpair pi = fs_pop(rootnode, s, j);
    g_assert(pi.token != NULLTOKEN);
    subs[nsubs++] = pi;
    gboolean nlimit_ok = (nsubs >= nlimit);
    gboolean plimit_ok = FALSE;
    if (plimit < 1.0) {
      gdouble lastp = exp10(pi.logp);
      psum += lastp;
      gdouble maxrest = lastp * (lm->nvocab - nsubs);
      plimit_ok = (plimit <= (psum/(psum + maxrest)));
    }
    if (nlimit_ok || plimit_ok) break;
  }
  return nsubs;
}

static FSnode fs_alloc() {
  FSnode root = minialloc(sizeof(struct _FSnode));
  memset(root, 0, sizeof(struct _FSnode));
  root->type = ROOT;
  root->heap = minialloc(sizeof(Hpair)*(1 + lm1->nvocab));
  root->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
  root->terms = minialloc(lm1->order * sizeof(struct _FSnode));
  for (int i = 0; i < lm1->order; i++) {
    FSnode alt = &root->terms[i];
    memset(alt, 0, sizeof(struct _FSnode));
    alt->type = ALT;
    alt->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    alt->terms = minialloc(lm1->order * sizeof(struct _FSnode));
    for (int j = 0; j < lm1->order; j++) {
      FSnode logp = &alt->terms[j];
      memset(logp, 0, sizeof(struct _FSnode));
      logp->type = LOGP;
    }
  }
  return root;
}

/** fs_init(n, s, target): Initialize root node for target word
 * s[target].  If target is X in abXcd and order=3, this creates three
 * alt nodes for abX, bXc, Xcd.
 */
static void fs_init(FSnode n, Sentence s, int target) {
  /* Unchanged: */
  /* n->type = ROOT; */
  /* n->ngram = NULL; */
  g_assert(n->type == ROOT);
  heap_size(n->heap) = 0;
  g_hash_table_remove_all(n->hash);
  n->offset = 0.0;
  n->nterms = 0;
  n->imax = -1;
  Token s_target = s[target];
  s[target] = NULLTOKEN;
  int order = lm1->order;
  for (int ngram_end = target; (ngram_end < target+order) && (ngram_end <= sentence_size(s)); ngram_end++) {
    int ngram_start = ngram_end - order + 1;
    if (ngram_start < 1) ngram_start = 1;
    FSnode ni = &n->terms[n->nterms];
    fs_init_alt(ni, s, ngram_start, target, ngram_end);
    /* check if there are any defined logp terms in the alt node */
    if (ni->nterms > 0) {
      n->nterms++;
    } else {
      /* if there are no defined logp nodes all we need is the offset
	 which represents the lower bound on the cost of an unspecified token. */
      n->offset += ni->offset;
    }
  }
  s[target] = s_target;
  n->umax = n->offset;
  for (int i = 0; i < n->nterms; i++) {
    FSnode ni = &n->terms[i];
    Hpair pi = fs_top_alt(ni);
    /* pi.token could be NULL if none of the tokens have score above alt.offset */
    /* even if we manage not to create any empty alt nodes */
    /* g_assert(pi.token != NULLTOKEN); */
    /* however the pi.logp should still be a valid upper bound for this node */
    n->umax += pi.logp;
  }
}

/** fs_init_alt(n, s, ngram_start, target, ngram_end): Initialize alt
 * node for ngram s[ngram_start..ngram_end] with target s[target].
 * e.g. In the abXcd example:
 *
 * abX => abX, (ab)bX, (ab)(b)X
 * bXc => bXc, (bX)Xc, (bX)(X)c
 * Xcd => Xcd, (Xc)cd, (Xc)(c)d
 *
 * The bow terms with X can be zero, so we ignore them.
 * The bow and logp terms without X are constants.
 * A sum with all constants does not need a logp node, the largest
 * such sum will be the offset for the alt node.  It will set a lower
 * bound on the values from this alt node.
 */
static void fs_init_alt(FSnode n, Sentence s, int ngram_start, int target, int ngram_end) {
  /* Unchanged: */
  /* n->type = ALT; */
  /* n->ngram = NULL; */
  /* n->heap = NULL; */
  /* n->umax = 0.0; */
  g_assert(n->type == ALT);
  g_hash_table_remove_all(n->hash);
  n->imax = -1;
  n->offset = SRILM_LOG0;
  n->nterms = 0;
  int bow_end = ngram_end - 1;
  int logp_end = ngram_end;
  for (int logp_start = ngram_start; logp_start <= ngram_end; logp_start++) {
    gfloat const_sum = 0.0;
    /* Check bow terms, add to const_sum if constant */
    for (int bow_start = ngram_start; bow_start < logp_start; bow_start++) {
      if ((target < bow_start) || (target > bow_end)) {
	const_sum += fs_lookup_logB(s, bow_start, bow_end);
      }
    }
    /* Check the logp term, add to const_sum if constant */
    if (target < logp_start) {
      const_sum += fs_lookup_logP(s, logp_start, logp_end);
      if (const_sum > n->offset) {
	n->offset = const_sum;
      }
    } else {
      /* Construct a logp term if target is inside the logp region */
      FSnode ni = &n->terms[n->nterms];
      fs_init_logP(ni, s, logp_start, logp_end, const_sum);
      /* Do not bother adding terms if logp term lookup fails */
      if (ni->ngram != NULL) {
	n->nterms++;
      }
    }
  }
}

static gfloat fs_lookup(Sentence s, int i, int k, gboolean bow) {
  Token s_i_1 = s[i-1];
  s[i-1] = k-i+1;
  gfloat logP = (bow ? lm_logB(lm1, &s[i-1]) : lm_logP(lm1, &s[i-1]));
  s[i-1] = s_i_1;
  return logP;
}

static void fs_init_logP(FSnode n, Sentence s, int logp_start, int logp_end, gfloat offset) {
  /* Unchanged: */
  /* n->type = LOGP; */
  /* n->hash = NULL; */
  /* n->terms = NULL; */
  /* n->nterms = 0; */
  /* n->umax = 0.0; */
  g_assert(n->type == LOGP);
  n->offset = offset;
  n->imax = 1;
  n->ngram = NULL;
  n->heap = NULL;
  Token s_logp_start_1 = s[logp_start - 1];
  s[logp_start-1] = logp_end - logp_start + 1;
  g_hash_table_lookup_extended(lmheap1, &s[logp_start - 1], (gpointer*) &n->ngram, (gpointer*) &n->heap);
  s[logp_start-1] = s_logp_start_1;
}


static Hpair fs_top(FSnode n, Sentence s, int target) {
  g_assert(n->type == ROOT);

  /* n->umax should be equal to the sum of ni->top (where ni are the
     child nodes) plus n->offset, therefore an upper bound on how high
     the next word's logp can be.  If top(n).logp is equal or higher
     than umax no unchecked word can surpass it. */

  while ((heap_size(n->heap) == 0) || (heap_top(n->heap).logp < n->umax)) {

    /* find the next word to lookup. ideally this should be the one
       whose deletion will decrease n->umax the most, but for now we
       will just pick the next valid child. */
    FSnode ni;
    Hpair pi;
    int i = n->imax;
    while (1) {
      i++;
      if (i >= n->nterms) i = 0;
      ni = &n->terms[i];
      pi = fs_top_alt(ni);
      if (i == n->imax) break;
      if (pi.token != NULLTOKEN) break;
    }
    g_assert(pi.token != NULLTOKEN);
    n->imax = i;

    /* update umax (need even if pi.token in hash) */
    pi = fs_pop_alt(ni);
    g_assert(pi.token != NULLTOKEN);
    Hpair pi2 = fs_top_alt(ni);
    n->umax = n->umax - pi.logp + pi2.logp;

    /* compute logp and insert into heap if not already in hash */
    if (hget(n->hash, pi.token) == NULL) {
      hset(n->hash, pi.token);
      Token s_target = s[target];
      s[target] = pi.token;
      pi.logp = 0;
      for (int ngram_end = target; 
	   (ngram_end < target + lm1->order) && 
	     (ngram_end <= sentence_size(s)); 
	   ngram_end++) {
	pi.logp += sentence_logp(s, ngram_end, lm1);
      }
      s[target] = s_target;
      heap_insert_max(n->heap, pi.token, pi.logp);
      fs_niter++;
    }
  }

  return heap_top(n->heap);
}

static Hpair fs_pop(FSnode n, Sentence s, int target) {
  /* if heap not ready call fs_top() */
  if ((heap_size(n->heap) == 0) || (heap_top(n->heap).logp < n->umax)) {
    fs_top(n, s, target);
  }
  return heap_delete_max(n->heap);
}

/* fs_top_alt(n): 
 * For an alt node the top node is simply the top child with max logp.
 * hash also contains the popped tokens, so no update here.
 * imax = -1 means did not find top child yet
 * imax = nterms means ran out of heap
 * 0 <= imax < nterms points to the max child
 * offset is a lower-bound on top, if top falls below offset just
 * return offset with an unspecified token.
 */

static Hpair fs_top_alt(FSnode n) {
  g_assert(n->type == ALT);
  if (n->imax == -1) {
    gfloat pmax = n->offset; 	/* do not return anything below offset */
    for (int i = n->nterms - 1; i >= 0; i--) {
      FSnode ni = &n->terms[i];
      Hpair pi = fs_top_logp(ni);
      /* We do not want to return the same token twice. */
      while ((pi.token != NULLTOKEN) && hget(n->hash, pi.token)) {
	fs_pop_logp(ni);
	pi = fs_top_logp(ni);
      }
      if (pi.logp >= pmax) {
	pmax = pi.logp;
	n->imax = i;
      }
    }
    if (n->imax == -1) 		/* nothing larger than offset */
      n->imax = n->nterms;
  }
  g_assert(n->imax >= 0);
  Hpair pi;
  if (n->imax == n->nterms) {
    pi.token = NULLTOKEN; pi.logp = n->offset;
  } else { 
    pi = fs_top_logp(&n->terms[n->imax]);
  }
  return pi;
}

static Hpair fs_pop_alt(FSnode n) {
  g_assert(n->type == ALT);
  if (n->imax == -1) fs_top_alt(n);
  g_assert(n->imax >= 0);
  Hpair pi;
  if (n->imax == n->nterms) {
    pi.token = NULLTOKEN; pi.logp = n->offset;
  } else {
    pi = fs_pop_logp(&n->terms[n->imax]);
    g_assert(pi.token != NULLTOKEN);
    hset(n->hash, pi.token);
    n->imax = -1;
  }
  return pi;
}

static Hpair fs_pop_logp(FSnode n) {
  Hpair pi = fs_top_logp(n);
  n->imax++;
  return pi;
}

static Hpair fs_top_logp(FSnode n) {
  Hpair pi;
  if ((n->heap == NULL) || (n->imax > heap_size(n->heap))) {
    pi.token = NULLTOKEN; pi.logp = SRILM_LOG0;
  } else {
    pi = n->heap[n->imax];
    pi.logp += n->offset;
  }
  return pi;
}

/* debug fn */
void fs_print_node(gpointer data, gpointer user_data) {
  FSnode node = (FSnode) data;
  if (node->type != ROOT) printf(" ");
  printf("(");
  switch(node->type) {
  case ROOT: printf("ROOT"); break;
  case ALT: printf("ALT"); break;
  case LOGP: printf("LOGP"); break;
  }
  if (node->ngram != NULL) {
    printf(" \"");
    for (int i = 1; i <= ngram_size(node->ngram); i++) {
      if (i > 1) printf(" ");
      if (node->ngram[i] == NULLTOKEN) printf("_");
      else printf("%s", token_to_string(node->ngram[i]));
    }
    printf("\"");
  }
  if ((node->offset != 0) && (node->offset != SRILM_LOG0)) {
    printf(" offset=%g", node->offset);
  }
  if (node->nterms > 0) {
    for (int i = 0; i < node->nterms; i++) {
      fs_print_node(&node->terms[i], NULL);
    }
  }
  printf(")");
  if (node->type == ROOT) printf("\n");
}
