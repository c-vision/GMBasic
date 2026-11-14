/* gmb_trace.c — stack trace BASIC del runtime C. Vedi gmb_trace.h per il contratto.
 * Nessuna allocazione per frame: la catena vive nello stack delle procedure BASIC (l'emitter
 * C99 emette `gmb_trace_frame_t __gmb_tf __attribute__((cleanup(gmb_trace_leave))) = ...`).
 */
#include "gmb_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gmb_trace_frame_t* gmb_trace_top = NULL;

gmb_trace_frame_t gmb_trace_enter(const char* fn, long line)
{
    gmb_trace_frame_t f;
    f.fn = (fn && *fn) ? fn : "?";
    f.line = line;
    f.prev = gmb_trace_top;
    gmb_trace_top = &f;
    return f;
}

void gmb_trace_leave(gmb_trace_frame_t* f)
{
    /* `cleanup` passa l'indirizzo della variabile locale: se è la cima, si torna al
     * chiamante. Il controllo su `prev` rende la funzione idempotente e sicura anche se
     * chiamata su un frame non in cima (es. longjmp che salta un livello). */
    if (!f) return;
    if (gmb_trace_top == f) gmb_trace_top = f->prev;
    else {
        /* rimuove f dalla catena (caso raro: uscita fuori ordine) */
        gmb_trace_frame_t* p = gmb_trace_top;
        while (p && p->prev != f) p = p->prev;
        if (p && p->prev == f) p->prev = f->prev;
    }
}

int gmb_trace_depth(void)
{
    int n = 0;
    for (gmb_trace_frame_t* f = gmb_trace_top; f; f = f->prev) n++;
    return n;
}

/* Snapshot della catena catturato all'ULTIMO raise (gmb_raise_error): dopo il longjmp i
 * frame delle procedure attraversate non esistono più, quindi il testo va catturato PRIMA
 * del salto. `ERR.StackTrace` e il messaggio fatale leggono questo. */
static char* g_err_trace_snapshot = NULL;

void gmb_trace_snapshot(void)
{
    free(g_err_trace_snapshot);
    g_err_trace_snapshot = gmb_trace_text_skip(0);
}

const char* gmb_err_trace(void)
{
    return g_err_trace_snapshot ? g_err_trace_snapshot : "";
}

char* gmb_trace_text_skip(int skip)
{
    size_t cap = 64;
    int n = 0;
    for (gmb_trace_frame_t* f = gmb_trace_top; f; f = f->prev) { n++; cap += strlen(f->fn) + 32; }
    char* out = (char*)malloc(cap);
    if (!out) return strdup("");
    size_t o = 0;
    int i = 0;
    out[0] = 0;
    for (gmb_trace_frame_t* f = gmb_trace_top; f; f = f->prev, i++) {
        if (i < skip) continue;
        int w = snprintf(out + o, cap - o, "  in %s (riga %ld)\n", f->fn, f->line);
        if (w <= 0 || (size_t)w >= cap - o) break;
        o += (size_t)w;
    }
    return out;
}
