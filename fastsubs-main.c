#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include "procinfo.h"
#include "fastsubs.h"
#include "heap.h"
#include "minialloc.h"

#define BUF  (1<<16)		/* max input line length */
#define SMAX (1<<16)		/* max tokens in a sentence */
#define PMAX 1.0		/* default value for -p */
#define NMAX G_MAXUINT		/* default value for -n */


int main(int argc, char **argv) {
  const char *usage = "Usage: fastsubs [-n <n> | -p <p>] model.lm[.gz] < input.txt";
  g_message_init();
  char buf[BUF];
  Token s[SMAX+1];
  char *w[SMAX+1];

  int opt;
  guint opt_n = NMAX;
  gdouble opt_p = PMAX;
  while ((opt = getopt(argc, argv, "p:n:")) != -1) {
    switch(opt) {
    case 'n':
      opt_n = atoi(optarg);
      break;
    case 'p':
      opt_p = atof(optarg);
      break;
    default:
      g_error("%s", usage);
    }
  }
  if (optind >= argc)
    g_error("%s", usage);
  g_message("Get substitutes until count=%d OR probability=%g", opt_n, opt_p);
  g_message("Loading model file %s", argv[optind]);
  LM lm = lm_init(argv[optind]);
  Hpair *subs = minialloc(lm->nvocab * sizeof(Hpair));
  g_message("ngram order = %d\n==> Enter sentences:\n", lm->order);
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
  minialloc_free_all();
  g_message("calls=%d subs/call=%g pops/call=%g", 
	    fs_ncall, (double)fs_nsubs/fs_ncall, (double)fs_niter/fs_ncall);
}
