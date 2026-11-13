/* gmb_parser_stmt_embed.c — direttiva `EMBEDFILE` (equivalente di `@embedFile` di Zig).
 *
 *   EMBEDFILE "risorsa.dat" AS nome$        ' anche: AS nome
 *
 * Il contenuto del file diventa un LETTERALE STRINGA BINARIO: viene incorporato
 * nell'eseguibile/chunk, quindi il programma consegnato non ha bisogno del file
 * accanto. Nessun I/O a runtime.
 *
 * Desugaring (parità con il frontend C#, che fa lo stesso): due statement
 *   1. DIM nome AS STRING          -> scope esplicito (globale a livello di modulo,
 *                                     locale dentro una SUB, come ogni DIM)
 *   2. nome = <letterale binario>  -> il valore
 * Il primo è il valore di ritorno del parser, il secondo va in coda (`gmb_p_queue_stmt`).
 *
 * `EMBEDFILE` NON è una keyword del lexer: è un SOFT keyword riconosciuto solo se la
 * riga inizia con la parola e il token successivo è un letterale stringa, così una
 * variabile che si chiamasse `embedfile` continua a funzionare (stesso criterio di RUN).
 */
#include "gmb_parser_internal.h"
#include "gmb_embed.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Copia il testo del token in una stringa malloc. */
static char* tok_dup(const gmb_token_t* t)
{
    if (!t) return NULL;
    char* s = (char*)malloc((size_t)t->length + 1);
    if (!s) return NULL;
    memcpy(s, t->lexeme, (size_t)t->length);
    s[t->length] = 0;
    return s;
}

/* Valore testuale di un letterale stringa: toglie i delimitatori ("..." o `...`) e
 * risolve la sequenza "" -> " (stessa regola dell'expression parser). */
static char* lit_value(const gmb_token_t* t)
{
    if (!t || !t->lexeme || t->length < 2) return NULL;
    long n = t->length;
    const char* s = t->lexeme;
    if ((s[0] == '"' && s[n - 1] == '"') || (s[0] == '`' && s[n - 1] == '`')) { s++; n -= 2; }
    char* out = (char*)malloc((size_t)n + 1);
    if (!out) return NULL;
    long o = 0;
    for (long i = 0; i < n; i++) {
        if (s[i] == '"' && i + 1 < n && s[i + 1] == '"') { out[o++] = '"'; i++; }
        else out[o++] = s[i];
    }
    out[o] = 0;
    return out;
}

gmb_stmt_t* gmb_p_parse_embedfile(gmb_parser_t* p, int line)
{
    const gmb_token_t* pt = gmb_p_cur(p);
    if (!pt || pt->kind != GMB_TOK_StringLiteral) {
        p->error = strdup("EMBEDFILE: atteso il percorso tra virgolette, es. EMBEDFILE \"logo.txt\" AS logo$");
        return NULL;
    }
    char* path = lit_value(pt);
    gmb_p_adv(p);

    /* AS nome — AS è una keyword hard; si accetta anche la forma senza AS (come DIM). */
    if (gmb_p_check(p, GMB_TOK_As) || gmb_p_tok_is_keyword(gmb_p_cur(p), "As")) gmb_p_adv(p);

    const gmb_token_t* nt = gmb_p_cur(p);
    if (!nt || nt->kind == GMB_TOK_Newline || nt->kind == GMB_TOK_Colon || nt->kind == GMB_TOK_Eof
        || nt->kind == GMB_TOK_Equal) {
        p->error = strdup("EMBEDFILE: manca il nome della variabile (EMBEDFILE \"file\" AS nome$)");
        free(path);
        return NULL;
    }
    char* name = tok_dup(nt);
    gmb_p_adv(p);

    if (!path || !*path) { p->error = strdup("EMBEDFILE: percorso vuoto"); free(path); free(name); return NULL; }

    long blen = 0;
    char* err = NULL;
    char* data = gmb_embed_load(path, &blen, &err);
    if (!data) {
        size_t n = strlen(path) + 64 + (err ? strlen(err) : 0);
        char* m = (char*)malloc(n);
        if (m) {
            if (err) snprintf(m, n, "%s", err);
            else snprintf(m, n, "EMBEDFILE: file non leggibile: '%s'", path);
        }
        p->error = m ? m : strdup("EMBEDFILE: file non leggibile");
        free(err);
        free(path);
        free(name);
        return NULL;
    }
    free(err);
    free(path);

    /* Un solo statement: `nome = <letterale binario>`. Stesso desugaring del frontend C#
     * (nessun DIM: la variabile segue la regola delle assegnazioni implicite, identica
     * sulle 4 gambe — verificata da MiniTest/parity4). */
    gmb_stmt_t* asg = gmb_stmt_new(GMB_STMT_ASSIGN, line);
    asg->target = name;
    asg->value = gmb_expr_str_n(data, blen, line);
    free(data);
    return asg;
}
