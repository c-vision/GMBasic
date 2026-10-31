/* gmb_rewrite.h — Passo di REWRITE del linker C (pari a `Linker.RewriteTarget`/`nExpression` del C#):
 * arricchisce l'AST risolvendo, per ogni espressione/variabile/chiamata, il SIMBOLO (SymbolId) e il
 * TIPO RISOLTO (ResolvedType). È il prodotto principale del rewrite C# ("arricchendolo con il suo
 * tipo risolto e risolvendo i SymbolId") e ciò che serve al codegen. COMPILATORE C A SÉ STANTE. */
#ifndef GMB_REWRITE_H
#define GMB_REWRITE_H

#include "gmb_ast.h"
#include "gmb_symtab.h"
#include "gmb_types.h"

/* Riscrive l'AST in-place: per ogni gmb_expr_t di tipo VAR/CALL imposta `resolved_sym` (id del
 * simbolo, -1 se non risolto) e `resolved_type` (tipo risolto, "" se ignoto) usando symtab+typereg.
 * Ritorna il numero di espressioni arricchite. */
int gmb_rewrite_program(gmb_stmt_t** stmts, int n, const gmb_symtab_t* st, const gmb_typereg_t* tr);

#endif /* GMB_REWRITE_H */
