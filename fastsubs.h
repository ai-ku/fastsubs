#ifndef __FASTSUBS_H__
#define __FASTSUBS_H__
#include "lm.h"
#include "sentence.h"
#include "heap.h"
typedef uint32_t u32;
typedef uint64_t u64;

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
u32 fastsubs(Hpair *subs, Sentence s, u32 target, LM lm, double plimit, u32 nlimit);

extern uint64_t fs_niter; /* number of pops for performance analysis */

#endif
