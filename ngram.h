#ifndef __NGRAM_H__
#define __NGRAM_H__
#include <glib.h>
#include "token.h"

/** Ngrams are represented as Token arrays with zeroth element
 * representing count.  The actual tokens are in positions 1..n.
 */

typedef Token *Ngram;

#define ngram_size(p) ((guint32)(*p))
#define ngram_copy(p) g_memdup(p,(ngram_size(p)+1)*sizeof(Token))
#define ngram_free g_free

guint ngram_hash_function(gconstpointer p);
gboolean ngram_equal(gconstpointer pa, gconstpointer pb);
Ngram ngram_from_string(char *str);

#endif
