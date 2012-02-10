#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include "procinfo.h"
#include "fastsubs.h"
#include "heap.h"
#include "minialloc.h"
const char *usage = "Usage: fastsubs [-n <n> | -p <p>] model.lm < input.txt";

int main(int argc, char **argv) {
  g_message_init();
  char buf[1024];
  Token s[1025];

  int opt;
  int opt_n = 0;
  gdouble opt_p = 0.0;
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
  if ((opt_n == 0) && (opt_p == 0))
      g_error(usage);
  g_message("Minimum substitutes -n=%d, minimum probability -p=%g", opt_n, opt_p);
  g_message("Loading model file %s", argv[optind]);
  LM lm = lm_init(argv[optind]);
  Hpair *subs = minialloc(lm->nvocab * sizeof(Hpair));
  g_message("ngram order = %d\n==> Enter sentences:\n", lm->order);
  while(fgets(buf, 1024, stdin)) {
    int n = sentence_from_string(s, buf, 1024);
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
