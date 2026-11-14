/* gmb_trace.c — stack trace BASIC del runtime C. Vedi gmb_trace.h per il contratto.
 *
 * Nessuna allocazione per frame: la catena è un array thread-local di entry (nome + riga)
 * e il "frame" che l'emitter C99 porta nello stack della procedura è solo l'indice.
 * Così un longjmp (gmb_raise_error) lascia la profondità alta ma NON lascia puntatori a
 * stack morto: la scrittura della riga resta sempre dentro l'array.
 */
#include "gmb_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__thread gmb_trace_ent_t gmb_trace_stack[GMB_TRACE_MAX];
__thread int gmb_trace_n = 0;

gmb_trace_frame_t gmb_trace_enter(const char* fn, long line)
{
    gmb_trace_frame_t f;
    f.idx = gmb_trace_n;
    if (gmb_trace_n < GMB_TRACE_MAX) {
        gmb_trace_stack[gmb_trace_n].fn = (fn && *fn) ? fn : "?";
        gmb_trace_stack[gmb_trace_n].line = line;
        gmb_trace_n++;
    }
    return f;
}

void gmb_trace_leave(gmb_trace_frame_t* f)
{
    if (!f) return;
    /* Uscita NORMALE: il frame è la cima. Uscita dopo un longjmp catturato: i frame più
     * interni sono ancora contati (i loro cleanup non sono passati) → si tronca a questo. */
    if (f->idx < gmb_trace_n) gmb_trace_n = f->idx;
}

void gmb_trace_restore_frame(const gmb_trace_frame_t* f)
{
    if (!f) return;
    if (f->idx + 1 <= gmb_trace_n) gmb_trace_n = f->idx + 1;
}

void gmb_trace_restore_depth(int n)
{
    if (n < 0) n = 0;
    if (n < gmb_trace_n) gmb_trace_n = n;
}

int gmb_trace_depth(void)
{
    return gmb_trace_n;
}

char* gmb_trace_text_skip(int skip)
{
    size_t cap = 64;
    for (int i = skip; i < gmb_trace_n; i++) cap += strlen(gmb_trace_stack[i].fn) + 32;
    char* out = (char*)malloc(cap);
    if (!out) return strdup("");
    size_t o = 0;
    out[0] = 0;
    for (int i = gmb_trace_n - 1; i >= skip; i--) {
        int w = snprintf(out + o, cap - o, "  in %s (riga %ld)\n",
                         gmb_trace_stack[i].fn, gmb_trace_stack[i].line);
        if (w <= 0 || (size_t)w >= cap - o) break;
        o += (size_t)w;
    }
    return out;
}

/* Snapshot della catena catturato all'ULTIMO raise (gmb_raise_error): dopo il longjmp i
 * frame delle procedure attraversate non esistono più, quindi il testo va catturato PRIMA
 * del salto. `ERR.StackTrace` e il messaggio fatale leggono questo. */
static __thread char* g_err_trace_snapshot = NULL;

void gmb_trace_snapshot(void)
{
    free(g_err_trace_snapshot);
    g_err_trace_snapshot = gmb_trace_text_skip(0);
}

const char* gmb_err_trace(void)
{
    return g_err_trace_snapshot ? g_err_trace_snapshot : "";
}
