/* native/gmb_runtime_c/include/gmb_lexer.h — Lexer GMBASIC in C.
 * Tokenizza il sorgente in un array di gmb_token_t (kind/lexeme/line/col), con keyword
 * case-insensitive (tabella da common/Lexer/Lexer.Keywords.cs), numeri (dec/hex/bin/float),
 * stringhe "...", commenti (' e REM), newline e operatori/delimitatori.
 * Specchia il nucleo di common/Lexer/Lexer.cs (Fase B del C-Compiler-Autonomo-Plan).
 */
#ifndef GMB_LEXER_H
#define GMB_LEXER_H

#include "gmb_tokenkind.h"

typedef struct gmb_token {
    gmb_tokenkind_t kind;
    const char* lexeme;   /* punta dentro il source (non copiato) */
    int length;
    int line, col;
} gmb_token_t;

typedef struct gmb_lex_result {
    gmb_token_t* tokens;
    int count;
    int cap;
    const char* error;    /* != NULL se errore di tokenizzazione */
    char* owned_src;      /* sorgente posseduto (quando preprocessato): liberato in free */
} gmb_lex_result_t;

/* Tokenizza il sorgente. Il chiamante libera il risultato con gmb_lex_result_free. */
gmb_lex_result_t gmb_lexer_scan(const char* src, long len);

void gmb_lex_result_free(gmb_lex_result_t* r);

/* Token della keyword di PARSING per il suo lexeme (tabella keyword del lexer),
 * o -1 se non è una keyword. Usata per classificare i TIPI nel dispatch per-token. */
int gmb_keyword_tok(const char* word);

#endif
