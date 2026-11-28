/* ============================================================================
 * gmb_ui_inspector.h — INSPECTOR della UI (albero dei controlli, rect, costo
 * del frame), condiviso TUI + FORM/PIXEL e dalle 4 gambe.
 *
 * Si accende dal registro opzioni, come i flag di accessibilità:
 *
 *     AI SET OPTION "ui.inspector", "1"
 *
 * Il pannello NON è un controllo BASIC (non entra nei registri, non prende il
 * fuoco, non genera eventi): è un overlay che il frame disegna dopo i controlli.
 * La raccolta delle voci è del frame stesso (i due registri — TUI a celle e FORM
 * pixel — hanno handle sovrapposti), quindi il flusso è:
 *
 *     gmb_inspector_reset()
 *     gmb_inspector_add(...)          per ogni controllo del frame
 *     gmb_inspector_draw_cells(buf)   oppure gmb_inspector_draw_pixels()
 *
 * `gmb_inspector_lines()` produce il testo anche senza disegnare: è la parte
 * verificabile in modo deterministico (headless) da un selfcheck.
 * ============================================================================ */
#ifndef GMB_UI_INSPECTOR_H
#define GMB_UI_INSPECTOR_H

#include "gmb_tui.h"   /* gmb_screen_buffer_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Voce del registro opzioni (`AI LIST OPTIONS`). 1 = pannello visibile. */
#define GMB_INSPECTOR_OPT "ui.inspector"

#define GMB_INSPECTOR_MAX      128
#define GMB_INSPECTOR_LINE_MAX 128
#define GMB_INSPECTOR_MAXLINES 40

/* Flag di una voce (bit) — come compaiono nella colonna `flags`. */
enum {
    GMB_INSPECT_VISIBLE  = 1,    /* v */
    GMB_INSPECT_ENABLED  = 2,    /* e */
    GMB_INSPECT_FOCUSED  = 4,    /* f */
    GMB_INSPECT_LAYOUT   = 8,    /* L  (EXPAND/ANCHOR/ALIGN/min/max/grow) */
    GMB_INSPECT_FLOW     = 16,   /* Fl (contenitore WrapBox/Grid) */
    GMB_INSPECT_GROUP    = 32,   /* G  (figlio di un contenitore flow) */
    GMB_INSPECT_ANIMATED = 64    /* a  (ha un'animazione in corso) */
};

/* 1 se `ui.inspector` è acceso. Rilegge il registro quando cambia (come
 * gmb_a11y_get), così vale sia all'avvio sia a runtime. */
int gmb_inspector_enabled(void);

/* Costo del frame: chiamato da chi disegna, una volta per frame. */
void gmb_inspector_note_frame(int frame_ms, int controls);
void gmb_inspector_stats(int* frame_ms, int* controls, int* frames);

/* Raccolta delle voci del frame corrente. */
void gmb_inspector_reset(void);
int  gmb_inspector_add(const char* name, const char* kind,
                       int x, int y, int w, int h, int flags, int cols);
int  gmb_inspector_count(void);

/* Testo del pannello (righe separate da '\n'). Ritorna i byte scritti. */
int gmb_inspector_lines(char* out, int cap);
/* Spezza il testo in righe; ritorna quante ne ha scritte in `lines`. */
int gmb_inspector_split(char lines[][GMB_INSPECTOR_LINE_MAX], int max);

/* Nome breve di un GMB_CK_* (per la colonna `kind`). */
const char* gmb_inspector_kind_name(int ck);

/* Disegno del pannello: TUI a celle o percorso grafico (pixel). */
void gmb_inspector_draw_cells(gmb_screen_buffer_t* b);
void gmb_inspector_draw_pixels(void);

#ifdef __cplusplus
}
#endif
#endif /* GMB_UI_INSPECTOR_H */
