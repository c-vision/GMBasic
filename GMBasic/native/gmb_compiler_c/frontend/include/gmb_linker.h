/* native/gmb_runtime_c/include/gmb_linker.h — linker BASIC (Fase E).
 * Risolve i simboli dell'AST: raccoglie funzioni e variabili dichiarate (DIM, parametri,
 * variabili di FOR), segnala riferimenti non dichiarati e definizioni duplicate.
 */
#ifndef GMB_LINKER_H
#define GMB_LINKER_H

#include "gmb_ast.h"
#include <stdio.h>

#define GMB_LINK_MAX_SYMS 4096

typedef enum { GMB_SYM_FUNCTION, GMB_SYM_VARIABLE } gmb_sym_kind_t;

typedef struct gmb_symbol {
    char* name;
    gmb_sym_kind_t kind;
    int decl_line;
    bool used;
    bool implicit;           /* aggiunto via uso, in attesa di dichiarazione (forward ref) */
} gmb_symbol_t;

typedef struct gmb_link_result {
    gmb_symbol_t syms[GMB_LINK_MAX_SYMS];
    int count;
    char* error;             /* primo errore (undefined/duplicate) */
    int error_line;
    int undefined_count;
    int duplicate_count;
} gmb_link_result_t;

/* Analizza l'AST: ritorna la tabella dei simboli + eventuale primo errore. */
void gmb_link_resolve(gmb_link_result_t* r, gmb_stmt_t** stmts, int n);

void gmb_link_free(gmb_link_result_t* r);

/* dump dell'AST (per il comando AST) */
void gmb_ast_dump(gmb_stmt_t** stmts, int n, FILE* out);

#endif
