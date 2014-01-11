#include <stdio.h>
#include <unistd.h>
#include <omp.h>
#include "fastsubs.h"
#include "heap.h"
#include "lm.h"
#include "dlib.h"

#define BUF  (1<<16)		/* max input line length */
#define SMAX (1<<16)		/* max tokens in a sentence */
#define PMAX 1.0		/* default value for -p */
#define NMAX UINT32_MAX		/* default value for -n */
#define BATCHSIZE 1000		/* number of sentences to process at a time */

typedef struct sent_s {
  str_t sstr;			// string for the sentence
  str_t *wstr;			// wstr[i] is the string for i'th word (pointer into sstr)
  Sentence sent;		// token array for the sentence
  Heap *subs;			// subs[i] is the sub/score array for i'th word
} *sent_t;

static int sent_read(struct sent_s *sarray, size_t smax) {
  char buf[BUF];
  size_t nsent = 0;
  for (nsent = 0; nsent < smax; nsent++) {
    if (fgets(buf, BUF, stdin) == NULL) break;
    sarray[nsent].sstr = strdup(buf);
  }  
  return nsent;
}

static void sent_free(sent_t s) {
  int nword = sentence_size(s->sent);
  for (int i = 2; i <= nword; i++) {
    _d_free(s->subs[i]);
  }
  _d_free(s->subs);
  _d_free(s->wstr);
  _d_free(s->sent);
  free(s->sstr);
}

static void sent_alloc(sent_t s, u32 opt_n) {
  Token tok[SMAX+1];
  str_t str[SMAX+1];
  int nword_1 = 1 + sentence_from_string(tok, s->sstr, SMAX, str);
  s->sent = (Sentence) _d_malloc(nword_1 * sizeof(Token));
  s->wstr = (str_t *) _d_malloc(nword_1 * sizeof(str_t));
  s->subs = (Heap *) _d_malloc(nword_1 * sizeof(Heap));
  s->sent[0] = tok[0];		// sentence length
  s->sent[1] = tok[1];		// SOS token
  for (int i = 2; i < nword_1; i++) {
    s->sent[i] = tok[i];
    s->wstr[i] = str[i];
    s->subs[i] = _d_malloc((1 + opt_n) * sizeof(Hpair));
  }
}

static void sent_process(sent_t s, LM lm, u32 opt_n, double opt_p) {
  sent_alloc(s, opt_n);
  u32 ssize = sentence_size(s->sent);
  for (int i = 2; i <= ssize; i++) {
    // TODO: fastsubs should treat the subs array as a heap and put the size in [0]
    heap_size(s->subs[i]) = fastsubs(&(s->subs[i][1]), s->sent, i, lm, opt_p, opt_n);
  }
}

static void sent_write(sent_t s) {
  u32 ssize = sentence_size(s->sent);
  for (u32 i = 2; i <= ssize; i++) {
    fputs(s->wstr[i], stdout);
    Heap subs = s->subs[i];
    u32 nsubs = heap_size(subs);
    for (u32 j = 1; j <= nsubs; j++) {
      // TODO: why is this .8f?
      printf("\t%s %.8f", token_to_string(subs[j].token), subs[j].logp);
    }
    putchar('\n');
  }
}

int main(int argc, char **argv) {
  const char *usage = "Usage: fastsubs-omp [-n <n> | -p <p>] model.lm[.gz] < input.txt\n";
  int opt;
  uint32_t opt_n = NMAX;
  double opt_p = PMAX;
  while ((opt = getopt(argc, argv, "p:n:")) != -1) {
    switch(opt) {
    case 'n':
      opt_n = atoi(optarg);
      break;
    case 'p':
      opt_p = atof(optarg);
      break;
    default:
      die("%s", usage);
    }
  }
  if (optind >= argc)
    die("%s", usage);

  msg("Loading model file %s", argv[optind]);
  LM lm = lm_init(argv[optind]);
  uint32_t nvocab = lm_nvocab(lm);
  msg("nvocab:%d order:%d", nvocab, lm_order(lm));
  if (nvocab < opt_n){
    opt_n = nvocab - 1;
  }
  msg("Get substitutes until count=%d OR probability=%g", opt_n, opt_p);
  struct sent_s *s = (struct sent_s *) _d_malloc(BATCHSIZE * sizeof(struct sent_s));
  msg("Allocated sentence structs");
  while (1) {
    int nsent = sent_read(s, BATCHSIZE);
    if (nsent == 0) break;
#pragma omp parallel for
    for (int i = 0; i < nsent; i++) {
      sent_process(&s[i], lm, opt_n, opt_p);
    }
    for (int i = 0; i < nsent; i++) {
      sent_write(&s[i]);
    }
#pragma omp parallel for
    for (int i = 0; i < nsent; i++) {
      sent_free(&s[i]);
    }
    fputc('.', stderr);
  }
  fputc('\n', stderr);
  msg("Freeing sentence structs.");
  _d_free(s);
  msg("Freeing lm");
  lm_free(lm);
  msg("Freeing tmp space");
  dfreeall();
  msg("Freeing symtable");
  symtable_free();
  msg("done");
}

