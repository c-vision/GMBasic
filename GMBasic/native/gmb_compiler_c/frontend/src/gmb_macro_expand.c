/* native/gmb_runtime_c/src/gmb_macro_expand.c — espansione macro (speculare a
 * Lexer.Macros.cs / MacroExpander). Opera sul flusso di token: riconosce le definizioni
 * `#DEF NOME(args) corpo...` (corpo fino a fine riga) e sostituisce le chiamate
 * `NOME(args)` con il corpo in cui i parametri sono rimpiazzati dagli argomenti.
 */
#include "gmb_macro_expand.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GMB_MACRO_MAX 128
#define GMB_MACRO_PARAMS 16
#define GMB_MACRO_BODY 512

typedef struct gmb_macro {
    char* name;
    char** params;
    int nparams;
    gmb_token_t body[GMB_MACRO_BODY];
    int nbody;
} gmb_macro_t;

static gmb_macro_t g_macros[GMB_MACRO_MAX];
static int g_nmacros = 0;

static gmb_macro_t* macro_find(const char* name)
{
    for (int i = 0; i < g_nmacros; i++)
        if (strcasecmp(g_macros[i].name, name) == 0) return &g_macros[i];
    return NULL;
}

static void macros_clear(void)
{
    for (int i = 0; i < g_nmacros; i++) {
        free(g_macros[i].name);
        for (int p = 0; p < g_macros[i].nparams; p++) free(g_macros[i].params[p]);
        free(g_macros[i].params);
    }
    g_nmacros = 0;
}

/* push un token nell'output */
static void out_push(gmb_lex_result_t* out, gmb_token_t t)
{
    if (out->count >= out->cap) {
        out->cap = out->cap ? out->cap * 2 : 256;
        out->tokens = (gmb_token_t*)realloc(out->tokens, (size_t)out->cap * sizeof(gmb_token_t));
    }
    out->tokens[out->count++] = t;
}

gmb_lex_result_t gmb_macro_expand(const gmb_lex_result_t* lx)
{
    gmb_lex_result_t out = {0};
    out.cap = lx->count + 16;
    out.tokens = (gmb_token_t*)malloc((size_t)out.cap * sizeof(gmb_token_t));
    out.owned_src = lx->owned_src;   /* il buffer sorgente è posseduto dall'input: NON liberarlo due volte */
    macros_clear();

    int i = 0;
    while (i < lx->count) {
        gmb_token_t t = lx->tokens[i];

        if (t.kind == GMB_TOK_Define && t.length >= 2 && t.lexeme[0] == '#') {
            /* definizione macro: #DEF NOME [ (params) ] corpo... fino a Newline
             * (solo direttive che iniziano con '#': la keyword DEFINE condivide il token) */
            i++;
            if (i >= lx->count || lx->tokens[i].length <= 0) { i++; continue; }
            gmb_macro_t m = {0};
            m.name = strndup(lx->tokens[i].lexeme, (size_t)lx->tokens[i].length);
            i++;
            if (i < lx->count && lx->tokens[i].kind == GMB_TOK_LeftParen) {
                i++;
                m.params = (char**)calloc(GMB_MACRO_PARAMS, sizeof(char*));
                while (i < lx->count && lx->tokens[i].kind != GMB_TOK_RightParen) {
                    if (lx->tokens[i].kind == GMB_TOK_Identifier && m.nparams < GMB_MACRO_PARAMS) {
                        m.params[m.nparams++] = strndup(lx->tokens[i].lexeme, (size_t)lx->tokens[i].length);
                    }
                    i++;
                    if (i < lx->count && lx->tokens[i].kind == GMB_TOK_Comma) i++;
                }
                if (i < lx->count) i++;   /* RightParen */
            }
            /* corpo fino a Newline/Eof */
            while (i < lx->count && lx->tokens[i].kind != GMB_TOK_Newline && lx->tokens[i].kind != GMB_TOK_Eof && m.nbody < GMB_MACRO_BODY) {
                m.body[m.nbody++] = lx->tokens[i];
                i++;
            }
            if (g_nmacros < GMB_MACRO_MAX) g_macros[g_nmacros++] = m;
            else { free(m.name); free(m.params); }
            continue;
        }

        if (t.length > 0) {
            char* name = strndup(t.lexeme, (size_t)t.length);
            gmb_macro_t* m = macro_find(name);
            free(name);
            if (m) {
                if (m->nparams == 0) {
                    i++;
                    for (int b = 0; b < m->nbody; b++) out_push(&out, m->body[b]);
                    continue;
                }
                /* macro con parametri: valida solo se seguita da '(' (ignorando Newline) */
                int j = i + 1;
                while (j < lx->count && lx->tokens[j].kind == GMB_TOK_Newline) j++;
                if (j < lx->count && lx->tokens[j].kind == GMB_TOK_LeftParen) {
                    /* espandi: raccogli gli argomenti separati da virgola fino a ')' */
                    i = j + 1;
                    gmb_token_t args[GMB_MACRO_PARAMS][GMB_MACRO_BODY];
                    int arglen[GMB_MACRO_PARAMS] = {0};
                    int nargs = 0;
                    int depth = 0;
                    while (i < lx->count && lx->tokens[i].kind != GMB_TOK_Eof) {
                        if (lx->tokens[i].kind == GMB_TOK_LeftParen) depth++;
                        else if (lx->tokens[i].kind == GMB_TOK_RightParen) {
                            if (depth == 0) { i++; break; }
                            depth--;
                        } else if (lx->tokens[i].kind == GMB_TOK_Comma && depth == 0) {
                            if (nargs < GMB_MACRO_PARAMS - 1) nargs++;
                            i++;
                            continue;
                        }
                        if (nargs < GMB_MACRO_PARAMS && arglen[nargs] < GMB_MACRO_BODY)
                            args[nargs][arglen[nargs]++] = lx->tokens[i];
                        i++;
                    }
                    nargs = nargs + 1;
                    /* sostituzione: per ogni token del corpo, se è un parametro metti l'argomento */
                    for (int b = 0; b < m->nbody; b++) {
                        int pi = -1;
                        if (m->body[b].length > 0) {
                            char* pn = strndup(m->body[b].lexeme, (size_t)m->body[b].length);
                            for (int k = 0; k < m->nparams; k++)
                                if (strcasecmp(m->params[k], pn) == 0) { pi = k; break; }
                            free(pn);
                        }
                        if (pi >= 0 && pi < nargs) {
                            for (int a = 0; a < arglen[pi]; a++) out_push(&out, args[pi][a]);
                        } else out_push(&out, m->body[b]);
                    }
                    continue;
                }
            }
        }
        out_push(&out, t);
        i++;
    }
    return out;
}
