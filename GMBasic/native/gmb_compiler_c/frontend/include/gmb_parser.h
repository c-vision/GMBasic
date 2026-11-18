/* native/gmb_runtime_c/include/gmb_parser.h — parser BASIC (Fase D). */
#ifndef GMB_PARSER_H
#define GMB_PARSER_H

#include "gmb_ast.h"
#include "gmb_lexer.h"

typedef struct {
    gmb_stmt_t** stmts;
    int n, cap;
    /* LIBRARY "nome": nome della libreria condivisa da produrre ("" = eseguibile). */
    char library_name[96];
    /* OPTION STRICT ON|OFF (direttiva di modulo): con ON ogni nome usato deve essere
     * dichiarato (nessuna variabile implicita) — parità con il linker C#. Vedi
     * Guide/Variables-Scope-Spec.md §2.1/§3.1 e il capitolo manuale II.5. */
    int option_strict;
} gmb_program_t;

/* Parsa i token in un programma (AST). error != NULL su errore di parse. */
gmb_program_t gmb_parse(const gmb_lex_result_t* lx, char** error);
void gmb_program_free(gmb_program_t* prog);

#endif
