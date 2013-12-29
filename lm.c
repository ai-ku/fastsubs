#include <stdlib.h>
#include <assert.h>
#include "dlib.h"
#include "lm.h"

/* Define ngram-logp hash */
#define _lp_init(ng) ((struct NF_s) { (ng), 0 }) // ng must be already allocated
D_HASH(_lp, struct NF_s, Ngram, ngram_equal, ngram_hash, d_keyof, _lp_init, d_keyisnull, d_keymknull)
#define _logP(lm, ng) (_lpget(((lm)->logP),(ng),1)->val)
#define _logB(lm, ng) (_lpget(((lm)->logB),(ng),1)->val)

static LM lm1;

LM lm_init(char *lmfile) {
  LM lm = NULL;
  if (lm1 == NULL) {
    lm1 = _d_malloc(sizeof(struct _LMS));
    lm = lm1;
    lm->logP = _lpnew(0);
    lm->logB = _lpnew(0);
    lm->order = 0;
    lm->nvocab = 0;
  } else {
    die("Only one LM is allowed."); // why?
  }
  size_t tok_cnt = 0;
  forline(str, lmfile) {
    errno = 0;
    if (*str == '\n' || *str == '\\' || *str == 'n') continue;
    char *tok[] = { NULL, NULL, NULL };
    size_t ntok = split(str, '\t', tok, 3);
    assert(ntok >= 2);
    float f = strtof(tok[0], NULL);
    assert(errno == 0);
    if (ntok == 2) {
      size_t len = strlen(tok[1]);
      if (tok[1][len-1] == '\n') tok[1][len-1] = '\0';
    }
    Ngram ng = ngram_from_string(tok[1]);
    tok_cnt += ngram_size(ng);
    if (ngram_size(ng) > lm->order) 
      lm->order = ngram_size(ng);
    for (int i = ngram_size(ng); i > 0; i--) {
      if (ng[i] > lm->nvocab) lm->nvocab = ng[i];
    }
    _logP(lm, ng) = f;
    if (ntok == 3) {
      f = (float) strtof(tok[2], NULL);
      assert(errno == 0);
      _logB(lm, ng) = f;
    }
  }
  msg("lm_init done: logP=%zux(%zu/%zu) logB=%zux(%zu/%zu) toks=%zu", 
      sizeof(struct NF_s), len(lm->logP), cap(lm->logP), 
      sizeof(struct NF_s), len(lm->logB), cap(lm->logB), 
      tok_cnt);

  return lm;
}

void lm_free(LM lm) {
  _lpfree(lm->logP);
  _lpfree(lm->logB);
  _d_free(lm);
}

float lm_logP(LM lm, Ngram ng) {
  NF_t p = _lpget(lm->logP, ng, false);
  return (p == NULL ? SRILM_LOG0 : p->val);
}

float lm_logB(LM lm, Ngram ng) {
  NF_t p = _lpget(lm->logB, ng, false);
  return (p == NULL ? 0 : p->val);
}
