#define DEBUG 0
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "dlib.h"
#include "sentence.h"
#include "lm.h"
#include "fastsubs.h"
#include "heap.h"
uint64_t fs_niter = 0;		// number of pops for performance analysis

/*** Typedefs: representing terms and associated data in eq(5) in the paper */

typedef enum {
  ROOT,		  // root level node, representing e.g. abxcd -> abx + bxc + xcd
  ALT,            // alternatives e.g. abx -> abx | [ab]bx | [ab][b]x; where [] represents bow
  LOGP		  // leaf node e.g. [bx] or abx
} NodeType;

typedef struct _FSnode {
  NodeType type;          // One of ROOT, ALT, or LOGP
  // Ngram ngram;	          // ngram with target word replaced with NULLTOKEN
  struct _FSnode *terms;  // child nodes
  int nterms;		  // number of child nodes
  float offset;		  // ??
  Heap heap;		  // heap of words that can replace the target
  darr_t hash;		  // words already popped from heap
  float umax;		  // ??
  int imax;		  // ??
} *FSnode;

// Token hash table to detect repeat tokens
D_HASH(thash_, Token, Token, d_eqmatch, d_ident, d_ident, d_ident, d_iszero, d_mkzero)

/*** Static functions and macros */
static FSnode fs_alloc(uint32_t order, uint32_t nvocab);

static void fs_init(FSnode n, Sentence s, int target, LM lm);
static void fs_init_alt(FSnode n, Sentence s, int ngram_start, int target, int ngram_end, LM lm);
static void fs_init_logP(FSnode n, Sentence s, int logp_start, int logp_end, float offset, LM lm);

static Hpair fs_pop(FSnode n, Sentence s, int j, LM lm);
static Hpair fs_pop_alt(FSnode n);
static Hpair fs_pop_logp(FSnode n);

static Hpair fs_top(FSnode n, Sentence s, int j, LM lm);
static Hpair fs_top_alt(FSnode n);
static Hpair fs_top_logp(FSnode n);

static inline float fs_lookup_logP(Sentence s, int i, int k, LM lm);
static inline float fs_lookup_logB(Sentence s, int i, int k, LM lm);

#define hget(h,k) thash_get(h, k, false)
#define hset(h,k) thash_get(h, k, true)


/*** The only external function */
int fastsubs(Hpair *subs, Sentence s, int j, LM lm, double plimit, uint32_t nlimit) {
  assert((j >= 1) && (j <= sentence_size(s)));
  uint32_t order = lm_order(lm);
  uint32_t nvocab = lm_nvocab(lm);
  FSnode rootnode = fs_alloc(order, nvocab);
  fs_init(rootnode, s, j, lm);
#if DEBUG
  fs_print_node(rootnode, NULL);
#endif
  int nsubs = 0;
  double psum = 0.0;
  double last_logp = 0.0;
  while (nsubs < nvocab - 1) { /* The -1 is for <s> */
    Hpair pi = fs_pop(rootnode, s, j, lm);
    /* We can have pi.token == NULLTOKEN earlier than nvocab */
    /* assert(pi.token != NULLTOKEN); */
    if (pi.token == NULLTOKEN) break;
    assert(pi.logp <= last_logp);
    last_logp = pi.logp;
    subs[nsubs++] = pi;
    bool nlimit_ok = (nsubs >= nlimit);
    bool plimit_ok = false;
    if (plimit < 1.0) {
      double lastp = exp10(pi.logp);
      psum += lastp;
      double maxrest = lastp * (nvocab - nsubs);
      plimit_ok = (plimit <= (psum/(psum + maxrest)));
    }
    if (nlimit_ok || plimit_ok) break;
  }
  return nsubs;
}

/** fs_init(n, s, target, order): Initialize root node for target word
 * s[target].  If target is X in abXcd and order=3, this creates three
 * alt nodes for abX, bXc, Xcd.
 */
static void fs_init(FSnode n, Sentence s, int target, LM lm) {
  /* Unchanged: */
  /* n->type = ROOT; */
  /* n->ngram = NULL; */
  assert(n->type == ROOT);
  heap_size(n->heap) = 0;
  thash_clear(n->hash);
  n->offset = 0.0;
  n->nterms = 0;
  n->imax = -1;
  uint32_t order = lm_order(lm);
  Token s_target = s[target];
  s[target] = NULLTOKEN;
  for (int ngram_end = target; (ngram_end < target+order) && (ngram_end <= sentence_size(s)); ngram_end++) {
    int ngram_start = ngram_end - order + 1;
    if (ngram_start < 1) ngram_start = 1;
    FSnode ni = &n->terms[n->nterms];
    fs_init_alt(ni, s, ngram_start, target, ngram_end, lm);
    /* check if there are any defined logp terms in the alt node */
    if (ni->nterms > 0) {
      n->nterms++;
    } else {
      /* if there are no defined logp nodes all we need is the offset
	 which represents the lower bound on the cost of an unspecified token. */
      // when does this happen?
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
    /* assert(pi.token != NULLTOKEN); */
    /* however the pi.logp should still be a valid upper bound for this node */
    n->umax += pi.logp;
  }
}

static Hpair fs_top(FSnode n, Sentence s, int target, LM lm) {
  assert(n->type == ROOT);
  uint32_t order = lm_order(lm);

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
      if (i == n->imax) break;	// came full circle
      if (pi.token != NULLTOKEN) break; // found a child with a word
    }
    assert(pi.token != NULLTOKEN); // will always have one, because of the X node
    n->imax = i;		   // record last picked child

    /* update umax (need even if pi.token in hash) */
    pi = fs_pop_alt(ni);
    assert(pi.token != NULLTOKEN);
    Hpair pi2 = fs_top_alt(ni);
    n->umax = n->umax - pi.logp + pi2.logp;

    /* compute logp and insert into heap if not already in hash */
    if (hget(n->hash, pi.token) == NULL) {
      hset(n->hash, pi.token);
      Token s_target = s[target];
      s[target] = pi.token;
      pi.logp = 0;
      for (int ngram_end = target; 
	   (ngram_end < target + order) && 
	     (ngram_end <= sentence_size(s)); 
	   ngram_end++) {
	pi.logp += sentence_logp(s, ngram_end, lm);
      }
      s[target] = s_target;
      heap_insert_max(n->heap, pi.token, pi.logp);
      fs_niter++;
    }
  }

  return heap_top(n->heap);
}

static Hpair fs_pop(FSnode n, Sentence s, int target, LM lm) {
  /* if heap not ready call fs_top() */
  if ((heap_size(n->heap) == 0) || (heap_top(n->heap).logp < n->umax)) {
    fs_top(n, s, target, lm);
  }
  return heap_delete_max(n->heap);
}

/** fs_init_alt(n, s, ngram_start, target, ngram_end, lm): Initialize alt
 * node for ngram s[ngram_start..ngram_end] with target s[target].
 * e.g. If the ngram is abXc we have the following four alternatives:
 * abXc, (abX)bXc, (abX)(bX)Xc, (abX)(bX)(X)c
 *
 * The terms in parens represent the bow values, others logp nodes.
 * The bow terms with X like (abX) can be zero, so we ignore them.
 * The bow and logp terms without X like (a), (ab) are constants.
 * These constants are passed to logp_init, stored in logp->offset,
 * and added to the result during logp->lookup of bXc and Xc.
 * A sum with all constants like (abX)c does not need a logp node, 
 * the largest such sum will be the offset for the alt node.  
 * It will set a lower bound on the values from this alt node.
 */
static void fs_init_alt(FSnode n, Sentence s, int ngram_start, int target, int ngram_end, LM lm) {
  /* Unchanged: */
  /* n->type = ALT; */
  /* n->ngram = NULL; */
  /* n->heap = NULL; */
  /* n->umax = 0.0; */
  assert(n->type == ALT);
  thash_clear(n->hash);		// so we do not return duplicates
  n->imax = -1;
  n->offset = SRILM_LOG0;
  n->nterms = 0;
  int bow_end = ngram_end - 1;
  int logp_end = ngram_end;
  for (int logp_start = ngram_start; logp_start <= ngram_end; logp_start++) {
    float const_sum = 0.0;
    /* Check bow terms, add to const_sum if constant */
    for (int bow_start = ngram_start; bow_start < logp_start; bow_start++) {
      if ((target < bow_start) || (target > bow_end)) {
	const_sum += fs_lookup_logB(s, bow_start, bow_end, lm);
      }
    }
    /* Check the logp term, add to const_sum if constant */
    if (target < logp_start) {
      const_sum += fs_lookup_logP(s, logp_start, logp_end, lm);
      if (const_sum > n->offset) {
	n->offset = const_sum;
      }
    } else {
      /* Construct a logp term if target is inside the logp region */
      FSnode ni = &n->terms[n->nterms];
      fs_init_logP(ni, s, logp_start, logp_end, const_sum, lm);
      /* Do not bother adding terms if logp term lookup fails */
      if (ni->heap != NULL) {
	n->nterms++;
      }
    }
  }
}

/* fs_top_alt(n): 
 * For an alt node the top node is simply the top child with max logp.
 * hash also contains the popped tokens, so no update here.
 * imax = -1 means did not find top child yet
 * imax = nterms means ran out of heap
 * 0 <= imax < nterms points to the max child
 * offset is a lower-bound on top, if top falls below offset just
 * return offset with an unspecified token.
 * Since the logp values coming from children are exact, the top value 
 * is also exact, no need for umax here.
 */

static Hpair fs_top_alt(FSnode n) {
  assert(n->type == ALT);
  if (n->imax == -1) {
    float pmax = n->offset; 	/* do not return anything below offset */
    for (int i = n->nterms - 1; i >= 0; i--) {
      FSnode ni = &n->terms[i];
      Hpair pi = fs_top_logp(ni);
      /* We do not want to return the same token twice. */
      while ((pi.token != NULLTOKEN) && hget(n->hash, pi.token)) {
	fs_pop_logp(ni);
	pi = fs_top_logp(ni);
      }
      if (pi.logp > pmax) {
	pmax = pi.logp;
	n->imax = i;
      }
    }
    if (n->imax == -1) 		/* nothing larger than offset */
      n->imax = n->nterms;
  }
  assert(n->imax >= 0);
  Hpair pi;
  if (n->imax == n->nterms) {
    pi.token = NULLTOKEN; pi.logp = n->offset;
  } else { 
    pi = fs_top_logp(&n->terms[n->imax]);
  }
  return pi;
}

static Hpair fs_pop_alt(FSnode n) {
  assert(n->type == ALT);
  if (n->imax == -1) fs_top_alt(n);
  assert(n->imax >= 0);
  Hpair pi;
  if (n->imax == n->nterms) {
    pi.token = NULLTOKEN; pi.logp = n->offset;
  } else {
    pi = fs_pop_logp(&n->terms[n->imax]);
    assert(pi.token != NULLTOKEN);
    hset(n->hash, pi.token);
    n->imax = -1;
  }
  return pi;
}

/** fs_init_logP(FSnode n, Sentence s, int logp_start, int logp_end, float offset, LMheap h)
  Initialize logP node with the ngram and heap looked up from lmheap.
  The ngram s[logp_start..logp_end] has the target word replaced with NULLTOKEN.
  The heap is actually a (read-only) sorted array of substitutes.
  The offset is to be added to values in the heap.
  When the heap runs out pop returns (NULLTOKEN, SRILM_LOG0)
 */

static void fs_init_logP(FSnode n, Sentence s, int logp_start, int logp_end, float offset, LM lm) {
  /* Unchanged: 
  n->type = LOGP;		// always logp
  n->hash = NULL;		// no chance of repeat so no need
  n->terms = NULL;		// no children
  n->nterms = 0; 		// no children
  n->umax = 0.0; 		// values exact so no need
  */
  assert(n->type == LOGP);
  n->offset = offset;	     // bow value added to every logp in heap before return
  n->imax = 1;		     // keeps position in sorted array n->heap
  // n->ngram = NULL;	     // Do we ever need this?
  n->heap = NULL;	     // sorted array in lmheap
  Token s_logp_start_1 = s[logp_start - 1];
  s[logp_start-1] = logp_end - logp_start + 1;
  n->heap = lm_logP_heap(lm, &s[logp_start - 1]);
  s[logp_start-1] = s_logp_start_1;
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

static inline float fs_lookup_logP(Sentence s, int i, int k, LM lm) {
  Token s_i_1 = s[i-1];
  s[i-1] = k-i+1;
  float logP = lm_logP(lm, &s[i-1]);
  s[i-1] = s_i_1;
  return logP;
}

static inline float fs_lookup_logB(Sentence s, int i, int k, LM lm) {
  Token s_i_1 = s[i-1];
  s[i-1] = k-i+1;
  float logB = lm_logB(lm, &s[i-1]);
  s[i-1] = s_i_1;
  return logB;
}

static FSnode fs_alloc(uint32_t order, uint32_t nvocab) {
  FSnode root = dalloc(sizeof(struct _FSnode));
  memset(root, 0, sizeof(struct _FSnode));
  root->type = ROOT;
  root->heap = dalloc(sizeof(Hpair)*(1 + nvocab));
  root->hash = thash_new(0);
  root->terms = dalloc(order * sizeof(struct _FSnode));
  for (int i = 0; i < order; i++) {
    FSnode alt = &root->terms[i];
    memset(alt, 0, sizeof(struct _FSnode));
    alt->type = ALT;
    alt->hash = thash_new(0);
    alt->terms = dalloc(order * sizeof(struct _FSnode));
    for (int j = 0; j < order; j++) {
      FSnode logp = &alt->terms[j];
      memset(logp, 0, sizeof(struct _FSnode));
      logp->type = LOGP;
    }
  }
  return root;
}

/* debug fn */
void fs_print_node(ptr_t data, ptr_t user_data) {
  FSnode node = (FSnode) data;
  if (node->type != ROOT) printf(" ");
  printf("(");
  switch(node->type) {
  case ROOT: printf("ROOT"); break;
  case ALT: printf("ALT"); break;
  case LOGP: printf("LOGP"); break;
  }
  /*
  if (node->ngram != NULL) {
    printf(" \"");
    for (int i = 1; i <= ngram_size(node->ngram); i++) {
      if (i > 1) printf(" ");
      if (node->ngram[i] == NULLTOKEN) printf("_");
      else printf("%s", token_to_string(node->ngram[i]));
    }
    printf("\"");
  }
  */
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
