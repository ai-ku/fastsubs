#include <stdio.h>
#include <unistd.h>
#include "fastsubs.h"
#include "heap.h"
#include "dlib.h"

#define BUF  (1<<16)		/* max input line length */
#define SMAX (1<<16)		/* max tokens in a sentence */
#define PMAX 1.0		/* default value for -p */
#define NMAX UINT32_MAX		/* default value for -n */


int main(int argc, char **argv) {
  const char *usage = "Usage: fastsubs [-s seed] [-n <n> | -p <p>] model.lm[.gz] < input.txt\n";
  char buf[BUF];
  Token s[SMAX+1];
  char *w[SMAX+1];

  int opt;
  uint32_t opt_n = NMAX;
  double opt_p = PMAX;
  while ((opt = getopt(argc, argv, "p:n:s:")) != -1) {
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

  msg("Get substitutes until count=%d OR probability=%g", opt_n, opt_p);
  msg("Loading model file %s", argv[optind]);
  LM lm = lm_init(argv[optind]);
  msg("vocab:%d", lm->nvocab);
  if (lm->nvocab < opt_n){
       opt_n = lm->nvocab - 1;
       msg("[Number of substitutes > vocabulary size] set to maximum substitute number=%d", opt_n - 1);
  }
  Hpair *subs = dalloc(lm->nvocab * sizeof(Hpair));
  msg("ngram order = %d\n==> Enter sentences:\n", lm->order);
  int fs_ncall = 0;
  int fs_nsubs = 0;
  while(fgets(buf, BUF, stdin)) {
    int n = sentence_from_string(s, buf, SMAX, w);
    for (int i = 2; i <= n; i++) {
      int nsubs = fastsubs(subs, s, i, lm, opt_p, opt_n);
      fs_ncall++; fs_nsubs += nsubs;
      fputs(w[i], stdout);
      for (int j = 0; j < nsubs; j++) {
	printf("\t%s %.8f", token_to_string(subs[j].token), subs[j].logp);
      }
      printf("\n");
    }
  }
  msg("free lmheap...");
  fastsubs_free();
  msg("free lm...");
  lm_free(lm);
  msg("free symtable...");
  symtable_free();
  msg("free dalloc space...");
  dfreeall();
  msg("calls=%d subs/call=%g pops/call=%g", 
      fs_ncall, (double)fs_nsubs/fs_ncall, (double)fs_niter/fs_ncall);
}
