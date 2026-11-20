/* gmb_c99_emit_clock.c — emissione del dominio `CLOCK` (tempo a passo fisso).
 *
 * Sintassi (comandi legacy a spazio, §1: nessuna parentesi):
 *   CLOCK PAUSE                  ' ferma la simulazione (la coda si azzera)
 *   CLOCK RESUME                 ' riprende dal passo successivo
 *   CLOCK SCALE secondi          ' moltiplicatore del tempo reale (0.5 = slow motion)
 *   CLOCK FPS frequenza          ' passi di simulazione al secondo (1..1000)
 *
 * Letture (funzioni, §1: leggono e basta):
 *   CLOCK.DT(), CLOCK.ALPHA(), CLOCK.FPS(), CLOCK.SCALE()
 *
 * Ogni emissione è UNA chiamata alla facciata `gmb_clock_frame_*` del runtime
 * (gmb_clock.c): qui non c'è stato né logica, solo la traduzione dei termini.
 * Lo STESSO orologio alimenta DOEVENTS (`gmb_form_frame(0)`), quindi il programma
 * e il ciclo grafico parlano dello stesso tempo: una sola sorgente (§0bis).
 * Verbo sconosciuto = errore esplicito, mai no-op silenzioso.
 */
#include "gmb_c99_emit_internal.h"

#include <string.h>

/* `gmb_clock_frame_xxx(<expr>);` — i comandi non hanno valore di ritorno (§1). */
static void ccall1(ctx_t* c, const char* fn, gmb_expr_t* a, int line, const char* usage)
{
    if (!a) { c99_fail(c, line, "%s", usage); return; }
    ind(c);
    sb_printf(&c->out, "%s(", fn);
    emit_expr(c, a);
    sb_put(&c->out, ");\n");
}

void emit_clock_domain(ctx_t* c, gmb_stmt_t* s)
{
    int tk = s->act_tk;
    gmb_expr_t** a = s->go_args;
    int n = s->n_go_args;

    if (tk == GMB_TOK_Pause) { ind(c); sb_put(&c->out, "gmb_clock_frame_pause(1);\n"); return; }
    if (tk == GMB_TOK_Resume) { ind(c); sb_put(&c->out, "gmb_clock_frame_pause(0);\n"); return; }
    if (tk == GMB_TOK_Scale)
    {
        if (n < 1) { c99_fail(c, s->line, "CLOCK SCALE: manca il moltiplicatore (0..16)"); return; }
        ccall1(c, "gmb_clock_frame_set_scale", a[0], s->line, "CLOCK SCALE: manca il moltiplicatore (0..16)");
        return;
    }
    if (tk == GMB_TOK_Fps)
    {
        if (n < 1) { c99_fail(c, s->line, "CLOCK FPS: manca la frequenza (1..1000)"); return; }
        ccall1(c, "gmb_clock_frame_set_fps", a[0], s->line, "CLOCK FPS: manca la frequenza (1..1000)");
        return;
    }
    c99_fail(c, s->line, "CLOCK: verbo non supportato dall'emitter nativo");
}

/* Namespace della call: il parser a volte conserva `ns` a parte, altre fonde il nome
 * (`var = "CLOCK.DT"` con `ns` vuoto). Entrambe le forme sono legittime: si ricava il
 * namespace dal prefisso quando `ns` non c'è (stessa regola del dominio WORLD). */
static int cns_is_clock(const char* name, const char* ns)
{
    if (ns && ns[0]) return strcasecmp(ns, "CLOCK") == 0;
    if (!name) return 0;
    const char* dot = strrchr(name, '.');
    if (!dot || dot == name) return 0;
    size_t n = (size_t)(dot - name);
    return n == 5 && strncasecmp(name, "CLOCK", 5) == 0;
}

/* Letture qualificate: CLOCK.DT/ALPHA/FPS/SCALE. Il filtro sul namespace serve perché
 * `FPS` e `SCALE` sono parole usate altrove: senza qualifica CLOCK non è roba nostra. */
int emit_bf_clock(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk,
                  gmb_expr_t** args, int nargs, int line)
{
    (void)args; (void)line;
    if (!cns_is_clock(name, ns)) return 0;

    const char* fn = NULL;
    switch (tk)
    {
        case GMB_TOK_ClockDt:    fn = "gmb_clock_frame_dt"; break;
        case GMB_TOK_ClockAlpha: fn = "gmb_clock_frame_alpha"; break;
        case GMB_TOK_ClockFps:   fn = "gmb_clock_frame_fps"; break;
        case GMB_TOK_ClockScale: fn = "gmb_clock_frame_scale"; break;
        default: return 0;
    }
    if (nargs != 0) return 0;   /* nessuna lettura del dominio accetta argomenti */
    sb_printf(&c->out, "%s()", fn);
    return 1;
}
