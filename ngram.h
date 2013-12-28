#ifndef __NGRAM_H__
#define __NGRAM_H__
#include "token.h"

// todo:
// change hash ret to size_t after getting rid of glib
// try non-random hash for ngrams like fnv1a
// try regular alloc instead of dalloc

/** Ngrams are represented as Token arrays with zeroth element
 * representing count.  The actual tokens are in positions 1..n.
 */

typedef Token *Ngram;

#define ngram_size(p) ((p)[0])

unsigned int ngram_hash(const Ngram p); // change this to 64 bits
int ngram_equal(const Ngram pa, const Ngram pb); // change this to bool?
Ngram ngram_from_string(char *str);
Ngram ngram_dup(Ngram ng);
Ngram ngram_cpy(Ngram ngcopy, Ngram ng);
#endif
