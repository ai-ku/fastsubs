#include <stdio.h>
#include <unistd.h>
#include <omp.h>
#include <math.h>
#include <string.h>
#include "fastsubs.h"
#include "heap.h"
#include "lm.h"
#include "dlib.h"


#define BUF  (1<<16)		/* max input line length */
#define SMAX (1<<16)		/* max tokens in a sentence */
#define PMAX 1.0		/* default value for -p */
#define NMAX UINT32_MAX		/* default value for -n */
#define BATCHSIZE 1000		/* number of sentences to process at a time */

const char tab_delimiter[2] = "\t";
const char eol_delimiter[2] = "\n";

typedef struct sent_s {
  int target_index;		// index of target word for which substitute are to be generated
  str_t sent_record_buf;	// string for the sentence record - used in single target per sentence mode
  str_t sstr;			// string for the sentence
  str_t *wstr;			// wstr[i] is the string for i'th word (pointer into sstr)
  Sentence sent;		// token array for the sentence
  Heap *subs;			// subs[i] is the sub/score array for i'th word
} *sent_t;

static int sent_read(struct sent_s *sarray, size_t smax, int one_target_per_sentence) {
  char buf[BUF];
  int index;

  size_t nsent = 0;
  for (nsent = 0; nsent < smax; nsent++) {
    if (fgets(buf, BUF, stdin) == NULL) break;
    if (one_target_per_sentence == 1) {
    	sarray[nsent].sent_record_buf = strdup(buf);
    	char* target = strtok(buf, tab_delimiter);
    	char* id = strtok(NULL, tab_delimiter);
    	char* index_str = strtok(NULL, tab_delimiter);
    	char* text = strtok(NULL, eol_delimiter);
    	index = atoi(index_str)+2;
        sarray[nsent].sstr = strdup(text);
        sarray[nsent].target_index = index;
    } else {
    	sarray[nsent].sent_record_buf = NULL;
    	sarray[nsent].sstr = strdup(buf);
    	sarray[nsent].target_index = -1;
    }

  }  
  return nsent;
}

static void sent_free(sent_t s) {
  int nword = sentence_size(s->sent);
  for (int i = 2; i <= nword; i++) {
	if ((s->target_index ==-1) || (s->target_index == i)) {
	  _d_free(s->subs[i]);
	}
  }
  _d_free(s->subs);
  _d_free(s->wstr);
  _d_free(s->sent);
  free(s->sstr);
  if (s->sent_record_buf != NULL)
	  free(s->sent_record_buf);
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
    if ((s->target_index == -1) || (s->target_index == i)) {
    	s->subs[i] = _d_malloc((1 + opt_n) * sizeof(Hpair));
    }
  }
}

static void sent_process(sent_t s, LM lm, u32 opt_n, double opt_p) {
  sent_alloc(s, opt_n);
  u32 ssize = sentence_size(s->sent);
  for (int i = 2; i <= ssize; i++) {
	  // TODO: fastsubs should treat the subs array as a heap and put the size in [0]
	  if ((s->target_index == -1) || (s->target_index == i)) {
		  heap_size(s->subs[i]) = fastsubs(&(s->subs[i][1]), s->sent, i, lm, opt_p, opt_n);
	  }
  }
}

static void subs_write(Heap subs, int normalize_prob) {
	u32 nsubs = heap_size(subs);

	if (normalize_prob == 1) {
		double norm = 0.0;
		for (int j = 1; j <= nsubs; j++) {
			norm += pow(10, subs[j].logp);
		}
		for (int j = 1; j <= nsubs; j++) {
			double prob = pow(10, subs[j].logp)/norm;
			if (prob < 0.00000001)
				break;
			if (j>1) putchar('\t');
			printf("%s %.8f", token_to_string(subs[j].token), prob);

		}
	} else {
		for (int j = 1; j <= nsubs; j++) {
			if (j>1) putchar('\t');
			printf("%s %.8f", token_to_string(subs[j].token), subs[j].logp);
		}
	}
}

static void sent_write(sent_t s, int normalize_prob) {

	if (s->target_index == -1) {
		u32 ssize = sentence_size(s->sent);
		for (u32 i = 2; i <= ssize; i++) {
			fputs(s->wstr[i], stdout);
			putchar('\t');
			Heap subs = s->subs[i];
			subs_write(subs, normalize_prob);
			putchar('\n');
		}
	} else { // one target per sentence
		printf("%s", s->sent_record_buf);
		Heap subs = s->subs[s->target_index];
		subs_write(subs, normalize_prob);
		putchar('\n');
	}
}

int main(int argc, char **argv) {
  const char *usage = "Usage: fastsubs-omp [-n <n> | -p <p> | -m <max-threads> | -t | -z ] model.lm[.gz] < input.txt\n";
  int opt;
  uint32_t opt_n = NMAX;
  double opt_p = PMAX;
  int opt_max_threads = -1;
  int one_target_per_sentence = 0;
  int normalize_prob = 0;
  while ((opt = getopt(argc, argv, "p:n:m:tz")) != -1) {
    switch(opt) {
    case 'n':
      opt_n = atoi(optarg);
      break;
    case 'p':
      opt_p = atof(optarg);
      break;
    case 'm':
      opt_max_threads = atof(optarg);
      break;
    case 't':
      one_target_per_sentence = 1;
      break;
    case 'z':
      normalize_prob = 1;
      break;
    break;
    default:
      die("%s", usage);
    }
  }
  if (optind >= argc)
    die("%s", usage);

  if (opt_max_threads > 0) {
	  omp_set_num_threads(opt_max_threads);
	  msg("Maximum threads to be used: %d", opt_max_threads);
  }

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
    int nsent = sent_read(s, BATCHSIZE, one_target_per_sentence);
    if (nsent == 0) break;
#pragma omp parallel for
    for (int i = 0; i < nsent; i++) {
      sent_process(&s[i], lm, opt_n, opt_p);
    }
    for (int i = 0; i < nsent; i++) {
      sent_write(&s[i], normalize_prob);
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

