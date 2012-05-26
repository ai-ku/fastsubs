#ifndef __FASTSUBS_H__
#define __FASTSUBS_H__
#include <glib.h>
#include "lm.h"
#include "sentence.h"
#include "heap.h"

/**Typedefs*/
typedef enum {
  ROOT,		  /* root level node, representing e.g. abxcd -> abx + bxc + xcd */
  ALT,            /* alternatives e.g. abx -> abx | [ab]bx | [ab][b]x; where [] represents bow */
  LOGP		  /* leaf node e.g. [bx] or abx */
} NodeType;

typedef struct _FSnode {
  NodeType type;
  Ngram ngram;
  struct _FSnode *terms;
  int nterms;
  gfloat offset;
  Hpair *heap;
  GHashTable *hash;
  gfloat umax;
  int imax;
} *FSnode;

/** fastsubs(): Fills the pre-allocated subs array (should have nvocab
 *  elements to be safe) with substitutes of word j in sentence s
 *  according to LMheap h.  The substitutes are sorted by descending
 *  probability and the Hpair structure pairs them with their
 *  (unnormalized) log10_probability.  The number of substitutes
 *  found is controlled by plimit which lower bounds the total
 *  (normalized) probability of the returned subs, or nlimit which
 *  lower bounds the number of returned subs.  Both limits have to be
 *  satisfied, so just pass 0 for a limit you do not care about.  The
 *  number of entries placed in the subs array is returned.
 */
int fastsubs(Hpair *subs, Sentence s, int j, LM lm, gdouble plimit, guint nlimit);

extern guint fs_niter;		/* number of pops for performance analysis */

#endif
