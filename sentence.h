#ifndef __SENTENCE_H__
#define __SENTENCE_H__
#include "token.h"

/** Sentences are represented as Token arrays.  The zeroth element is
 * token count.  The first and last elements are SOS (<s>) and EOS
 * (</s>) (added by this program, not necessary in the input).  The
 * token count includes the <s> and </s> tags.
 */
typedef Token *Sentence;

#define sentence_size(s) ((uint32_t)(*s))

#define SOSTAG "<s>"
#define EOSTAG "</s>"
#define UNKTAG "<unk>"

extern Token SOS, EOS, UNK;

/** sentence_from_string(): The st array should be pre-allocated to
 * hold nmax+1 elements.  st[0] holds the word count, st[1]=SOS,
 * st[n]=EOS, and st[2..(n-1)] are the words.  SOS and EOS are added
 * by sentence_from_string, if they exist in the original input those
 * are ignored.  Words that are not already defined are replaced with
 * <unk>.  Since we use a single hash for the vocabulary this only
 * works if there is a single lm loaded before the first call to
 * sentence_from_string.  The last argument (if not NULL) is for
 * storing the original strings for the words including SOS and EOS.
 * Returns number of tokens including SOS and EOS (also stored in
 * st[0]).
 */
uint32_t sentence_from_string(Sentence st, char *str, int nmax, char **w);

#endif
