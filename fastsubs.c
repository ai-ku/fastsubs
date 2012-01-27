#define DEBUG 1
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

#define hget(h,k) g_hash_table_lookup((h),GUINT_TO_POINTER(k))
#define hset(h,k) g_hash_table_insert((h),GUINT_TO_POINTER(k),(h))

const Hpair NULLPAIR = {NULLTOKEN,SRILM_LOG0};

static LM lm1;
static LMheap lmheap1;
static FSnode rootnode;

static FSnode fs_alloc();
static void fs_alloc_alt(FSnode n);
static void fs_alloc_sum(FSnode n);

static void fs_init(FSnode n, Sentence s, int j);
static void fs_init_alt(FSnode n, Sentence s, int i, int k);
static void fs_init_sum(FSnode n, Sentence s, int i, int l, int k);
static void fs_init_logX(FSnode n, Sentence s, int i, int k, gboolean bow);
#define fs_init_logP(n,s,i,k) fs_init_logX(n,s,i,k,FALSE)
#define fs_init_logB(n,s,i,k) fs_init_logX(n,s,i,k,TRUE)

static Hpair fs_pop(FSnode n, Sentence s, int j);
static Hpair fs_pop_alt(FSnode n);
static Hpair fs_pop_sum(FSnode n);
static Hpair fs_pop_sum1(FSnode n);
static Hpair fs_pop_logp(FSnode n);

static Hpair fs_top(FSnode n, Sentence s, int j);
static Hpair fs_top_alt(FSnode n);
static Hpair fs_top_sum(FSnode n);
static Hpair fs_top_sum1(FSnode n);
static Hpair fs_top_logp(FSnode n);

static gfloat fs_lookup(Sentence s, int i, int k, gboolean bow);
#define fs_lookup_logP(s,i,k) fs_lookup(s,i,k,FALSE)
#define fs_lookup_logB(s,i,k) fs_lookup(s,i,k,TRUE)
static gboolean fs_has_target(Sentence s, int i, int k);
static void fs_ngram_sub(Ngram ng1, Ngram ng, Token tok);
static void fs_print_node(gpointer data, gpointer user_data);

int fastsubs(Hpair *subs, Sentence s, int j, LM lm, gfloat plimit, guint nlimit) {
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
  gfloat psum = 0;
  while (nsubs < lm->nvocab) {
    subs[nsubs++] = fs_pop(rootnode, s, j);
    gboolean nlimit_ok = (nsubs >= nlimit);
    gboolean plimit_ok = TRUE;
    if (plimit > 0) {
      gfloat lastp = exp10f(subs[nsubs-1].logp);
      psum += lastp;
      gfloat maxrest = lastp * (lm->nvocab - nsubs);
      plimit_ok = (plimit <= (psum/(psum + maxrest)));
    }
    if (nlimit_ok && plimit_ok) break;
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
    fs_alloc_alt(&root->terms[i]);
  }
  return root;
}

static void fs_init(FSnode n, Sentence s, int j) {
  /* Unchanged: */
  /* n->type = ROOT; */
  /* n->ngram = NULL; */
  /* n->offset = 0; */
  heap_size(n->heap) = 0;
  g_hash_table_remove_all(n->hash);
  n->umax = 0.0;
  n->nterms = 0;
  Token s_j = s[j];
  s[j] = NULLTOKEN;
  int order = lm1->order;
  for (int k = j; (k < j+order) && (k <= sentence_size(s)); k++) {
    int i = k - order + 1;
    if (i < 1) i = 1;
    fs_init_alt(&n->terms[n->nterms++], s, i, k);
  }
  s[j] = s_j;
}

static Hpair fs_top(FSnode n, Sentence s, int j) {
  g_assert(n->type == ROOT);

  /* n->umax should be equal to the sum of ni->umax (which itself is
     equal to the logp of last popped item from ni), therefore an
     upper bound on how high the next word's logp can be.  If
     top(n).logp is equal or higher than umax no unchecked word can
     surpass it. */

  while ((heap_size(n->heap) == 0) || (heap_top(n->heap).logp < n->umax)) {

    /* find the next word to lookup. ideally this should be the one
       whose deletion will decrease umax the most.  */
    gfloat dmax = 0;
    int imax = 0;
    for (int i = n->nterms - 1; i >= 0; i--) {
      FSnode ni = &n->terms[i];
      Hpair pi = fs_top_alt(ni);
      gfloat diff = ni->umax - pi.logp;
      if (diff > dmax) {
	dmax = diff;
	imax = i;
      }
    }

    /* update umax (need even if pi.token in hash) */
    FSnode ni = &n->terms[imax];
    gfloat ni_umax = ni->umax;	/* save old ni->umax */
    Hpair pi = fs_pop_alt(ni);	/* this should update ni.umax <- pi.logp */
    g_assert(ni->umax == pi.logp);
    g_assert(ni->umax <= ni_umax);
    n->umax = n->umax - ni_umax + ni->umax;

    /* compute logp and insert into heap if not already in hash */
    if (hget(n->hash, pi.token) == NULL) {
      hset(n->hash, pi.token);
      Token s_j = s[j];
      s[j] = pi.token;
      pi.logp = 0;
      int k = j + lm1->order - 1;
      if (k > sentence_size(s)) k = sentence_size(s);
      while (k >= j) {
	pi.logp += sentence_logp(s, k--, lm1);
      }
      s[j] = s_j;
      heap_insert_max(n->heap, pi.token, pi.logp);
    }
  }

  return heap_top(n->heap);
}

static Hpair fs_pop(FSnode n, Sentence s, int j) {
  /* if heap not ready call fs_top() */
  if ((heap_size(n->heap) == 0) || (heap_top(n->heap).logp < n->umax)) {
    fs_top(n, s, j);
  }
  return heap_delete_max(n->heap);
}

static void fs_alloc_alt(FSnode alt) {
    memset(alt, 0, sizeof(struct _FSnode));
    alt->type = ALT;
    alt->terms = minialloc(lm1->order * sizeof(struct _FSnode));
    alt->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    for (int j = 0; j < lm1->order; j++) {
      fs_alloc_sum(&alt->terms[j]);
    }
}

static void fs_init_alt(FSnode n, Sentence s, int i, int k) {
  g_assert(n->type == ALT);

  /* Unchanged: */
  /* n->type = ALT; */
  /* n->ngram = NULL; */
  /* n->offset = 0.0; */
  /* n->heap = NULL; */
  n->imax = -1;
  n->umax = 0.0;
  g_hash_table_remove_all(n->hash);
  n->nterms = 0;
  /* l indicates the start of non-bow term */
  for (int l = i; l <= k; l++) {
    fs_init_sum(&n->terms[n->nterms++], s, i, l, k);
  }
}

static Hpair fs_top_alt(FSnode n) {
  g_assert(n->type == ALT);

  /* For an alt node the top node is simply the top child with max logp. */
  /* umax should be set to the last popped logp, so not update here. */
  /* hash also contains the popped tokens, so no update here. */
  if (n->imax == -1) {
    gfloat pmax = SRILM_LOG0;
    for (int i = n->nterms - 1; i >= 0; i--) {
      FSnode ni = &n->terms[i];
      Hpair pi = fs_top_sum(ni);
      /* We do not want to return the same token twice. */
      while (hget(n->hash, pi.token)) {
	fs_pop_sum(ni);
	pi = fs_top_sum(ni);
      }
      if (pi.logp > pmax) {
	pmax = pi.logp;
	n->imax = i;
      }
    }
  }
  g_assert((n->imax >= 0) && (n->imax < n->nterms));
  FSnode ni = &n->terms[n->imax];
  Hpair pi = fs_top_sum(ni);
  return pi;
}

static Hpair fs_pop_alt(FSnode n) {
  if (n->imax == -1) {
    fs_top_alt(n);
  }
  FSnode ni = &n->terms[n->imax];
  Hpair pi = fs_pop_sum(ni);
  n->imax = -1;
  n->umax = pi.logp;
  hset(n->hash, pi.token);
  g_assert(pi.token != NULLTOKEN);
  return pi;
}

static void fs_alloc_sum(FSnode sum) {
  memset(sum, 0, sizeof(struct _FSnode));
  sum->type = SUM;
  sum->heap = minialloc(sizeof(Hpair)*(1 + lm1->nvocab));
  sum->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
  sum->terms = minialloc(lm1->order * sizeof(struct _FSnode));
  for (int k = 0; k < lm1->order; k++) {
    memset(&sum->terms[k], 0, sizeof(struct _FSnode));
  }
}

static void fs_init_sum(FSnode n, Sentence s, int i, int l, int k) {
  g_assert(n->type == SUM);
  /* Unchanged: */
  /* n->type = SUM; */
  /* n->ngram = NULL; */
  /* n->imax = 0; */
  n->nterms = 0;
  n->offset = 0.0;
  /* construct m..k-1 bow term */
  for (int m = i; m < l; m++) {
    if (fs_has_target(s, m, k-1)) {
      fs_init_logB(&n->terms[n->nterms++], s, m, k-1);
    } else {
      n->offset += fs_lookup_logB(s, m, k-1);
    }
  }
  /* construct logp term */
  if (fs_has_target(s, l, k)) {
    fs_init_logP(&n->terms[n->nterms++], s, l, k);
  } else {
    n->offset += fs_lookup_logP(s, l, k);
  }
  /* initialize umax, heap and hash if nterms > 1 */
  /* they don't get used if nterms == 1 */
  if (n->nterms > 1) {
    n->umax = n->offset;
    heap_size(n->heap) = 0;
    g_hash_table_remove_all(n->hash);
  }
}

static Hpair fs_top_sum1(FSnode n) {
  g_assert(n->type == SUM);
  g_assert(n->nterms == 1);
  /* A sum with a single term just passes the top of its only child */
  Hpair pi = fs_top_logp(&n->terms[0]);
  pi.logp += n->offset;
  return pi;
}

static Hpair fs_pop_sum1(FSnode n) {
  g_assert(n->type == SUM);
  g_assert(n->nterms == 1);
  /* A sum with a single term just passes the top of its only child */
  Hpair pi = fs_pop_logp(&n->terms[0]);
  pi.logp += n->offset;
  return pi;
}

static Hpair fs_pop_sum(FSnode n) {
  g_assert(n->type == SUM);
  if (n->nterms == 1) return fs_pop_sum1(n);
  /* if heap not ready call fs_top() */
  if ((heap_size(n->heap) == 0) || (heap_top(n->heap).logp < n->umax)) {
    fs_top_sum(n);
  }
  return heap_delete_max(n->heap);
}

static Hpair fs_top_sum(FSnode n) {
  g_assert(n->type == SUM);
  if (n->nterms == 1) return fs_top_sum1(n);
  static Ngram ng1;
  if (ng1 == NULL) {
    ng1 = minialloc(sizeof(Token) * (1 + lm1->order));
  }

  /* n->umax should be equal to the sum of children ni->umax (which
     itself is equal to the logp of last popped item from ni) plus the
     offset, therefore an upper bound on how high an unchecked word's
     logp can be.  If top(n).logp is equal or higher than umax no
     unchecked word can surpass it. */

  while ((heap_size(n->heap) == 0) || (heap_top(n->heap).logp < n->umax)) {

    /* find the next word to lookup. ideally this should be the one
       whose deletion will decrease umax the most.  */
    gfloat dmax = 0;
    int imax = 0;
    for (int i = n->nterms - 1; i >= 0; i--) {
      FSnode ni = &n->terms[i];
      Hpair pi = fs_top_logp(ni);
      gfloat diff = ni->umax - pi.logp;
      if (diff > dmax) {
	dmax = diff;
	imax = i;
      }
    }

    /* update umax (need even if pi.token in hash) */
    FSnode ni = &n->terms[imax];
    gfloat ni_umax = ni->umax;	/* save old ni->umax */
    Hpair pi = fs_pop_logp(ni);	/* this should update ni.umax <- pi.logp */
    g_assert(ni->umax == pi.logp);
    g_assert(ni->umax <= ni_umax);
    n->umax = n->umax - ni_umax + ni->umax;

    /* compute logp and insert into heap if not already in hash */
    if (hget(n->hash, pi.token) == NULL) {
      hset(n->hash, pi.token);
      pi.logp = n->offset;
      for (int i = n->nterms - 1; i >= 0; i--) {
	FSnode ni = &n->terms[i];
	Ngram ng = ni->ngram;
	fs_ngram_sub(ng1, ng, pi.token);
	pi.logp += ((ni->type == LOGP) ? lm_logP(lm1, ng1) : lm_logB(lm1, ng1));
      }      
      heap_insert_max(n->heap, pi.token, pi.logp);
    }
  }

  return heap_top(n->heap);
}

static void fs_ngram_sub(Ngram ng1, Ngram ng, Token tok) {
  int replaced = 0;
  ngram_size(ng1) = ngram_size(ng);
  for (int i = 1; i <= ngram_size(ng); i++) {
    if (ng[i] == NULLTOKEN) {
      ng1[i] = tok;
      replaced++;
    } else {
      ng1[i] = ng[i];
    }
  }
  g_assert(replaced == 1);
}

static void fs_init_logX(FSnode n, Sentence s, int i, int k, gboolean bow) {
  /* Unchanged: */
  /* n->hash = NULL; */
  /* n->terms = NULL; */
  /* n->nterms = 0; */
  /* n->offset = 0.0; */
  n->type = (bow ? LOGB : LOGP);
  n->umax = 0.0;
  n->imax = 1;
  n->ngram = NULL;
  n->heap = NULL;
  Token s_i_1 = s[i-1];
  s[i-1] = k-i+1;
  g_hash_table_lookup_extended((bow ? lmheap1->logB_heap : lmheap1->logP_heap),
			       &s[i-1], (gpointer*) &n->ngram, (gpointer*) &n->heap);
  s[i-1] = s_i_1;
}

static Hpair fs_pop_logp(FSnode n) {
  Hpair pi = fs_top_logp(n);
  n->imax++;
  n->umax = pi.logp;
  return pi;
}

static Hpair fs_top_logp(FSnode n) {
  return (((n->heap == NULL) || (n->imax > heap_size(n->heap))) ? NULLPAIR : n->heap[n->imax]);
}

static gfloat fs_lookup(Sentence s, int i, int k, gboolean bow) {
  Token s_i_1 = s[i-1];
  s[i-1] = k-i+1;
  gfloat logP = (bow ? lm_logB(lm1, &s[i-1]) : lm_logP(lm1, &s[i-1]));
  s[i-1] = s_i_1;
  return logP;
}

static gboolean fs_has_target(Sentence s, int i, int k) {
  for (int j = i; j <= k; j++) {
    if (s[j] == NULLTOKEN) return TRUE;
  }
  return FALSE;
}

/* debug fn */
static void fs_print_node(gpointer data, gpointer user_data) {
  FSnode node = (FSnode) data;
  if (node->type != ROOT) printf(" ");
  printf("(");
  switch(node->type) {
  case ROOT: printf("ROOT"); break;
  case ALT: printf("ALT"); break;
  case SUM: printf("SUM"); break;
  case LOGP: printf("LOGP"); break;
  case LOGB: printf("LOGB"); break;
  }
  if (node->ngram != NULL) {
    printf(" \"");
    for (int i = 1; i <= ngram_size(node->ngram); i++) {
      if (i > 1) printf(" ");
      if (node->ngram[i] == NULLTOKEN) printf("_");
      else printf(token_to_string(node->ngram[i]));
    }
    printf("\"");
  }
  if (node->offset != 0) {
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
