#ifndef __NGRAM_H__
#define __NGRAM_H__
#include "token.h"

/** Ngrams are represented as Token arrays with zeroth element
 * representing count.  The actual tokens are in positions 1..n.
 */

typedef Token *Ngram;

#define ngram_size(p) ((p)[0])

uint64_t ngram_hash(const Ngram p);
bool ngram_equal(const Ngram pa, const Ngram pb);
Ngram ngram_from_string(char *str);
Ngram ngram_dup(Ngram ng);
Ngram ngram_cpy(Ngram ngcopy, Ngram ng);
#endif
