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

/* ── TESTO SORGENTE per ERR.SourceLineText ────────────────────────────────────────────────
 * Il codice generato incorpora le righe del sorgente concatenato come UN letterale
 * ("riga1\nriga2\n...", indice 0 = riga 1 — la stessa numerazione di `chunk->Lines` e dei
 * `#line` del C emesso) e lo registra all'avvio. Il testo si cerca solo quando un errore
 * viene letto (`ERR.SourceLineText`), quindi la scansione lineare non costa nulla nei
 * percorsi caldi. */
static const char* g_src_blob = NULL;   /* GLOBALE: registrato una volta, letto da qualunque thread */
/* Variante ad ARRAY (chunk A1: le righe arrivano già separate) — usata dal VM/C. */
static const char* const* g_src_lines = NULL;
static int g_src_lines_n = 0;

void gmb_src_text_register(const char* blob) { g_src_blob = blob; }

void gmb_src_lines_register(const char* const* lines, int count)
{
    g_src_lines = lines;
    g_src_lines_n = lines ? count : 0;
}

const char* gmb_src_line_text(long line)
{
    const char* p;
    const char* e;
    if (g_src_lines != NULL)
    {
        if (line < 1 || line > (long)g_src_lines_n) return "";
        p = g_src_lines[line - 1];
        if (p == NULL) return "";
        e = p + strlen(p);
    }
    else
    {
        if (g_src_blob == NULL || line < 1) return "";
        p = g_src_blob;
        for (long cur = 1; cur < line; cur++) {
            while (*p && *p != '\n') p++;
            if (*p != '\n') return "";          /* riga oltre la fine del blob */
            p++;
        }
        e = p;
        while (*e && *e != '\n') e++;
    }
    /* Copia trimata (spazi iniziali e finali): il testo della riga senza indentazione, così
     * l'oracolo è stabile fra leg e non dipende dalla formattazione del sorgente. */
    static __thread char buf[512];
    size_t n = (size_t)(e - p);
    while (n > 0 && (p[0] == ' ' || p[0] == '\t')) { p++; n--; }
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r')) n--;
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, p, n);
    buf[n] = 0;
    return buf;
}
