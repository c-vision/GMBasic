/* native/gmb_runtime_c/include/gmb_macro_expand.h — espansione macro (MacroExpander). */
#ifndef GMB_MACRO_EXPAND_H
#define GMB_MACRO_EXPAND_H

#include "gmb_lexer.h"

/* Espande le macro #DEF sul flusso di token. Ritorna un nuovo flusso (il chiamante
 * lo libera con gmb_lex_result_free). owned_src viene preservato. */
gmb_lex_result_t gmb_macro_expand(const gmb_lex_result_t* lx);

#endif
