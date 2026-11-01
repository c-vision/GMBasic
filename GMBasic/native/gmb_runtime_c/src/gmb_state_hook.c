/* ============================================================================
 * gmb_state_hook.c — implementazione dell'hook opzionale di stato (vedi header).
 *
 * Nessuna allocazione attraversa il confine: il client scrive in un buffer fornito
 * da questo file; se la risposta non ci sta si rialloca e si richiama (le letture
 * sono idempotenti; per le mutazioni il chiamante passa una risposta corta, "0"/"1").
 * ============================================================================ */
#include "gmb_state_hook.h"

#include <stdlib.h>
#include <string.h>

static gmb_state_query_fn g_state_fn = NULL;
static void* g_state_ctx = NULL;

void gmb_state_hook_set(gmb_state_query_fn fn, void* ctx) {
    g_state_fn = fn;
    g_state_ctx = ctx;
}

int gmb_state_hook_active(void) { return g_state_fn != NULL; }

/* Trasporto comune: interroga il client e ritorna il buffer (malloc) o NULL. */
static char* hook_call(const char* surface, const char* a, const char* b, const char* c) {
    gmb_state_query_fn fn = g_state_fn;
    if (!fn || !surface) return NULL;

    char* out = NULL;
    int cap = 8192;
    for (int attempt = 0; attempt < 4; attempt++) {
        char* buf = (char*)malloc((size_t)cap + 1);
        if (!buf) { out = NULL; break; }
        int n = fn(g_state_ctx, surface, a, b, c, buf, cap);
        if (n < 0) { free(buf); out = NULL; break; }        /* superficie non gestita */
        if (n <= cap) { buf[n] = '\0'; out = buf; break; }  /* risposta completa */
        free(buf);
        cap = n + 1;                                        /* troncata: riprova più grande */
    }
    return out;
}

char* gmb_state_hook_query(const char* surface, const char* a, const char* b) {
    return hook_call(surface, a, b, NULL);
}

int gmb_state_hook_mutate(const char* op, const char* a, const char* b, const char* c) {
    char* r = hook_call(op, a, b, c);
    if (!r) return -1;                 /* non gestita → registro C */
    int ok = (r[0] == '1');            /* "1" = riuscita, "0" = rifiutata */
    free(r);
    return ok;
}
