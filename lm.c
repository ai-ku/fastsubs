#include "foreach.h"
#include "lm.h"

struct _LMS {
  GHashTable *logP;
  GHashTable *logB;
  guint order;
};

static LM lm_alloc() {
  LM lm = g_new(struct _LMS, 1);
  lm->logP = g_hash_table_new_full(ngram_hash_function, ngram_equal, ngram_free, NULL);
  lm->logB = g_hash_table_new_full(ngram_hash_function, ngram_equal, NULL, NULL);
  lm->order = 0;
  return lm;
}

void lm_free(LM lm) {
  if (lm != NULL) {
    if (lm->logP != NULL) g_hash_table_destroy(lm->logP);
    if (lm->logB != NULL) g_hash_table_destroy(lm->logB);
    g_free(lm);
  }
}

/* These macros only work if double and pointer are the same size */
#define GPOINTER_TO_DOUBLE(p)	(*((gdouble*)(&(p))))
#define GDOUBLE_TO_POINTER(s)	(*((gpointer*)(&(s))))

LM lm_init(char *lmfile) {
  g_assert(sizeof(gdouble) == sizeof(gpointer));
  LM lm = lm_alloc();
  foreach_line(str, lmfile) {
    if (*str == '\n' || *str == '\\' || *str == 'n') continue;
    char *s = strtok(str, "\t");
    errno = 0;
    gdouble logp = g_ascii_strtod(s, NULL);
    if(errno != 0) fprintf(stderr, s);
    g_assert(errno == 0);
    s = strtok(NULL, "\t\n");
    Ngram ng = ngram_from_string(s);
    if (ngram_size(ng) > lm->order) lm->order = ngram_size(ng);
    if (g_hash_table_lookup_extended(lm->logP, ng, NULL, NULL)) 
      g_error("Duplicate ngram");
    g_hash_table_insert(lm->logP, ng, GDOUBLE_TO_POINTER(logp));
    s = strtok(NULL, "\n");
    if (s != NULL) {
      gdouble logb = g_ascii_strtod(s, NULL);
      if(errno != 0) fprintf(stderr, s);
      g_assert(errno == 0);
      g_hash_table_insert(lm->logB, ng, GDOUBLE_TO_POINTER(logb));
    }
  }
  return lm;
}

guint lm_ngram_order(LM lm) {
  return lm->order;
}

gdouble lm_logP(LM lm, Ngram ng) {
  gpointer p = g_hash_table_lookup(lm->logP, ng);
  return (p == NULL ? SRILM_LOG0 : GPOINTER_TO_DOUBLE(p));
}

gdouble lm_logB(LM lm, Ngram ng) {
  gpointer p = g_hash_table_lookup(lm->logB, ng);
  return (p == NULL ? SRILM_LOG0 : GPOINTER_TO_DOUBLE(p));
}
