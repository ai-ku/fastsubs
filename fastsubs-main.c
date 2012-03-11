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
  const char *usage = "Usage: fastsubs [-n <n> | -p <p>] model.lm < input.txt";
  g_message_init();
  char buf[BUF];
  Token s[SMAX+1];

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
      g_error(usage);
    }
  }
  if (optind >= argc)
      g_error(usage);
  g_message("Minimum substitutes -n=%d, or minimum probability -p=%g", opt_n, opt_p);
  g_message("Loading model file %s", argv[optind]);
  LM lm = lm_init(argv[optind]);
  Hpair *subs = minialloc(lm->nvocab * sizeof(Hpair));
  g_message("ngram order = %d\n==> Enter sentences:\n", lm->order);
  while(fgets(buf, BUF, stdin)) {
    int n = sentence_from_string(s, buf, SMAX);
    for (int i = 2; i <= n; i++) {
      int nsubs = fastsubs(subs, s, i, lm, opt_p, opt_n);
      printf("%s", token_to_string(s[i]));
      for (int j = 0; j < nsubs; j++) {
	printf("\t%s %.8f", token_to_string(subs[j].token), subs[j].logp);
      }
      printf("\n");
    }
  }
  minialloc_free_all();
}
