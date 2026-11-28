/* ============================================================================
 * gmb_console.c — output console (print/cls/locate/color) e sleep.
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include "gmb_c99gpm.h"   /* gmb_task_t: la cadenza TICK vive sul task (migra col task) */
#include "gmb_ui_gfx.h"
extern int gmb_gfx_present_video_is_real(void);
#include "gmb_graphics.h"
#include "gmb_tui_io.h"
#include "gmb_tui_app.h"
#include "gmb_tui_clipper.h"
#include "gmb_tui_widgets.h"
#include "gmb_console_extra.h"
#include "gmb_iconsole.h"
#include "gmb_form.h"

#include <stdlib.h>   /* _Exit: guardia §0ter delle attese bloccanti (gmb_sleep_ms) */

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <io.h>
#else
#include <time.h>
#include <unistd.h>
#endif

/* Colonna corrente di stampa (parity concettuale IlRuntime._ilPrintColumn): quanti
 * caratteri sono stati scritti dall'ultimo newline. Usata SOLO da gmb_print_tab_zone per
 * calcolare il padding della PRINT a virgola (zona di 14 colonne) — azzerata da
 * gmb_print_newline. Non insegue posizione del cursore su terminale/grafica, solo i
 * caratteri effettivamente inviati da questo modulo (stesso schema dell'oracolo IL, che
 * traccia la lunghezza del testo scritto, non la colonna video reale). */
static int g_print_col = 0;

/* Larghezza in code-point Unicode (canone 2026-09-30, parità IlRuntime._ilPrintColumn che
 * conta string.Length): i byte di continuazione UTF-8 (10xxxxxx) non avanzano la colonna.
 * Prima strlen (byte) → "Età:" (5 byte, 4 caratteri) sfasava le zone TAB di 1. */
static int gmb_utf8_codepoints(const char* text)
{
    int n = 0;
    if (!text) return 0;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Staging di stampa PER-STATEMENT (canone: SCARTARE il parziale su errore, parità
 * il/pcode/vmc). Se un errore scatta mentre si valuta un argomento di PRINT, l'output
 * PARZIALE già prodotto non deve comparire: viene bufferizzato e scartato al raise.
 *
 * __thread (TLS) OBBLIGATORIO (bug NetSslConcurrency 2026-10-07, parità con lo stage
 * ThreadStatic di IlRuntime): i task GPM possono girare su thread diversi; con i buffer
 * GLOBALI due PRINT concorrenti si infilavano l'uno nell'altro nello stesso s_stage_buf
 * (righe fuse/duplicate osservate 3/3 sulle gambe C con 10 task concorrenti). Con buffer
 * per-thread ogni istruzione commette la SUA riga in un solo fwrite (atomico su FILE*).
 * Limite noto ( identico al flat IL ThreadStatic): un PRINT che parcheggia A METÀ
 * (argomento future) e riprende su un altro thread perde il parziale — caso raro, non
 * corruzione incrociata. */
static __thread char*  s_stage_buf = NULL;
static __thread size_t s_stage_len = 0, s_stage_cap = 0;
static __thread int    s_stage_active = 0;
static __thread int    s_stage_col0 = 0;

/* Livelli SOSPESI dello staging. Un PRINT ANNIDATO (es. un PRINT dentro un generatore
 * valutato come argomento di un altro PRINT) non deve distruggere il parziale del PRINT
 * esterno: `begin` SOSPENDE il livello corrente e ne apre uno nuovo, `end` lo chiude e
 * riprende il precedente. Così `PRINT "pref="; NEXT(it)` con un PRINT nel generatore dà
 * la riga del generatore e poi `pref= 7` — come IL/PCode/VM-C.
 *
 * Su ERRORE tutti i livelli si SCARTANO (gmb_print_stage_abort): il longjmp salta gli
 * `end` dell'emitter, quindi i livelli pendenti non vanno ripresi né committati (canone
 * "un PRINT interrotto non emette la riga parziale"). */
#define GMB_STAGE_MAX 8
typedef struct { char* buf; size_t len, cap; int col0; } gmb_stage_lvl_t;
static __thread gmb_stage_lvl_t s_stage_saved[GMB_STAGE_MAX];
static __thread int s_stage_depth = 0;

/* Modalità SCREEN TUI (definizione sotto): dichiarazioni anticipate perché lo staging
 * di stampa (sopra la definizione) deve instradare il testo commesso allo screen. */
static int  s_tui_on = 0;
static int  s_force_console = 0;
void gmb_print_force_console(int on) { s_force_console = on ? 1 : 0; }
static int  s_tui_row = 1, s_tui_col = 1;
static void tui_screen_put(const char* text);

void gmb_print_stage_begin(void)
{
    if (s_stage_active && s_stage_depth < GMB_STAGE_MAX)
    {
        s_stage_saved[s_stage_depth].buf = s_stage_buf;
        s_stage_saved[s_stage_depth].len = s_stage_len;
        s_stage_saved[s_stage_depth].cap = s_stage_cap;
        s_stage_saved[s_stage_depth].col0 = s_stage_col0;
        s_stage_depth++;
        s_stage_buf = NULL; s_stage_len = 0; s_stage_cap = 0;
    }
    s_stage_active = 1; s_stage_len = 0; s_stage_col0 = g_print_col;
}

void gmb_print_stage_end(int commit)
{
    if (!s_stage_active) return;
    if (commit && s_stage_len > 0)
    {
        /* CPRINT (s_force_console): SEMPRE su stdout, anche con TUI/gfx attivi (semantica
         * Clipper CPrint; il commento dell'emitter: "forzato su console (stdout)").
         * Altrimenti: TUI attiva → screen; gfx → pagina; else → stdout. */
        if (s_force_console) { fwrite(s_stage_buf, 1, s_stage_len, stdout); fflush(stdout); }
        else if (s_tui_on) tui_screen_put(s_stage_buf);
        else if (GMB_GFX_SCREEN_ON()) GMB_GFX_PRINT(s_stage_buf);
        else {
            /* Console attiva (main loop C#: activeConsole.Out): Standard/System espliciti o
             * SDL con finestra. Default headless → fallback stdout (comportamento invariato). */
            char* _tmp = (char*)malloc(s_stage_len + 1);
            int _handled = 0;
            if (_tmp) { memcpy(_tmp, s_stage_buf, s_stage_len); _tmp[s_stage_len] = 0;
                        _handled = gmb_iconsole_try_write(_tmp); free(_tmp); }
            if (!_handled) { fwrite(s_stage_buf, 1, s_stage_len, stdout); fflush(stdout); }
        }
    }
    if (!commit) g_print_col = s_stage_col0;   /* scartato: ripristina la colonna di inizio */
    s_stage_len = 0;
    if (s_stage_depth > 0)
    {
        /* riprende il livello esterno (il buffer del livello chiuso si libera) */
        char* child = s_stage_buf;
        s_stage_depth--;
        s_stage_buf = s_stage_saved[s_stage_depth].buf;
        s_stage_len = s_stage_saved[s_stage_depth].len;
        s_stage_cap = s_stage_saved[s_stage_depth].cap;
        s_stage_col0 = s_stage_saved[s_stage_depth].col0;
        s_stage_saved[s_stage_depth].buf = NULL;
        s_stage_active = 1;
        if (child && child != s_stage_buf) free(child);
    }
    else
    {
        s_stage_active = 0;
    }
}

/* ERRORE (gmb_raise_error): scarta TUTTI i livelli pendenti. Il longjmp salta gli `end`
 * emessi dall'emitter, quindi i livelli aperti non verrebbero mai chiusi: qui si buttano. */
void gmb_print_stage_abort(void)
{
    if (s_stage_depth > 0) g_print_col = s_stage_saved[0].col0;
    for (int i = 0; i < s_stage_depth; i++)
    {
        free(s_stage_saved[i].buf);
        s_stage_saved[i].buf = NULL;
        s_stage_saved[i].len = s_stage_saved[i].cap = 0;
    }
    s_stage_depth = 0;
    s_stage_active = 0;
    s_stage_len = 0;
}

/* ============================================================================
 * Modalità SCREEN TUI (parità IL su C99Native): quando una finestra WINDOW è attiva,
 * PRINT/LOCATE/CLS scrivono nel buffer dello screen (gmb_clipper_global_state()->buffer)
 * invece che su stdout, e lo screen viene reso in ANSI con gmb_render_screen.
 * LOCATE è window-relative (come IL): offset = content-rect della finestra focalizzata.
 * Prima sul C99Native la TUI appariva come testo sequenziale su stdout (niente finestra).
 * ========================================================================== */
/* s_tui_on / s_tui_row / s_tui_col: definiti sopra (servono anche allo staging). */

/* ============================================================================
 * CONSOLE DI SISTEMA per i FRAME TUI (2026-11-12)
 *
 * I frame (clear + render ANSI del buffer TUI) NON sono output del programma: sono
 * interfaccia. Vanno quindi sulla console di sistema, MAI sul canale dati del processo:
 * prima si scriveva sempre su stdout, così con l'output rediretto (harness, `prog > f`,
 * pipe, servizio) i frame finivano DENTRO l'output del programma — su VMC
 * `controls/window_events` stampava i bordi `┌─[■]…` prima di `flags: …`, e sul VHS le
 * gambe managed li mandavano al terminale dell'harness.
 *
 * Regola (una scelta per processo, il terminale non cambia):
 *   1. stdout è un TERMINALE → è già la console: stdout (uso interattivo normale);
 *   2. altrimenti il TERMINALE DI CONTROLLO (/dev/tty, su Windows CONOUT$);
 *   3. nessun terminale (headless, servizio, CI) → NULL: i frame si SCARTANO.
 * ========================================================================== */
FILE* gmb_tui_console_out(void)
{
    static int done = 0;
    static FILE* f = NULL;
    if (done) return f;
    done = 1;
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(_fileno(stdout))) { f = stdout; return f; }
    f = fopen("CONOUT$", "w");
#else
    if (isatty(fileno(stdout))) { f = stdout; return f; }
    f = fopen("/dev/tty", "w");
#endif
    return f;
}

void gmb_tui_screen_enable(void)
{
    /* Settings: applica consoleConfiguration[0] (charGrid/logical) una volta. */
    { static int applied = 0; if (!applied) { applied = 1; gmb_settings_apply_console_default(); } }
    /* Pulizia schermo + cursore nascosto: la TUI prende il controllo del terminale (altrimenti
     * banner/compilazione restano mescolati al frame). */
    { void gmb_console_apply_env_once(void); gmb_console_apply_env_once(); }
    if (!s_tui_on) {
        FILE* co = gmb_tui_console_out();   /* console di sistema, MAI il canale dati */
        if (co) { fputs("\x1b[2J\x1b[H\x1b[?25l", co); fflush(co); }
        /* Desktop dal TEMA (default Turbo = blu Borland + ░): un'unica famiglia di
         * finestre/controlli, nessuna palette "Clipper" separata. */
        gmb_clipper_state_t* st = gmb_clipper_global_state();
        gmb_theme_t th0 = gmb_ui_theme();
        gmb_cell_color_t dfg = (th0.head_fg.r || th0.head_fg.g || th0.head_fg.b) ? th0.head_fg : th0.text;
        if (st && st->buffer) {
            st->fg = dfg;
            st->bg = th0.desktop_bg;
            gmb_theme_paint_desktop(st->buffer);
        }
    }
    s_tui_on = 1; s_tui_row = 1; s_tui_col = 1;
}
int  gmb_tui_screen_active(void) { return s_tui_on; }

void gmb_tui_screen_flush(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (!st || !st->buffer) return;
    /* SdlConsole attiva: rende lei (RenderBuffer); niente dump ANSI su stdout. */
    if (gmb_iconsole_sdl_active()) return;
    /* SDLConsole dell'APP (gmb_tui_sdl_start: finestra a celle posseduta da gmb_tui_app):
     * rende gmb_tui_sdl_present(); senza questo return il flush dell'ANSI finiva su stdout
     * a ogni frame sopra la finestra SDL. */
    { extern int gmb_tui_sdl_active(void); if (gmb_tui_sdl_active()) return; }
    /* Screen grafico attivo: il buffer TUI verrà presentato sulla finestra dal pump
     * del main thread (gmb_gpm_start). Qui NON si tocca SDL (siamo su un worker) e NON
     * si scrive ANSI su stdout. SDLConsole/TUI (screen spento): ANSI come prima. */
    extern void gmb_tui_present_gfx(void);
    if (GMB_GFX_SCREEN_ON()) { gmb_tui_present_gfx(); return; }
    static char* fbuf = NULL;
    static size_t fcap = 0;
    size_t need = (size_t)gmb_screen_width(st->buffer) * (size_t)gmb_screen_height(st->buffer) * 32 + 4096;
    if (fcap < need) {
        char* nb = (char*)realloc(fbuf, need);
        if (!nb) return;
        fbuf = nb; fcap = need;
    }
    size_t n = gmb_render_screen(st->buffer, fbuf, fcap);
    {   /* frame sulla CONSOLE DI SISTEMA (mai sul canale dati: vedi gmb_tui_console_out) */
        FILE* co = gmb_tui_console_out();
        if (n && co) { fwrite(fbuf, 1, n, co); fflush(co); }
    }
}

/* Offset (0-based) del contenuto della finestra attiva: focalizzata, altrimenti la prima. */
static void tui_window_offset(int* ox, int* oy)
{
    *ox = 0; *oy = 0;
    gmb_window_manager_t* wm = gmb_window_manager_static();
    if (!wm) return;
    int wid = gmb_window_manager_focused(wm);
    if (wid < 0 && wm->count > 0) wid = wm->windows[0].id;
    gmb_managed_window_t* mw = (wid >= 0) ? gmb_window_manager_get(wm, wid) : NULL;
    if (mw) { gmb_rect_t cr = gmb_managed_window_content_rect(mw); *ox = cr.x; *oy = cr.y; }
}

static uint32_t tui_utf8_decode(const char* p, int* adv)
{
    const unsigned char* u = (const unsigned char*)p;
    if (u[0] < 0x80) { *adv = 1; return u[0]; }
    if ((u[0] & 0xE0) == 0xC0 && u[1]) { *adv = 2; return (uint32_t)(((u[0] & 0x1F) << 6) | (u[1] & 0x3F)); }
    if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2]) { *adv = 3; return (uint32_t)(((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F)); }
    if ((u[0] & 0xF8) == 0xF0 && u[1] && u[2] && u[3]) { *adv = 4; return (uint32_t)(((u[0] & 0x07) << 18) | ((u[1] & 0x3F) << 12) | ((u[2] & 0x3F) << 6) | (u[3] & 0x3F)); }
    *adv = 1; return u[0];
}

/* Scrive il testo nel buffer screen alla posizione del cursore (window-relative). */
static void tui_screen_put(const char* text)
{
    if (!text) return;
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (!st || !st->buffer) return;
    int bw = gmb_screen_width(st->buffer), bh = gmb_screen_height(st->buffer);
    int ox = 0, oy = 0;
    tui_window_offset(&ox, &oy);
    int row = oy + (s_tui_row - 1), col = ox + (s_tui_col - 1);
    for (const char* p = text; *p; ) {
        char c = *p;
        if (c == '\n') { row++; col = ox; p++; continue; }
        if (c == '\r') { col = ox; p++; continue; }
        int adv = 1;
        uint32_t cp = tui_utf8_decode(p, &adv);
        if (row >= 0 && row < bh && col >= 0 && col < bw)
            gmb_screen_setchar(st->buffer, col, row, cp, st->fg, st->bg);
        col++;
        p += adv;
    }
    s_tui_row = row - oy + 1;
    s_tui_col = col - ox + 1;
    if (s_tui_col < 1) s_tui_col = 1;
    gmb_tui_screen_flush();
}

static void console_or_gfx_text(const char* text)
{
    if (!text) return;
    g_print_col += gmb_utf8_codepoints(text);
    if (s_stage_active)
    {
        size_t n = strlen(text);
        if (s_stage_len + n + 1 > s_stage_cap)
        {
            size_t nc = s_stage_cap ? s_stage_cap : 256;
            while (nc < s_stage_len + n + 1) nc *= 2;
            char* nb = (char*)realloc(s_stage_buf, nc);
            if (!nb) return;
            s_stage_buf = nb; s_stage_cap = nc;
        }
        memcpy(s_stage_buf + s_stage_len, text, n);
        s_stage_len += n; s_stage_buf[s_stage_len] = 0;
        return;
    }
    if (!s_force_console && s_tui_on) { tui_screen_put(text); return; }
    if (!s_force_console && GMB_GFX_SCREEN_ON()) GMB_GFX_PRINT(text);
    else if (!gmb_iconsole_try_write(text)) { fputs(text, stdout); fflush(stdout); }
}

void gmb_print_string(gmb_string_t s) {
    console_or_gfx_text(s.data);
}

/* Stampa di un TEMPORANEO: stampa e libera il buffer (nessun altro lo possiede).
 * Usata dall'emitter C99 per gli argomenti di PRINT che sono concat/call/letterali:
 * senza, ogni `PRINT "x" + STR$(i)` dentro un ciclo lasciava vivo un buffer per giro
 * (leak per iterazione, 2026-11-13). */
void gmb_print_string_tmp(gmb_string_t s) {
    console_or_gfx_text(s.data);
    gmb_string_free(&s);
}

/* Stampa di un VALORE TESTO il cui tipo si decide dal contenuto (getter RDD su percorsi
 * C99/C99Native, dove il motore RDD conserva celle testuali): testo numerico → display
 * double con il padding Clipper (identico ai builtin VM/C e al gestito), altrimenti
 * stringa. Unica implementazione (§0bis): il predicato è gmb_is_numeric, lo stesso dei
 * builtin della VM/C. */
void gmb_print_text_typed(const char* s)
{
    if (s && *s && gmb_is_numeric(s)) { gmb_print_double(strtod(s, NULL)); return; }
    gmb_print_string(gmb_string_alloc(s ? s : ""));
}

void gmb_print_int(gmb_int_t val) {
    /* C# FormatPrintValue int: (i>=0?" ":"") + i + " " */
    char buf[40];
    int n = 0;
    if (val >= 0) buf[n++] = ' ';
    n += snprintf(buf + n, sizeof(buf) - n, "%lld", (long long)val);
    if (n < (int)sizeof(buf) - 1) buf[n++] = ' ';
    buf[n] = '\0';
    console_or_gfx_text(buf);
}

void gmb_print_double(gmb_double_t val) {
    /* C# FormatDoubleValue: (d>=0?" ":"") + G15 + " " ; NaN/Infinity come .NET G15. */
    char buf[64];
    if (isnan(val)) snprintf(buf, sizeof(buf), "NaN");
    else if (isinf(val)) snprintf(buf, sizeof(buf), val > 0 ? "Infinity" : "-Infinity");
    else {
        if (val == 0.0) val = 0.0;   /* -0.0 -> "0" (parita con .NET/sample) */
snprintf(buf, sizeof(buf), "%.15g", val);
        char* e = strchr(buf, 'e');
        if (e) { *e = 'E'; if (e[1] != '-' && e[1] != '+') { memmove(e + 2, e + 1, strlen(e + 1) + 1); e[1] = '+'; } }
    }
    if (GMB_GFX_SCREEN_ON())
    {
        char out[80];
        int n = 0;
        if (val >= 0.0) out[n++] = ' ';
        int len = (int)strlen(buf);
        memcpy(out + n, buf, len);
        n += len;
        out[n++] = ' ';
        out[n] = '\0';
        console_or_gfx_text(out);
        return;
    }
    /* Passa da console_or_gfx_text (invece di putchar/fputs diretti) per tenere la
     * colonna di stampa allineata a quella tracciata da gmb_print_tab_zone — altrimenti
     * `PRINT n, x` calcolerebbe il padding della virgola come se il numero non avesse
     * occupato colonne (bug osservato: FOR EACH k, v IN dict, campi struct/UDT ecc. su
     * C99Native perdevano l'allineamento a colonne di IL/VM-C). */
    {
        char out[80];
        int n = 0;
        if (val >= 0.0) out[n++] = ' ';
        int len = (int)strlen(buf);
        memcpy(out + n, buf, len);
        n += len;
        out[n++] = ' ';
        out[n] = '\0';
        console_or_gfx_text(out);
    }
}

void gmb_print_bool(gmb_bool_t val) {
    /* C# FormatPrintValue: bool → "-1 " (true) / " 0 " (false) */
    console_or_gfx_text(val ? "-1 " : " 0 ");
}

/* Accumula in un buffer dinamico (stesso schema dello staging): il ramo lista/dict di
 * gmb_print_val scriveva DIRETTAMENTE su stdout, bypassando il buffer per-statement → una
 * lista stampata dentro un PRINT usciva PRIMA del testo già in buffer
 * (`[alpha, 10]BPlusTree.Count= 3 , Valore Min=`, CollectionsModern). Ora il testo passa da
 * console_or_gfx_text, che gestisce staging/TUI/console E la colonna (UTF-8 aware). */
static void print_buf_add(char** buf, size_t* cap, size_t* len, const char* s)
{
    if (!s) return;
    size_t n = strlen(s);
    if (*len + n + 1 > *cap)
    {
        size_t nc = *cap ? *cap : 128;
        while (nc < *len + n + 1) nc *= 2;
        char* nb = (char*)realloc(*buf, nc);
        if (!nb) return;
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
}

void gmb_print_val(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_INT: gmb_print_int(v.v_int); break;
        case GMB_TYPE_DOUBLE: gmb_print_double(v.v_double); break;
        case GMB_TYPE_BOOL: gmb_print_bool(v.v_bool); break;
        case GMB_TYPE_STRING: gmb_print_string(v.v_string); break;
        case GMB_TYPE_CURRENCY: {
            /* Campo DBF N/F nudo (CURRENCY): resa con `scale` decimali e spazio-segno
             * (" 100.00"), parità gmb_currency_string(lead=1)/gmb_cstr_val/IL/PCode/VM-C.
             * Prima non gestito → il `default: break` stampava VUOTO (`PRINT <campo nudo>`
             * senza output su C99Native, divergenza da IL/PCode/VM-C). */
            gmb_string_t s = gmb_cstr_val(v);
            if (s.data) gmb_print_string(s);
            break;
        }
        case GMB_TYPE_OBJECT: {
            /* lista → `[e1, e2, ...]` (parità IL/VM-C gmb_format_value). Il testo si compone
             * in buffer e va da console_or_gfx_text: così la colonna (per la PRINT a virgola,
             * gmb_print_tab_zone) la traccia la stessa funzione di ogni altra stampa e l'ordine
             * rispetto allo staging per-statement resta corretto. */
            const char* tn = gmb_obj_type(v);
            if (tn && strcmp(tn, "gmb_list") == 0) {
                gmb_list_t l = gmb_val_as_list(v);
                gmb_int_t n = gmb_list_count(l);
                char* ob = NULL; size_t oc = 0, ol = 0;
                print_buf_add(&ob, &oc, &ol, "[");
                for (gmb_int_t i = 0; i < n; i++) {
                    if (i) print_buf_add(&ob, &oc, &ol, ", ");
                    gmb_val_t e = gmb_list_get(l, i);
                    char ebuf[64];
                    if (e.kind == GMB_TYPE_DOUBLE) {
                        double d = e.v_double;
                        if (d == (double)(long long)d) snprintf(ebuf, sizeof(ebuf), "%lld", (long long)d);
                        else if (d == 0.0) d = 0.0;   /* -0.0 -> "0" (parita con .NET/sample) */
snprintf(ebuf, sizeof(ebuf), "%.15g", d);
                        print_buf_add(&ob, &oc, &ol, ebuf);
                    } else if (e.kind == GMB_TYPE_INT) {
                        snprintf(ebuf, sizeof(ebuf), "%lld", (long long)e.v_int);
                        print_buf_add(&ob, &oc, &ol, ebuf);
                    } else if (e.kind == GMB_TYPE_BOOL) {
                        print_buf_add(&ob, &oc, &ol, e.v_bool ? "-1" : "0");
                    } else if (e.kind == GMB_TYPE_STRING && e.v_string.data) {
                        print_buf_add(&ob, &oc, &ol, e.v_string.data);
                    } else { print_buf_add(&ob, &oc, &ol, "0"); }
                }
                print_buf_add(&ob, &oc, &ol, "]");
                if (ob) { console_or_gfx_text(ob); free(ob); }
                break;
            }
            if (tn && strcmp(tn, "gmb_dict") == 0) {
                /* TUPLE (dict "tuple-shaped" con chiavi "0".."n-1"): resa compatta
                 * "[e0, e1, ...]" (gmb_val_to_string). Dict GENUINO (chiavi nome, es.
                 * RGB/SCREENINFO): "[[k, v], ...]" (gmb_val_to_display) — parità
                 * IL/PCode/VM-C/C99Native. Prima il dict genuino usciva "<object>"
                 * (divergenza su minitests/graphics/collections.bas). */
                gmb_string_t s = gmb_val_to_string(v);
                if (s.data && strcmp(s.data, "<object>") == 0)
                    s = gmb_val_to_display(v);
                if (s.data) console_or_gfx_text(s.data);
                break;
            }
            console_or_gfx_text("<object>"); break;
        }
        /* NIL (GMB_TYPE_NIL o kind non riconosciuto): stampa vuota — parità IL/PCode/VM-C
         * (IlRuntime.PrintFormatValueCore: null => ""), non un placeholder di debug.
         * Bug trovato risolvendo NullConditionalAccess-Nullable-UDT-Divergenza: `c?.Campo`
         * su base NIL produce un gmb_val_nil() che PRIMA stampava il letterale "<nil>". */
        default: break;
    }
}

void gmb_print_newline(void) {
    if (s_stage_active) { console_or_gfx_text("\n"); g_print_col = 0; return; }
    g_print_col = 0;
    if (!s_force_console && s_tui_on) { tui_screen_put("\n"); return; }
    if (!s_force_console && GMB_GFX_SCREEN_ON()) { GMB_GFX_PRINT("\n"); return; }
    putchar('\n');
    fflush(stdout);
}

void gmb_print_tab_zone(void) {
    /* Parity oracolo (IlRuntime.PrintTabZone / VM-C OP_PRINT_TAB_ZONE): la virgola di
     * PRINT allinea alla zona di 14 caratteri successiva. */
    int padding = 14 - (g_print_col % 14);
    if (padding <= 0) padding = 14;
    char buf[16];
    memset(buf, ' ', (size_t)padding);
    buf[padding] = '\0';
    console_or_gfx_text(buf);
}

void gmb_cls(void) {
    if (s_tui_on) {
        gmb_clipper_state_t* st = gmb_clipper_global_state();
        if (st && st->buffer) gmb_screen_clear(st->buffer, st->fg, st->bg);
        s_tui_row = 1; s_tui_col = 1;
        gmb_tui_screen_flush();
        return;
    }
    if (GMB_GFX_SCREEN_ON()) { GMB_GFX_CLS(-1); return; }
    /* Mai inquinare stdout rediretto (harness/oracoli): escape solo su tty.
     * Parità C# headless (console service reale: niente byte su stdout catturato). */
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(1) == 0) return;
#else
    if (isatty(STDOUT_FILENO) == 0) return;
#endif
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

/* CLS [colore] (parità C# ClsDispatch/CLS_GPM): schermo grafico attivo → riempie la
 * pagina col colore (indice palette o RGB impacchettato); console → sfondo + clear. */
void gmb_cls_color(int color) {
    if (s_tui_on) { gmb_cls(); return; }
    if (GMB_GFX_SCREEN_ON()) { GMB_GFX_CLS(color); return; }
    gmb_set_color(-1, color);
    gmb_cls();
}

void gmb_locate(int row, int col) {
    if (!s_force_console && s_tui_on) { s_tui_row = row; s_tui_col = col; if (s_tui_row < 1) s_tui_row = 1; if (s_tui_col < 1) s_tui_col = 1; return; }
    if (GMB_GFX_SCREEN_ON()) { GMB_GFX_LOCATE(col, row); return; }
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(1) == 0) return;
#else
    if (isatty(STDOUT_FILENO) == 0) return;
#endif
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}

// Mappa un colore BASIC (0-15) al codice ANSI 30-37/90-97 (fg) o 40-47/100-107 (bg).
// -1 = "non specificato" (non emette la componente).
static int gmb_ansi_color(int c, int is_bg) {
    int base = is_bg ? 40 : 30;
    if (c < 8) return base + c;
    if (c < 16) return base + 60 + (c - 8); // bright → 90-97 / 100-107
    return base + 7;
}

// COLOR fg, bg (uno o entrambi -1 = non specificato). Resetta con gmb_color_reset().
void gmb_set_color(int fg, int bg) {
    if (GMB_GFX_SCREEN_ON()) { GMB_GFX_COLOR(fg, bg); return; }
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(1) == 0) return;
#else
    if (isatty(STDOUT_FILENO) == 0) return;
#endif
    printf("\033[0m");
    if (fg >= 0) printf("\033[%dm", gmb_ansi_color(fg, 0));
    if (bg >= 0) printf("\033[%dm", gmb_ansi_color(bg, 1));
    fflush(stdout);
}

void gmb_color_reset(void) {
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(1) == 0) return;
#else
    if (isatty(STDOUT_FILENO) == 0) return;
#endif
    printf("\033[0m");
    fflush(stdout);
}

/* INKEY$ (grafica o console): legge prima la typeahead scriptata GMB_TUI_SCRIPT
 * (la harness CI la valorizza con N ESC), poi — se non in modalita' grafica e non
 * scriptata — l'input reale del terminale (termios POSIX / conio Windows). */
gmb_string_t gmb_console_inkey(void)
{
    static int sidx = 0;
    /* 0) typeahead iniettata (CONSOLE.SENDKEY): prima di script/terminale, come
     * gmb_console_readkey_nb (prima INKEY la ignorava → SENDKEY+INKEY vuoto). */
    {
        int k = gmb_tui_global_typeahead_read();
        if (k > 0 && k <= 126)
        {
            char buf[2] = { (char)k, '\0' };
            return gmb_string_alloc(buf);
        }
    }
    const char* script = gmb_opt_env("GMB_TUI_SCRIPT");
    if (script && script[sidx])
    {
        char buf[2] = { script[sidx++], '\0' };
        return gmb_string_alloc(buf);
    }
    if (!GMB_GFX_SCREEN_ON())
    {
        /* Il terminale NON si legge qui: il feeder (gmb_input_pump) lo immette nella
         * coda eventi globale; INKEY ne è un CONSUMATORE, come i controlli TUI e ON KEY. */
        char kb[8];
        gmb_input_pump();
        if (GMB_GFX_POLL_KEY(kb, (int)sizeof(kb))) return gmb_string_alloc(kb);
        return gmb_string_alloc("");
    }
    /* GRAFICA: la tastiera e' quella della FINESTRA SDL (non il terminale). Pumpa gli
     * eventi (push_sdl_key riempie la coda input) e preleva il tasto dalla coda. */
    if (GMB_GFX_SCREEN_ON())
    {
        if (GMB_GFX_VIDEO_REAL())
        {
            char kb[8];
            GMB_GFX_PRESENT_TICK();
            if (GMB_GFX_POLL_KEY(kb, sizeof(kb)) != 0) return gmb_string_alloc(kb);
            if (GMB_GFX_SHOULD_QUIT()) return gmb_string_alloc("\x1b");
            return gmb_string_alloc("");
        }
        return gmb_string_alloc("\x1b");   /* headless (dummy): come prima */
    }
    return gmb_string_alloc("");
}

gmb_string_t gmb_console_readkey(int timeout_ms)
{
    static int sidx = 0;
    const char* script = gmb_opt_env("GMB_TUI_SCRIPT");
    /* Priorità: typeahead iniettata (CONSOLE.SENDKEY / gmb_tui_inject_keys). */
    {
        int k = gmb_tui_global_typeahead_read();
        if (k > 0 && k <= 126)
        {
            char buf[2] = { (char)k, '\0' };
            return gmb_string_alloc(buf);
        }
    }
    if (script && script[sidx])
    {
        char buf[2] = { script[sidx++], '\0' };
        return gmb_string_alloc(buf);
    }
    /* Modalità TEST/CI (GMB_TUI_SCRIPT impostato dal gate): dopo lo script iniettato
     * non attendere mai un tasto reale — ritorna ESC (i loop `INKEY K$: IF K$=CHR(27)
     * THEN EXIT` escono; i WAITKEY proseguono). Senza questo, WAITKEY con stdin=terminale
     * attende per sempre (hang del gate). */
    if (gmb_opt_env("GMB_TUI_SCRIPT") != NULL) return gmb_string_alloc("\x1b");
    /* GRAFICA: la tastiera e' quella della FINESTRA SDL. Attende un tasto della finestra
     * (timeout_ms=0 → attesa finche' non arriva un tasto o si chiude la finestra). */
    if (GMB_GFX_SCREEN_ON())
    {
        if (GMB_GFX_VIDEO_REAL())
        {
            char kb[8]; int waited = 0;
            for (;;)
            {
                GMB_GFX_PRESENT_TICK();
                if (GMB_GFX_POLL_KEY(kb, sizeof(kb)) != 0) return gmb_string_alloc(kb);
                if (GMB_GFX_SHOULD_QUIT()) return gmb_string_alloc("\x1b");
                if (timeout_ms > 0 && waited >= timeout_ms) return gmb_string_alloc("");
                usleep(16000); waited += 16;
            }
        }
        return gmb_string_alloc("\x1b");   /* headless (dummy): come prima */
    }
    /* stdin non è un terminale (CI/harness/redirect): niente da leggere → vuoto
     * subito, mai bloccarsi. */
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(0) == 0) return gmb_string_alloc("");
#else
    if (isatty(STDIN_FILENO) == 0) return gmb_string_alloc("");
#endif
    int waited = 0;
    for (;;)
    {
        /* Unico imbuto: il terminale riempie la coda, qui si consuma la coda. */
        char kb[8];
        gmb_input_pump();
        if (GMB_GFX_POLL_KEY(kb, (int)sizeof(kb))) return gmb_string_alloc(kb);
        if (timeout_ms > 0)
        {
            waited += 20;
            if (waited >= timeout_ms) return gmb_string_alloc("");
        }
        usleep(16000);
    }
    return gmb_string_alloc("");
}

/* WAITKEY/GET park-based (VM/C + C99 GPM): lettura NON bloccante di un tasto COMPLETO
 * (typeahead → GMB_TUI_SCRIPT → terminale reale se un evento è già pronto).
 * Ritorno:
 *   1  = un tasto è stato letto e *out lo contiene;
 *   0  = nessun tasto PRONTO ora ma il terminale è reale → il chiamante (task GPM) deve
 *        PARCHEGGIARE e riprovare (mai spin-wait);
 *  -1  = nessun terminale/input definitivamente assente (stdin non-tty, script vuoto in
 *        CI): il chiamante deve trattarlo come stringa vuota FINALE (non riprovare).
 * A differenza di gmb_console_inkey() (solo key ≤126, niente sequenze ESC/CSI/mouse),
 * decodifica un evento completo via gmb_terminal_reader_read_event_core. */
int gmb_console_readkey_nb(gmb_string_t* out)
{
    out->data = NULL; out->length = 0;
    /* 1) typeahead iniettata (CONSOLE.SENDKEY / gmb_tui_inject_keys). */
    {
        int k = gmb_tui_global_typeahead_read();
        if (k > 0 && k <= 126)
        {
            out->data = (char*)malloc(2);
            if (!out->data) return -1;
            out->data[0] = (char)k; out->data[1] = '\0'; out->length = 1;
            return 1;
        }
    }
    /* 2) grafica headless → ESC immediato (mai bloccare). */
    if (GMB_GFX_SCREEN_ON())
    {
        out->data = (char*)malloc(2);
        if (!out->data) return -1;
        out->data[0] = '\x1b'; out->data[1] = '\0'; out->length = 1;
        return 1;
    }
    /* 3) CI: GMB_TUI_SCRIPT impostato ma esaurito → ESC finale (i loop INKEY/WAITKEY
     *    escono). Equivale al ramo `GMB_TUI_SCRIPT != NULL → ESC` del readkey bloccante. */
    if (gmb_opt_env("GMB_TUI_SCRIPT") != NULL)
    {
        out->data = (char*)malloc(2);
        if (!out->data) return -1;
        out->data[0] = '\x1b'; out->data[1] = '\0'; out->length = 1;
        return 1;
    }
    /* 4) stdin non terminale (CI/harness/redirect) → nessun input, mai riprovare. */
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(0) == 0) return -1;
#else
    if (isatty(STDIN_FILENO) == 0) return -1;
#endif
    /* 5) terminale reale: solo se un evento è GIÀ pronto. Usa il RAW input reader
     *    (gmb_raw_input_reader_*): apre /dev/tty e lo mette in modalità raw — il select su
     *    STDIN in modalità canonical NON vede il byte finché non arriva Enter (il tasto
     *    digitato resta nel buffer di riga del tty). ensure_raw va chiamato PRIMA; has_byte
     *    fa solo select non bloccante (niente lseek: su un tty fallisce); se pronto,
     *    read_event_core legge e decodifica la sequenza. */
    gmb_input_pump();
    {
        char kb[8];
        if (GMB_GFX_POLL_KEY(kb, (int)sizeof(kb)))
        {
            size_t len = strlen(kb);
            out->data = (char*)malloc(len + 1);
            if (!out->data) return -1;
            memcpy(out->data, kb, len + 1);
            out->length = len;
            return 1;
        }
    }
    /* Niente in coda: con un tty reale il chiamante parcheggia e riprova (0);
     * senza terminale l'input è definitivamente assente (-1). */
    return gmb_input_terminal_available() ? 0 : -1;
}

/* CURSOR ON/OFF: ANSI show/hide cursor. In grafica il cursore è pixel-based
 * (nessun flag visibilità) → no-op. Su stdout redirect (test/harness) nessun byte
 * (parity PCode headless + oracoli MiniTest). */
void gmb_console_cursor(int on)
{
    if (GMB_GFX_SCREEN_ON()) return;
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(1) == 0) return;
#else
    if (isatty(STDOUT_FILENO) == 0) return;
#endif
    fputs(on ? "\033[?25h" : "\033[?25l", stdout);
    fflush(stdout);
}

/* INPUT/LINE INPUT: legge una riga dal terminale (o dalla typeahead scriptata
 * GMB_TUI_SCRIPT). Accumula i caratteri finché ENTER o EOF. In CI lo script fornisce
 * la riga completa (terminata da '\n' o N ESC). Ritorna la riga letta (senza \n). */
gmb_string_t gmb_console_readline(void)
{
    char buf[1024];
    int n = 0;
    static int sidx = 0;
    const char* script = gmb_opt_env("GMB_TUI_SCRIPT");
    /* Priorità: typeahead iniettata (CONSOLE.SENDKEY / gmb_tui_inject_keys). */
    {
        int k = gmb_tui_global_typeahead_read();
        if (k > 0 && k <= 126)
        {
            while (k != 13 && k != 10 && k != 0 && n < 1023)
            {
                if (k == 8) { if (n > 0) n--; }
                else buf[n++] = (char)k;
                k = gmb_tui_global_typeahead_read();
            }
            buf[n] = '\0';
            return gmb_string_alloc(buf);
        }
    }
    if (script)
    {
        while (script[sidx] && script[sidx] != '\n' && script[sidx] != '\x1b' && n < 1023)
        {
            buf[n++] = script[sidx++];
        }
        if (script[sidx] == '\n' || script[sidx] == '\x1b') sidx++;
        buf[n] = '\0';
        return gmb_string_alloc(buf);
    }
    if (GMB_GFX_SCREEN_ON()) return gmb_string_alloc("");
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(0) == 0) return gmb_string_alloc("");
#else
    if (isatty(STDIN_FILENO) == 0) return gmb_string_alloc("");
#endif
    gmb_ui_event_t ev;
    int waited = 0;
    for (;;)
    {
        if (gmb_terminal_reader_is_event_available_timeout(20))
        {
            if (gmb_terminal_reader_read_event_core(NULL, &ev) == 1 && ev.type == GMB_UIEVENT_KEY)
            {
                if (ev.key == GMB_K_ENTER || ev.key == 13 || ev.key == 10) break;
                if (ev.key == GMB_K_BS && n > 0) { n--; continue; }
                if (ev.key > 0 && ev.key <= 126 && n < 1023) buf[n++] = (char)ev.key;
            }
        }
        waited += 20;
        if (waited > 30000) break; /* timeout 30s di sicurezza */
    }
    buf[n] = '\0';
    return gmb_string_alloc(buf);
}

/* CURSOR SHAPE: BLOCK/UNDERLINE/BAR → DECSCUSR (ANSI). */
void gmb_console_cursor_shape(const char* shape)
{
    if (!shape) return;
#if defined(_WIN32) || defined(_WIN64)
    if (_isatty(1) == 0) return;
#else
    if (isatty(STDOUT_FILENO) == 0) return;
#endif
    if (strcmp(shape, "BLOCK") == 0) { fputs("\033[2 q", stdout); }
    else if (strcmp(shape, "UNDERLINE") == 0) { fputs("\033[4 q", stdout); }
    else if (strcmp(shape, "BAR") == 0) { fputs("\033[6 q", stdout); }
    else {
        /* Forma numerica (parità IL CursorSize/DECSCUSR): "0".."6" → "<n> q". */
        char* end = NULL;
        long n = strtol(shape, &end, 10);
        if (end != shape && *end == '\0' && n >= 0 && n <= 6) {
            char seq[16];
            snprintf(seq, sizeof(seq), "\033[%ld q", n);
            fputs(seq, stdout);
        }
    }
    fflush(stdout);
}

/* CLIPBOARD (CLIP GET/SET/CLEAR, gruppo Console): appunti IN-MEMORY condivisi,
 * deterministici e headless-safe — unica implementazione per VM/C, C99Native
 * (§0bis), allineata al ClipboardService in-memory di IL/PCode. Prima: pbcopy/pbpaste su
 * macOS (dipendente dal WindowServer/ambiente → le gambe native divergevano da IL e fra
 * loro: CLIP.GET dopo CLIP.SET era "" in VM-C/C99Native). */
static char g_clipboard_buf[65536];
gmb_string_t gmb_clipboard_get(void) { return gmb_string_alloc(g_clipboard_buf); }
void gmb_clipboard_set(const char* text) { if (!text) text = ""; snprintf(g_clipboard_buf, sizeof(g_clipboard_buf), "%s", text); }
void gmb_clipboard_clear(void) { g_clipboard_buf[0] = '\0'; }

/* Variante a buffer di gmb_clipboard_get (per i consumer P/Invoke, es. CGfxClipboardService):
 * scrive fino a maxlen-1 byte in out, ritorna la lunghezza (0 se vuoto). Stessa clipboard
 * di sistema delle altre gambe (§0bis). */
int gmb_clipboard_get_buf(char* out, int maxlen)
{
    gmb_string_t s = gmb_clipboard_get();
    int n = (s.data) ? (int)strlen(s.data) : 0;
    if (n > maxlen - 1) n = maxlen - 1;
    if (out && maxlen > 0) { if (n > 0) memcpy(out, s.data, (size_t)n); out[n] = '\0'; }
    gmb_string_free(&s);
    return n;
}

/* CONSOLE.CAPTION: OSC 0 titolo terminale (portabile ANSI); Windows usa
 * SetConsoleTitleW quando la console è reale. */
void gmb_console_set_title(const char* title)
{
#if defined(_WIN32) || defined(_WIN64)
    if (title)
    {
        wchar_t wbuf[512];
        int n = MultiByteToWideChar(CP_UTF8, 0, title, -1, wbuf, 512);
        if (n > 0) SetConsoleTitleW(wbuf);
    }
#else
    /* Mai inquinare stdout rediretto (harness/oracoli): l'escape OSC esce solo su tty.
     * Allinea C99Native/VM-C al gestito (IL/PCode: nessun byte su stdout). */
    if (title)
    {
        if (isatty(STDOUT_FILENO) == 0) return;
        printf("\033]0;%s\007", title);
        fflush(stdout);
    }
#endif
}

/* SAVE/RESTORE SCREEN: alternate screen buffer ANSI (portabile VT). */
void gmb_console_save_screen(void)
{
    fputs("\033[?47h", stdout);
    fflush(stdout);
}

void gmb_console_restore_screen(void)
{
    fputs("\033[?47l", stdout);
    fflush(stdout);
}

/* SAVE/RESTORE SCREEN con nome (parità C# SaveScreenBuffer headless): 16 slot statici
 * {label, snapshot}. Save cattura il buffer modale (se mai creato, altrimenti snapshot
 * vuoto); restore ricopia i caratteri se le dimensioni combaciano. Headless-safe: mai
 * output, mai hang, mai crash su nomi assenti o store pieno (no-op). Colori non
 * preservati (solo codepoint): follow-up documentato. */
#define GMB_SAVED_SCREENS_MAX 16
static struct { char label[64]; gmb_screen_snapshot_t snap; int used; } g_saved_screens[16];

static int saved_screen_slot(const char* name, int create)
{
    int free_slot = -1;
    for (int i = 0; i < GMB_SAVED_SCREENS_MAX; i++) {
        if (g_saved_screens[i].used) {
            if (name && strcmp(g_saved_screens[i].label, name) == 0) return i;
        } else if (free_slot < 0 && create) free_slot = i;
    }
    return free_slot;
}

void gmb_console_savescreen_named(const char* name)
{
    if (!name || !*name) return;
    int slot = saved_screen_slot(name, 1);
    if (slot < 0) return;
    gmb_screen_snapshot_free(&g_saved_screens[slot].snap);
    memset(&g_saved_screens[slot].snap, 0, sizeof(g_saved_screens[slot].snap));
    gmb_screen_buffer_t* buf = gmb_tui_modal_buffer();
    if (buf) gmb_screen_snapshot_capture(name, buf, &g_saved_screens[slot].snap);
    snprintf(g_saved_screens[slot].label, sizeof(g_saved_screens[slot].label), "%s", name);
    g_saved_screens[slot].used = 1;
}

void gmb_console_restorescreen_named(const char* name)
{
    if (!name || !*name) return;
    int slot = saved_screen_slot(name, 0);
    if (slot < 0) return;
    gmb_screen_snapshot_t* s = &g_saved_screens[slot].snap;
    gmb_screen_buffer_t* buf = gmb_tui_modal_buffer();
    if (!s->chars || !buf) return;
    if (gmb_screen_width(buf) != s->width || gmb_screen_height(buf) != s->height) return;
    gmb_screen_cell_t* cells = gmb_screen_cells(buf);
    if (!cells) return;
    for (int i = 0; i < s->width * s->height; i++) cells[i].character = s->chars[i];
}

/* SAVE/RESTORE SCREEN polimorfico: rispecchia l'ordine del C# SaveScreenBuffer
 * (finestre/TUI → grafica → console). Con lo schermo grafico attivo scrive/legge un
 * PNG su disco (gmb_gfx_save_screen_png/restore), come CGfxGraphicsService.SaveScreen;
 * altrimenti usa lo snapshot nominato del buffer modale/console. */
void gmb_console_savescreen_dispatch(const char* name)
{
    if (!name || !*name) return;
    /* Schermo grafico = PNG della pagina visibile (parita C# SaveScreen), anche con buffer TUI presente. */
    if (GMB_GFX_SCREEN_ON()) { (void)GMB_GFX_SAVE_PNG(name); return; }
    if (gmb_tui_modal_buffer()) { gmb_console_savescreen_named(name); return; }
    gmb_console_savescreen_named(name);
}

void gmb_console_restorescreen_dispatch(const char* name)
{
    if (!name || !*name) return;
    if (gmb_tui_modal_buffer()) { gmb_console_restorescreen_named(name); return; }
    if (GMB_GFX_SCREEN_ON()) { (void)GMB_GFX_RESTORE_PNG(name); return; }
    gmb_console_restorescreen_named(name);
}

/* SET <attr> <value> (SET_CLIPPER, subCode 0-16): opzioni di sessione. Lo stato
 * è mantenuto in static (le opzioni BELL/CONFIRM/CENTURY/THEME/DATEFORMAT/
 * WRAP/ESCAPE/...). SubCode allineato a Compiler.Console.VisitSetClipperStatement. */
static int g_clipper_opt_bell = 1;
static int g_clipper_opt_confirm = 1;
static int g_clipper_opt_cursor = 1;
static int g_clipper_opt_century = 0;
static int g_clipper_opt_wrap = 1;
static int g_clipper_opt_escape = 1;
static int g_clipper_opt_intensity = 1;
static int g_clipper_opt_fixed = 0;
static int g_clipper_opt_decimals = 2;
static const char* g_clipper_theme = "";
static const char* g_clipper_dateformat = "";

/* CONSOLE.SENDKEY "testo": inietta la stringa nella typeahead TUI globale; i
 * successivi INKEY/INPUT/LINE INPUT la leggono (priorità sulla tastiera reale). */
void gmb_console_sendkey(const char* keys)
{
    if (keys && *keys) gmb_tui_inject_keys(keys);
}

void gmb_console_set_clipper(int subCode, double value, const char* sval)
{
    int v = (int)value;
    switch (subCode)
    {
        case 0: g_clipper_opt_bell = v; break;
        case 1: g_clipper_opt_confirm = v; break;
        case 2: g_clipper_opt_cursor = v; break;
        case 3: g_clipper_opt_century = v; break;
        case 4:
            /* SET THEME TO "nome": il tema è UNO (registro gmb_ui_*); lo applica qui il
             * runtime condiviso. Prima il nome finiva in una variabile mai letta. */
            if (sval && *sval) gmb_ui_set_theme_name(sval);
            g_clipper_theme = sval ? sval : "";
            break;
        case 5: g_clipper_dateformat = sval ? sval : ""; break;
        case 6: g_clipper_opt_wrap = v; break;
        case 7: g_clipper_opt_escape = v; break;
        case 10: g_clipper_opt_intensity = v; break;
        case 12: g_clipper_opt_decimals = v; break;
        case 13: g_clipper_opt_fixed = v; break;
        case 21: gmb_shell_set_enabled(v); break;   /* SET SHELL ON|OFF → gmb_shell.c */
        default: break;
    }
}

static int g_cmd_argc = 0;
static char** g_cmd_argv = NULL;
void gmb_set_command_args(int argc, char** argv) { g_cmd_argc = argc; g_cmd_argv = argv; }
int gmb_command_count(void) { return g_cmd_argc > 0 ? g_cmd_argc - 1 : 0; }
const char* gmb_command_arg(int i) { if (i >= 1 && i < g_cmd_argc && g_cmd_argv) return g_cmd_argv[i]; return ""; }
/* argv[0]: percorso del PROGRAMMA in esecuzione (usato da BINPATH() per la cartella
 * dell'eseguibile). Vuoto per un embedder senza argv. */
const char* gmb_command_program(void) { return (g_cmd_argc > 0 && g_cmd_argv && g_cmd_argv[0]) ? g_cmd_argv[0] : ""; }

/* ENV$(nome) / ENV.HAS(nome): lettura ESPLICITA dell'ambiente da parte del programma.
 * Il motore continua a usare il registro opzioni (AGENTS); qui non si imposta nulla. */
const char* gmb_env_get(const char* name)
{
    if (!name || !*name) return "";
    const char* v = getenv(name);
    return v ? v : "";
}
int gmb_env_has(const char* name)
{
    return (name && *name && getenv(name)) ? 1 : 0;
}

int64_t gmb_ticks_ms(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (int64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* ── §0ter: guardia delle attese BLOCCANTI e park cooperativo ──
 * Tre superfici registrate dai backend (inversione di controllo, lo stesso schema di
 * GpmWorkerGuard/MarkDriverThread del backend IL):
 *   "sto su un worker GPM?"  — C99: scheduler attivo + task corrente; VM/C: task TLS con
 *                              scheduler. Senza query il controllo è un no-op (oracoli,
 *                              selfcheck, runner senza scheduler).
 *   "sai parcheggiare?"      — C99: gmb_yield_ticks (coroutine). gmb_gpm_sleep la usa.
 *   "parcheggi al syscall?"  — VM/C: nessuna coroutine per il frame C di un syscall; il park
 *                              è quello del syscall (sleep_until_ms + timer, il task riprende
 *                              dopo). Da lì gmb_gpm_sleep NON è utilizzabile: le attese del
 *                              runtime condiviso sono ASINCRONE (gmb_audio_sound_async /
 *                              gmb_audio_mml_async + park del chiamante). Dichiararlo rende
 *                              fatale ogni attesa reale su un worker: la guardia vale anche lì.
 * Perché la guardia: un'attesa reale su un worker lo tiene occupato per tutta la durata e
 * azzera la cooperazione — invisibile nei test, letale con GOMAXPROCS piccolo. */
static gmb_on_gpm_worker_fn g_on_worker_query = NULL;
static gmb_gpm_park_fn g_park_hook = NULL;
static int g_park_at_syscall = 0;
void gmb_gpm_set_on_worker_query(gmb_on_gpm_worker_fn fn) { g_on_worker_query = fn; }
int  gmb_gpm_on_worker(void) { return g_on_worker_query ? g_on_worker_query() : 0; }
void gmb_gpm_set_park_hook(gmb_gpm_park_fn fn) { g_park_hook = fn; }
int  gmb_gpm_park_available(void) { return g_park_hook != NULL; }
void gmb_gpm_set_park_at_syscall(int on) { g_park_at_syscall = on != 0; }
int  gmb_gpm_park_at_syscall(void) { return g_park_at_syscall; }

static void gmb_gpm_blocking_fail(const char* who)
{
    fprintf(stderr,
        "[GMBASIC] FATAL (§0ter): attesa BLOCCANTE da un worker GPM (%s).\n"
        "[GMBASIC]   Un worker non si blocca MAI: l'attesa va PARCHEGGIATA (gmb_gpm_sleep):\n"
        "[GMBASIC]   IL: IlTimerAwait  |  PCode: wait Timer  |  C99: gmb_yield_ticks\n"
        "[GMBASIC]   VM/C: sleep_until_ms + GMB_TS_BLOCKED (al confine del bytecode)\n"
        "[GMBASIC]   gmb_sleep_ms diretto (invece di gmb_gpm_sleep) tiene il worker occupato\n"
        "[GMBASIC]   per tutta l'attesa e falsa cadenza, fairness e GOMAXPROCS.\n",
        who ? who : "?");
    fflush(stderr);
    _Exit(2);
}

/* SLEEP cooperativo, UNICA attesa ammessa dentro un task: se il backend sa parcheggiare lo
 * fa (il worker torna subito libero), altrimenti attende davvero (main/lineare, o backend
 * senza park — VM/C, dove SOUND/MML devono comunque aspettare la fine del suono). */
void gmb_gpm_sleep(int ms)
{
    if (ms <= 0) return;
    if (g_park_hook) { g_park_hook(ms); return; }
    gmb_sleep_ms(ms);
}

/* §0ter — GUARDIA DELL'ATTESA. Vive qui, dentro l'unica attesa che può arrivare da un task
 * (gmb_sleep_ms): NON è un'API del runtime. I punti d'attesa scritti a mano non esistono più — i
 * task dormono col park (gmb_task_park_ms) e i thread del runtime HTTP/WS/MCP attendono sull'evento
 * del task (gmb_sched_wait_task_event) — quindi non esiste un secondo chiamante da servire, e un
 * simbolo in più nel runtime condiviso per un "domani" ipotetico sarebbe un'API morta. Se nasce un
 * nuovo punto d'attesa raggiungibile da un task, si ri-estrae con quel chiamante in mano.
 * Su un worker GPM con una strategia di park dichiarata (coroutine o confine del syscall) il
 * processo si ferma con la causa, invece di tenere occupato il worker in modo invisibile.
 * NON si chiama dai thread dedicati del runtime (driver host 60 Hz, pump dei messaggi, handler
 * HTTP, attesa del tool MCP): lì nessun task gira mai, sarebbe peso morto. */
static void wait_guard(const char* who)
{
    if (gmb_gpm_on_worker() && (gmb_gpm_park_available() || g_park_at_syscall))
        gmb_gpm_blocking_fail(who);
}

void gmb_sleep_ms(int ms) {
    /* §0ter — guardia su TUTTE le attese di questa funzione, per entrambe le strategie di park:
     * coroutine (C99) e confine del syscall (VM/C). Un'attesa reale su un worker è sempre un
     * errore: si arriva qui perché il chiamante ha usato gmb_sleep_ms invece del park
     * (gmb_gpm_sleep, o un'attesa asincrona + park del syscall). */
    if (ms > 0) wait_guard("gmb_sleep_ms dentro un task");
    /* Pompa il present SOLO se siamo sul thread UI (gmb_tui_present_gfx lo verifica):
     * sui worker GPM è no-op, quindi nessuna violazione (SDL solo sul main). */
    extern void gmb_tui_present_gfx(void);
    extern void gmb_msg_pump_all(void);
    int remaining = ms;
    while (remaining > 0) {
        int step = remaining > 8 ? 8 : remaining;
        gmb_tui_present_gfx();
        gmb_msg_pump_all();
        if (GMB_GFX_SHOULD_QUIT()) exit(0);
#if defined(_WIN32) || defined(_WIN64)
        Sleep(step);
#else
        struct timespec ts;
        ts.tv_sec = step / 1000;
        ts.tv_nsec = (step % 1000) * 1000000;
        nanosleep(&ts, NULL);
#endif
        remaining -= step;
    }
}

/* ---- 100% parity IConsole/SdlConsole (SdlConsole.cs) — primitive mancanti ---- */
void gmb_console_beep(void)
{
    printf("\a");
    fflush(stdout);
}
static int g_console_buf_w = 80, g_console_buf_h = 24;
int gmb_console_buffer_width(void)
{
    return g_console_buf_w;
}
int gmb_console_buffer_height(void)
{
    return g_console_buf_h;
}
void gmb_console_set_cursor_position(int x, int y)
{
    if (x < 1) x = 1;
    if (y < 1) y = 1;
    printf("\033[%d;%dH", y, x);
    fflush(stdout);
}
void gmb_console_reset_color(void)
{
    printf("\033[0m");
    fflush(stdout);
}

/* ---- 100% parity IConsole streams (IConsole.cs:74-107 In/Out/Error + Set*) ----
 * TextReader/TextWriter come struct di callback; default = stdin/stdout/stderr. */

static int stream_default_in_read_line(void* ctx, char* out, int cap)
{
    (void)ctx;
    if (!out || cap <= 0) return 0;
    if (!fgets(out, cap, stdin)) return 0;
    size_t l = strlen(out);
    while (l > 0 && (out[l-1] == '\n' || out[l-1] == '\r')) out[--l] = '\0';
    return (int)l;
}
static int stream_default_in_read_char(void* ctx)
{
    (void)ctx;
    int c = getchar();
    return c == EOF ? -1 : c;
}
static void stream_default_out_write(void* ctx, const char* s, int len)
{
    (void)ctx;
    fwrite(s, 1, (size_t)len, stdout);
}
static void stream_default_out_flush(void* ctx)
{
    (void)ctx;
    fflush(stdout);
}
static void stream_default_err_write(void* ctx, const char* s, int len)
{
    (void)ctx;
    fwrite(s, 1, (size_t)len, stderr);
}
static void stream_default_err_flush(void* ctx)
{
    (void)ctx;
    fflush(stderr);
}

static gmb_text_reader_t g_console_in = { stream_default_in_read_line, stream_default_in_read_char, NULL };
static gmb_text_writer_t g_console_out = { stream_default_out_write, stream_default_out_flush, NULL };
static gmb_text_writer_t g_console_err = { stream_default_err_write, stream_default_err_flush, NULL };

void gmb_console_set_in(gmb_text_reader_t r) { g_console_in = r; }
void gmb_console_set_out(gmb_text_writer_t w) { g_console_out = w; }
void gmb_console_set_error(gmb_text_writer_t w) { g_console_err = w; }

int gmb_console_in_read_line(char* out, int cap)
{
    if (!g_console_in.read_line) return 0;
    return g_console_in.read_line(g_console_in.ctx, out, cap);
}
int gmb_console_in_read_char(void)
{
    if (!g_console_in.read_char) return -1;
    return g_console_in.read_char(g_console_in.ctx);
}
int gmb_console_out_write(const char* s)
{
    if (!s || !g_console_out.write) return 0;
    int len = (int)strlen(s);
    g_console_out.write(g_console_out.ctx, s, len);
    return len;
}
void gmb_console_out_write_line(const char* s)
{
    gmb_console_out_write(s);
    gmb_console_out_write("\n");
}
void gmb_console_out_flush(void)
{
    if (g_console_out.flush) g_console_out.flush(g_console_out.ctx);
}
int gmb_console_err_write(const char* s)
{
    if (!s || !g_console_err.write) return 0;
    int len = (int)strlen(s);
    g_console_err.write(g_console_err.ctx, s, len);
    return len;
}
void gmb_console_err_flush(void)
{
    if (g_console_err.flush) g_console_err.flush(g_console_err.ctx);
}

/* ---- 100% parity IConsole state settable (IConsole.cs:111-144) ---- */
static int g_console_win_w = 80, g_console_win_h = 24;
static int g_console_win_l = 0, g_console_win_t = 0;
static int g_console_scroll_top = 0, g_console_scroll_bottom = 0;
static int g_console_dirty = 1;

void gmb_console_set_buffer_size(int w, int h)
{
    if (w > 0) g_console_buf_w = w;
    if (h > 0) g_console_buf_h = h;
    g_console_dirty = 1;
}
void gmb_console_set_window_size(int w, int h)
{
    if (w > 0) g_console_win_w = w;
    if (h > 0) g_console_win_h = h;
    g_console_dirty = 1;
}
void gmb_console_set_window_position(int left, int top)
{
    g_console_win_l = left;
    g_console_win_t = top;
}
void gmb_console_set_scroll_region(int top, int bottom)
{
    g_console_scroll_top = top;
    g_console_scroll_bottom = bottom;
    g_console_dirty = 1;
}
int gmb_console_is_dirty(void) { return g_console_dirty; }
void gmb_console_mark_clean(void) { g_console_dirty = 0; }
int gmb_console_window_left(void) { return g_console_win_l; }
int gmb_console_window_top(void) { return g_console_win_t; }

/* MESSAGE(text?) — getter/setter del testo messaggio (parità IlRuntime.MessageCmd):
 * imposta lo stato e/o ritorna il testo corrente; NON stampa su stdout (era il bug
 * VM/C: gmb_bf_679 stampava e ritornava 0). */
static char g_console_msg[2048] = "";
gmb_string_t gmb_console_message(const char* value)
{
    if (value) {
        strncpy(g_console_msg, value, sizeof(g_console_msg) - 1);
        g_console_msg[sizeof(g_console_msg) - 1] = '\0';
    }
    return gmb_string_alloc(g_console_msg);
}

/* VGA16 palette (parità CommonLib ScreenConfig.Palettes.GetVGA16Palette) + accessor
 * per CONSOLE.GETPALETTECOLOR. */
static const unsigned char g_vga16[16][3] = {
    {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
    {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
};
void gmb_console_palette_color(int idx, int* r, int* g, int* b)
{
    if (idx < 0 || idx > 15) idx = 0;
    if (r) *r = g_vga16[idx][0];
    if (g) *g = g_vga16[idx][1];
    if (b) *b = g_vga16[idx][2];
}

/* Indice VGA16 più vicino a un colore RGB (per le specifiche Clipper). */
static int gmb_vga16_nearest(gmb_cell_color_t c)
{
    if (c.is_default) return -1;
    int best = 7;
    long bd = -1;
    for (int i = 0; i < 16; i++) {
        long dr = (long)g_vga16[i][0] - (long)c.r;
        long dg = (long)g_vga16[i][1] - (long)c.g;
        long db = (long)g_vga16[i][2] - (long)c.b;
        long d = dr * dr + dg * dg + db * db;
        if (bd < 0 || d < bd) { bd = d; best = i; }
    }
    return best;
}

/* COLOR <specifica Clipper> ("W+/B", "N/W", …): risolve fg/bg e applica (VGA16 → ANSI).
 * Il backend C99Native usa questa forma quando il colore è una STRINGA: un cast
 * `(int)(gmb_string_alloc("W+/B"))` è C invalido. Condivide il parser con la TUI
 * (gmb_color_parse_pair), così specifica e resa restano una sola sorgente. */
void gmb_set_color_spec(const char* spec)
{
    gmb_cell_color_t fg = GMB_CELLCOLOR_WHITE;
    gmb_cell_color_t bg = GMB_CELLCOLOR_BLACK;
    if (spec && *spec) gmb_color_parse_pair(spec, &fg, &bg);
    gmb_set_color(gmb_vga16_nearest(fg), gmb_vga16_nearest(bg));
}

/* CONSOLE.GETCURSORPOS / GETTEXTMETRICS / GETPALETTECOLOR — ritornano gmb_list_t
 * (parità headless: [0,0] / [80,24] / VGA16[idx]). */
gmb_list_t gmb_console_get_cursor_pos(void)
{
    gmb_list_t l = gmb_list_new();
    gmb_list_add(&l, gmb_make_val_double(0));
    gmb_list_add(&l, gmb_make_val_double(0));
    return l;
}
gmb_list_t gmb_console_get_text_metrics(void)
{
    gmb_list_t l = gmb_list_new();
    gmb_list_add(&l, gmb_make_val_double(80));
    gmb_list_add(&l, gmb_make_val_double(24));
    return l;
}
gmb_list_t gmb_console_get_palette_color(int idx)
{
    gmb_list_t l = gmb_list_new();
    int r = 0, g = 0, b = 0;
    gmb_console_palette_color(idx, &r, &g, &b);
    gmb_list_add(&l, gmb_make_val_double((double)r));
    gmb_list_add(&l, gmb_make_val_double((double)g));
    gmb_list_add(&l, gmb_make_val_double((double)b));
    return l;
}

/* ALERT — UN solo controllo (AlertBox: disegno a celle per SDLConsole/TUI + a pixel per GFX).
 * Con SDLConsole/GFX attivi si mostra nella finestra via gmb_tui_run_control_modal (che
 * pompa/presenta la console); headless → 0 senza render. */
double gmb_console_alert(const char* msg)
{
    if (GMB_GFX_SCREEN_ON() || gmb_tui_sdl_active())
    {
        int w = (int)(msg ? strlen(msg) : 0) + 8; if (w < 30) w = 30; if (w > 60) w = 60;
        gmb_control_t* c = gmb_tui_add_alertbox(10, 8, w, 6, msg ? msg : "");
        if (c)
        {
            gmb_alert_box_t* a = (gmb_alert_box_t*)c;
            snprintf(a->title, sizeof(a->title), "Avviso");
            gmb_tui_run_control_modal(c);
        }
        return 0.0;
    }
    (void)msg; return 0.0;
}
double gmb_console_confirm(const char* msg) { if (GMB_GFX_SCREEN_ON()) { int r = gmb_form_dialog("Conferma", msg, "OK", "Annulla", NULL, NULL, "question"); return r == 0 ? 1.0 : 0.0; } (void)msg; return 0.0; }

/* ============================================================================
 * Via UNICA dei controlli modali "console" (MENU, INPUT, ...): crea il controllo del
 * runtime C, lo esegue con gmb_tui_run_control_modal (che si rende su TUI/SDLConsole/
 * grafica secondo il contesto attivo) e ritorna il risultato. Le diramazioni gestite
 * (ConsoleService C#) fanno P/Invoke QUI: un solo comportamento su tutti i backend.
 * ============================================================================ */
long long gmb_console_menu(const char* title, const char* items_csv)
{
    (void)title;
    gmb_tui_add_menu(2, 1, 40, "");
    int idx = gmb_tui_control_count() - 1;
    if (idx < 0) return -1;
    char spec[1024];
    snprintf(spec, sizeof(spec), "Menu:%s", items_csv ? items_csv : "");
    gmb_tui_menu_set(idx, spec);
    gmb_control_t* c = gmb_tui_control_at(idx);
    if (!c) return -1;
    gmb_tui_run_control_modal(c);
    long long r = gmb_tui_menu_poll();
    return r < 0 ? -1 : (r % 100);
}

const char* gmb_console_input(const char* prompt)
{
    (void)prompt;
    gmb_tui_add_textinput(2, 3, 40, "");
    int idx = gmb_tui_control_count() - 1;
    if (idx < 0) return "";
    gmb_control_t* c = gmb_tui_control_at(idx);
    if (!c) return "";
    gmb_tui_run_control_modal(c);
    return gmb_tui_control_text_ptr(idx);
}

/* --- Altri controlli modali "console" (stesso schema: controllo C + modale + risultato) --- */
static gmb_control_t* cc_last(int* out_idx) { int i = gmb_tui_control_count() - 1; if (out_idx) *out_idx = i; return i >= 0 ? gmb_tui_control_at(i) : NULL; }

long long gmb_console_toggle(const char* title, const char* prompt, int initial)
{
    (void)title; (void)initial;
    gmb_tui_add_toggle(2, 3, 40, prompt ? prompt : "");
    int idx; gmb_control_t* c = cc_last(&idx); if (!c) return initial ? 1 : 0;
    gmb_tui_run_control_modal(c);
    return gmb_tui_control_value(idx) ? 1 : 0;
}
double gmb_console_slider(const char* title, double mn, double mx, double initial)
{
    (void)title;
    gmb_tui_add_slider(2, 3, 40, (int)mn, (int)mx, (int)initial);
    int idx; gmb_control_t* c = cc_last(&idx); if (!c) return initial;
    gmb_tui_run_control_modal(c);
    return (double)gmb_tui_control_value(idx);
}
long long gmb_console_spinner(const char* title, const char* items_csv)
{
    (void)title;
    gmb_tui_add_spinner(2, 3, 40, (items_csv && *items_csv) ? items_csv : "dots");
    int idx; gmb_control_t* c = cc_last(&idx); if (!c) return 0;
    gmb_tui_run_control_modal(c);
    return gmb_tui_control_value(idx);
}
const char* gmb_console_filedialog(const char* title, const char* pattern, int save)
{
    (void)save;
    gmb_tui_add_filedialog(title ? title : "File", (pattern && *pattern) ? pattern : "*");
    int idx; gmb_control_t* c = cc_last(&idx); if (!c) return "";
    gmb_tui_run_control_modal(c);
    return gmb_tui_control_text_ptr(idx);
}
const char* gmb_console_colorpicker(const char* title, const char* defaultColor)
{
    (void)title; (void)defaultColor;
    gmb_tui_add_colorpicker(2, 3, 20, 10);
    int idx; gmb_control_t* c = cc_last(&idx); if (!c) return defaultColor ? defaultColor : "";
    gmb_tui_run_control_modal(c);
    return gmb_tui_control_text_ptr(idx);
}
const char* gmb_console_datepicker(const char* title, const char* defaultDate)
{
    (void)title; (void)defaultDate;
    gmb_tui_add_datepicker(2, 3, 20);
    int idx; gmb_control_t* c = cc_last(&idx); if (!c) return defaultDate ? defaultDate : "";
    gmb_tui_run_control_modal(c);
    return gmb_tui_control_text_ptr(idx);
}
void gmb_console_msgbox(const char* title, const char* message)
{
    (void)title;
    gmb_console_alert(message ? message : "");
}

/* ============================================================================
 * Clipper @ SAY — via UNICA controlli (NIENTE rendering proprio: niente
 * gmb_locate/gmb_print). SAY → controllo Label alla (row,col). Coordinate BASIC
 * 1-based → 0-based. Il @GET/READ usa la via unica GIA' esistente (gmb_read_* → TextInput).
 * ============================================================================ */
void gmb_at_say(int row, int col, const char* text)
{
    int w = 80 - (col - 1); if (w < 1) w = 1;
    gmb_tui_add_label(col - 1, row - 1, w, text ? text : "");
}

/* ALERT completo (parità AlertBox.cs.Show): messaggio + pulsanti + align + width + border.
 * Ritorna 1..N (pulsante) o 0 (chiusa). Via unica: controllo AlertBox C. */
int gmb_console_alert_ex(const char* msg, const char* b0, const char* b1, const char* b2,
                         int align, int width, int border,
                         int has_fg, int fr, int fgc, int fb, int has_bg, int br, int bgc, int bb)
{
    gmb_control_t* c = gmb_tui_add_alertbox(10, 8, 40, 6, msg ? msg : "");
    if (!c) return 0;
    gmb_alert_box_t* a = (gmb_alert_box_t*)c;
    gmb_alert_box_centered(a, 80, 24, "", msg, width, align);
    gmb_alert_box_with_border(a, border);
    gmb_alert_box_with_colors_rgb(a, has_fg, fr, fgc, fb, has_bg, br, bgc, bb);
    if (b0 || b1 || b2) gmb_alert_box_with_buttons(a, b0, b1, b2);
    return gmb_alert_box_show(a);
}

/* Eventi mouse/finestra: tabella statica di registrazione (parità IL/PCodeVM).
 * Il loop eventi TUI non esiste ancora nei backend C: le registrazioni sono
 * conservate ma nessun evento viene mai generato. */
#define GMB_MAX_EVENT_HANDLERS 32
static struct { char kind_str[64]; char handler[128]; } g_mouse_handlers[GMB_MAX_EVENT_HANDLERS];
static int g_mouse_n = 0;
static struct { char kind_str[64]; char handler[128]; } g_window_handlers[GMB_MAX_EVENT_HANDLERS];
static int g_window_n = 0;

void gmb_mouse_register_event(const char* event_kind, const char* handler) {
    if (!event_kind || !handler) return;
    for (int i = 0; i < g_mouse_n; i++)
        if (strcmp(g_mouse_handlers[i].kind_str, event_kind) == 0) {
            size_t n = strlen(handler); if (n >= sizeof(g_mouse_handlers[i].handler)) n = sizeof(g_mouse_handlers[i].handler) - 1;
            memcpy(g_mouse_handlers[i].handler, handler, n); g_mouse_handlers[i].handler[n] = 0; return;
        }
    if (g_mouse_n >= GMB_MAX_EVENT_HANDLERS) return;
    size_t kn = strlen(event_kind); if (kn >= sizeof(g_mouse_handlers[g_mouse_n].kind_str)) kn = sizeof(g_mouse_handlers[g_mouse_n].kind_str) - 1;
    memcpy(g_mouse_handlers[g_mouse_n].kind_str, event_kind, kn); g_mouse_handlers[g_mouse_n].kind_str[kn] = 0;
    size_t n = strlen(handler); if (n >= sizeof(g_mouse_handlers[g_mouse_n].handler)) n = sizeof(g_mouse_handlers[g_mouse_n].handler) - 1;
    memcpy(g_mouse_handlers[g_mouse_n].handler, handler, n); g_mouse_handlers[g_mouse_n].handler[n] = 0;
    g_mouse_n++;
}
void gmb_mouse_unregister_event(const char* event_kind) {
    if (!event_kind) return;
    for (int i = 0; i < g_mouse_n; i++)
        if (strcmp(g_mouse_handlers[i].kind_str, event_kind) == 0) {
            g_mouse_handlers[i] = g_mouse_handlers[--g_mouse_n]; return;
        }
}
void gmb_window_register_event(const char* event_kind, const char* handler) {
    if (!event_kind || !handler) return;
    for (int i = 0; i < g_window_n; i++)
        if (strcmp(g_window_handlers[i].kind_str, event_kind) == 0) {
            size_t n = strlen(handler); if (n >= sizeof(g_window_handlers[i].handler)) n = sizeof(g_window_handlers[i].handler) - 1;
            memcpy(g_window_handlers[i].handler, handler, n); g_window_handlers[i].handler[n] = 0; return;
        }
    if (g_window_n >= GMB_MAX_EVENT_HANDLERS) return;
    size_t kn = strlen(event_kind); if (kn >= sizeof(g_window_handlers[g_window_n].kind_str)) kn = sizeof(g_window_handlers[g_window_n].kind_str) - 1;
    memcpy(g_window_handlers[g_window_n].kind_str, event_kind, kn); g_window_handlers[g_window_n].kind_str[kn] = 0;
    size_t n = strlen(handler); if (n >= sizeof(g_window_handlers[g_window_n].handler)) n = sizeof(g_window_handlers[g_window_n].handler) - 1;
    memcpy(g_window_handlers[g_window_n].handler, handler, n); g_window_handlers[g_window_n].handler[n] = 0;
    g_window_n++;
}
void gmb_window_unregister_event(const char* event_kind) {
    if (!event_kind) return;
    for (int i = 0; i < g_window_n; i++)
        if (strcmp(g_window_handlers[i].kind_str, event_kind) == 0) {
            g_window_handlers[i] = g_window_handlers[--g_window_n]; return;
        }
}

/* Eventi finestra per HANDLE, con CALLBACK (function pointer): quelli per NOME non sono
 * invocabili dal runtime C (nessun risolutore nome->funzione). win=0 = finestra SDL. */
static struct { int win; char ev[64]; gmb_form_cb cb; } g_win_cb[GMB_MAX_EVENT_HANDLERS];
static int g_win_cb_n = 0;
/* Contesto dell'evento IN CORSO (parità gmb_form_firing_control per i controlli): i callback
 * C non ricevono argomenti, quindi chi ha bisogno di sapere (finestra, evento) lo legge qui
 * durante l'invocazione. */
static int g_win_firing_win = -1;
static const char* g_win_firing_ev = NULL;
void gmb_window_firing(int* out_win, const char** out_ev) {
    if (out_win) *out_win = g_win_firing_win;
    if (out_ev) *out_ev = g_win_firing_ev;
}
void gmb_window_register_event_of_cb(int win, const char* ev, gmb_form_cb cb) {
    if (!ev) return;
    for (int i = 0; i < g_win_cb_n; i++)
        if (g_win_cb[i].win == win && strcmp(g_win_cb[i].ev, ev) == 0) { g_win_cb[i].cb = cb; return; }
    if (g_win_cb_n >= GMB_MAX_EVENT_HANDLERS) return;
    g_win_cb[g_win_cb_n].win = win;
    size_t n = strlen(ev); if (n >= sizeof(g_win_cb[0].ev)) n = sizeof(g_win_cb[0].ev) - 1;
    memcpy(g_win_cb[g_win_cb_n].ev, ev, n); g_win_cb[g_win_cb_n].ev[n] = 0;
    g_win_cb[g_win_cb_n].cb = cb; g_win_cb_n++;
}
void gmb_window_fire(int win, const char* ev) {
    if (!ev) return;
    int prev_win = g_win_firing_win;
    const char* prev_ev = g_win_firing_ev;
    g_win_firing_win = win; g_win_firing_ev = ev;
    /* win < 0 = registrazione SENZA `OF` (ON WINDOW <ev> <azione>): vale per ogni finestra. */
    for (int i = 0; i < g_win_cb_n; i++)
        if ((g_win_cb[i].win == win || g_win_cb[i].win < 0) &&
            strcmp(g_win_cb[i].ev, ev) == 0 && g_win_cb[i].cb) g_win_cb[i].cb();
    g_win_firing_win = prev_win; g_win_firing_ev = prev_ev;
}
void gmb_window_reset_events(void) { g_win_cb_n = 0; }

/* ===== Console mode (CONSOLE n): applica consoleConfiguration[n] e ridisegna ===== */
static int g_console_mode = -1;
static int g_console_env_applied = 0;

int gmb_console_mode_get(void)
{
    if (g_console_mode < 0) {
        const char* e = gmb_opt_env("GMB_CONSOLE");
        g_console_mode = (e && *e) ? atoi(e) : 0;
    }
    return g_console_mode;
}

/* Applica UNA volta la modalita' console da GMB_CONSOLE (flag CLI -c/-s/-c64).
 * Chiamata da gmb_tui_screen_enable: solo se la variabile e' presente. */
void gmb_console_apply_env_once(void)
{
    if (g_console_env_applied) return;
    g_console_env_applied = 1;
    const char* e = gmb_opt_env("GMB_CONSOLE");
    if (!e || !*e) return;
    gmb_console_select(atoi(e));
}

void gmb_console_redraw(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (st && st->buffer) {
        gmb_theme_t th = gmb_ui_theme();
        gmb_cell_color_t dfg = (th.head_fg.r || th.head_fg.g || th.head_fg.b) ? th.head_fg : th.text;
        st->fg = dfg; st->bg = th.desktop_bg;
        gmb_theme_paint_desktop(st->buffer);
    }
    { extern void gmb_winstmt_render_all(void); GMB_GFX_WINSTM_RENDER_ALL(); }
    gmb_tui_screen_flush();
}

void gmb_console_select(int n)
{
    if (n < 0) n = 0;
    g_console_mode = n;
    int gw = 0, gh = 0, fs = 0;
    char fp[256] = {0};
    gmb_settings_console_int(n, "charGridWidth", &gw);
    gmb_settings_console_int(n, "charGridHeight", &gh);
    gmb_settings_console_str(n, "fontPath", fp, (int)sizeof(fp));
    gmb_settings_console_int(n, "fontSize", &fs);
    if (gw > 0 && gh > 0) { gmb_console_set_buffer_size(gw, gh); gmb_console_set_window_size(gw, gh); }
    if (n == 1)   /* C64: palette c64Classic (VICE) + font C64 */
    {
        static const unsigned char c64[16][3] = {
            {0,0,0},{255,255,255},{139,63,53},{122,192,182},{142,70,180},{108,165,80},
            {62,53,150},{206,215,123},{144,95,47},{101,78,30},{183,109,99},{86,86,86},
            {128,128,128},{159,213,131},{140,130,230},{172,172,172} };
        GMB_GFX_INIT();
        for (int i = 0; i < 16; i++) GMB_GFX_PALETTE(i, c64[i][0], c64[i][1], c64[i][2]);
        if (!fp[0])
    {
        char rd[2048];
        /* C64 Pro Mono (style64) non e' ridistribuibile: la modalita' C64 usa Sixtyfour
         * (SIL OFL 1.1, "fonts based on the Commodore 64"). */
        if (gmb_paths_resources_dir(rd, sizeof rd) && rd[0]) snprintf(fp, sizeof fp, "%s/Sixtyfour.ttf", rd);
        else snprintf(fp, sizeof fp, "Resources/Sixtyfour.ttf");
    }
        if (fs <= 0) fs = 16;
    }
    if (fp[0] && fs > 0) GMB_GFX_FONT(fp, fs);
    /* Ridisegna solo se c'e' una superficie grafica attiva: in headless/ANSI il flush
     * scriverebbe il desktop su stdout (inquinando l'output dei MiniTest). La finestra
     * SDL grafica si ridisegna al prossimo present; senza SDL non c'e' nulla da ridisegnare. */
    if (GMB_GFX_SCREEN_ON()) gmb_console_redraw();
}

/* ===== C64 PETSCII — NEUTRALIZZATO =====
 * Questa mappa esisteva per portare i box-drawing Unicode sui codici PETSCII che il font
 * C64_Pro_Mono-STYLE.otf esponeva ai codepoint Latin-1. Quel font (licenza style64) e' stato
 * rimosso: con un font Unicode e con le cornici disegnate PROCEDURALMENTE dalla geometria
 * della cella (cf_box/cf_box_mask in gmb_gfx_cellfont.c) non c'e' piu' nulla da rimappare.
 * La funzione resta come identita' per non toccare i chiamanti.
 * Usata dal renderer celle quando gmb_console_mode_get()==1 (%C64). */
int gmb_c64_remap(int cp)
{
    return cp;
}

#if 0   /* mappa storica (font C64 Pro Mono), tenuta come riferimento */
int gmb_c64_remap_legacy(int cp)
{
    switch (cp)
    {
    case 0x2554: return 218; case 0x2550: return 196; case 0x2557: return 191;
    case 0x2551: return 179; case 0x255A: return 192; case 0x255D: return 217;
    case 0x2560: return 195; case 0x2563: return 180; case 0x2566: return 194;
    case 0x2569: return 193; case 0x256C: return 197;
    case 0x250C: return 218; case 0x2500: return 196; case 0x2510: return 191;
    case 0x2502: return 179; case 0x2514: return 192; case 0x2518: return 217;
    case 0x251C: return 195; case 0x2524: return 180; case 0x252C: return 194;
    case 0x2534: return 193; case 0x253C: return 197;
    case 0x2588: return 219; case 0x2584: return 220; case 0x258C: return 221;
    case 0x2590: return 222; case 0x2580: return 223;
    default: return cp;
    }
}
#endif

/* VerticalFillChars (porting Atlas.cs): glifi da stirare in verticale per connettere
 * le cornici. Match sul carattere LOGICO (pre-remap), come in C#. */
int gmb_c64_is_vfill(int cp)
{
    switch (cp)
    {
    case 0x2502: case 0x250C: case 0x2510: case 0x2514: case 0x2518:
    case 0x251C: case 0x2524: case 0x252C: case 0x2534: case 0x253C:
    case 0x2551: case 0x2554: case 0x2557: case 0x255A: case 0x255D:
    case 0x2560: case 0x2563: case 0x2566: case 0x2569: case 0x256C:
    case 0x2588: case 0x258C: case 0x2590: case 0x2503:
    case 0x2575: case 0x2579: case 0x2576: case 0x2577: case 0x2578:
    case 0x257A: case 0x257B: case 0x257C: case 0x257D: case 0x257E: case 0x257F:
        return 1;
    default: return 0;
    }
}

/* TICK <fps>: attende il confine del frame. NON presenta — il present lo emette il codice
 * emesso subito dopo (gmb_gfx_present_frame), quindi l'ordine è attesa → present e il frame
 * dopo il risveglio. Prima il rate era ignorato: il loop girava a migliaia di fps e la
 * presentazione risultava instabile (finestra nera nei demo con TICK). */
void gmb_tick_pace(double fps)
{
    /* Limiter a DEADLINE, non `sleep(passo)` dopo il lavoro: dormendo il passo INTERO dopo
     * il frame, il periodo diventava lavoro + passo — misurato con TICK 60: 101 iterazioni
     * in 2001 ms (50 fps, 19,9 ms per frame), cioè il rate dichiarato non veniva rispettato.
     * Qui si attende solo fino al confine del frame, quindi il periodo è il passo; se il
     * lavoro sfora il confine non si accumula ritardo (in ritardo di un passo o più si
     * riparte da adesso, altrimenti si mantiene la cadenza).
     *
     * La scadenza sta sul TASK GPM (`tick_deadline_ms`), non in `__thread`: il task migra tra
     * worker, quindi una scadenza per-thread la perderebbe a ogni migrazione (e due task che si
     * alternano sullo stesso worker si contenderebbero il confine, dettandosi il ritmo a
     * vicenda). Fuori da un task (programma lineare, main thread) resta un `__thread` per
     * thread, così un TICK su un worker e uno sul main thread non si contendono la scadenza.
     * Due cicli TICK consecutivi nello stesso task la condividono (il secondo prosegue la
     * cadenza del primo, al più attende un passo) e dopo una pausa lunga il ramo "in ritardo"
     * riparte da adesso:
     * misurato con `TICK 60` per 400 ms poi `TICK 30` per 600 ms → 42-43 present in 1 s, con un
     * solo intervallo di 37-39 ms al cambio (48 col cap acceso, che rifiuta quel frame). Un
     * reset esplicito al cambio di rate è stato provato e RIMOSSO: spostava quel buco, non lo
     * toglieva.
     *
     * Nesting: due TICK annidati condividono la scadenza, quindi il pace esterno e quello
     * interno si alternano sul confine più vicino — nessun blocco, e il loop interno (più
     * lento) detta il ritmo. Pattern non supportato, ma innocuo. */
    static __thread int64_t main_deadline_ms = 0;   /* fuori dai task: confine del frame; 0 = nessuno */
    gmb_task_t* task = gmb_cur();
    int64_t* deadline_ms = task ? &task->tick_deadline_ms : &main_deadline_ms;
    int64_t wait = gmb_tick_advance(deadline_ms, fps);
    if (wait <= 0) return;                     /* confine già raggiunto: il frame parte subito */
    /* Anticipo: si attende il confine. Dentro un task si PARCHEGGIA la coroutine
     * (gmb_gpm_sleep -> gmb_yield_ticks, §0ter): il worker torna a eseguire gli altri
     * task, l'attesa non lo blocca. Fuori da un task (main/lineare) attesa reale. */
    if (task) { gmb_gpm_sleep((int)wait); }
    else
    {
        /* Main/lineare, non un worker: `gmb_cur()` è NULL qui, e il predicato C99 di §0ter è
         * "scheduler attivo && gmb_cur() != NULL" — quindi `gmb_gpm_on_worker()` è false per
         * costruzione e nemmeno la guardia di gmb_sleep_ms può scattare. L'attesa reale è
         * legittima: nessuna guardia dedicata (l'aveva gmb_tick_sync_wait, con un solo
         * chiamante, ed era un caso impossibile). */
        gmb_sleep_ms((int)wait);
    }
}

/* Formula UNICA del frame limiter (§0bis: C99Native e VM/C la condividono, nessuna copia nei
 * backend). Passo = 1000/fps con lo stesso clamp del canone; la scadenza avanza SEMPRE di un
 * passo (in ritardo di un passo o più si riparte da adesso, altrimenti la cadenza si mantiene). */
long long gmb_tick_advance(int64_t* deadline_ms, double fps)
{
    if (!deadline_ms || fps <= 0.0) return 0;
    double ms = 1000.0 / fps;
    if (ms < 1.0) ms = 1.0;
    if (ms > 1000.0) ms = 1000.0;
    long long step = (long long)(ms + 0.5);
    long long now = gmb_ticks_ms();
    long long wait = 0;
    if (*deadline_ms > now) wait = *deadline_ms - now;                          /* anticipo: si attende il confine */
    else if (*deadline_ms <= 0 || now - *deadline_ms >= step) *deadline_ms = now;  /* primo frame o in ritardo di un passo */
    *deadline_ms += step;
    return wait;
}

/* Frame present per i loop TICK: solo sul thread UI (gmb_tui_present_gfx verifica
 * il main thread e compone WM/form/grafica). Da un worker GPM è no-op: SDL resta
 * sul thread principale. */
void gmb_gfx_present_frame(void)
{
    extern void gmb_tui_present_gfx(void);
    gmb_tui_present_gfx();
}
