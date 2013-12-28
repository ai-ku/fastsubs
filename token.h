#ifndef __TOKEN_H__
#define __TOKEN_H__
#include "dlib.h"

/** Tokens are represented as guint32 integers (sym_t).  A sym_t (a
 *  symbol type, modeled after glib GQuark and Lisp symbols) is a
 *  non-zero integer which uniquely identifies a particular string. A
 *  sym_t value of zero is associated to NULL.  The data type sym_t
 *  and operations str2sym and sym2str are defined in dlib.h.
 */

typedef sym_t Token;
#define NULLTOKEN 0
#define token_from_string(s) str2sym(s,true)
#define token_try_string(s) str2sym(s,false)
#define token_to_string(u) sym2str(u)

#endif
