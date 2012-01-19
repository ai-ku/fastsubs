#ifndef __NGRAM_H__
#define __NGRAM_H__
#include <glib.h>
#include "token.h"

/** Ngrams are represented as Token arrays with zeroth element
 * representing count.  The actual tokens are in positions 1..n.
 */

typedef Token *Ngram;

#define ngram_size(p) ((guint32)(*p))

guint ngram_hash(gconstpointer p);
gboolean ngram_equal(gconstpointer pa, gconstpointer pb);
Ngram ngram_from_string(char *str);
Ngram ngram_alloc(int n);
Ngram ngram_copy(Ngram ng);
#endif
