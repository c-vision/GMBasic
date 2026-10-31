/* ============================================================================
 * gmb_gfx_cellfont.c — font a CELLE 8x16 per il render TUI->pixel (SDLConsole).
 * ASCII da tabella bitmap; box-drawing PROCEDURALE dalla geometria della cella
 * (cf_box_mask/cf_box) -> le cornici si CONNETTONO con qualunque font e a
 * qualunque dimensione, senza dipendere dai glifi del font. Tiling esatto.
 * ============================================================================ */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <string.h>
/* Tabella ASCII 8x16 GENERATA da Terminus (SIL OFL 1.1): vedi
 * gmb_gfx_cellfont_bitmap.h e tools/py/cellfont_from_bdf.py. */
#include "gmb_gfx_cellfont_bitmap.h"

static void cf_rect(gmb_gfx_page_t* p, int x, int y, int w, int h, uint32_t c)
{
    if (!p || w <= 0 || h <= 0) return;
    int x0=x, y0=y, x1=x+w-1, y1=y+h-1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 >= p->w) x1 = p->w - 1; if (y1 >= p->h) y1 = p->h - 1;
    for (int yy=y0; yy<=y1; yy++) for (int xx=x0; xx<=x1; xx++) p->pixels[(size_t)yy*p->w+xx]=c;
}

void gmb_gfx_cell_bg(int x, int y, int cw, int ch, uint32_t bg)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || cw <= 0 || ch <= 0) return;
    cf_rect(p, x, y, cw, ch, bg);
}

/* ── Box-drawing PROCEDURALE (indipendente dal font) ────────────────────────────────
 * PERCHÉ. Le cornici del TUI non possono dipendere dal fatto che il font attivo abbia i
 * glifi 0x2500-0x256C: i font liberi che possiamo distribuire non li hanno tutti (es.
 * Sixtyfour ne copre 18 su 128) e quelli che li avevano non erano ridistribuibili. Qui i
 * segmenti si disegnano a partire dalla GEOMETRIA DELLA CELLA, quindi la cornice si connette
 * con qualunque font, a qualunque dimensione.
 *
 * Maschera: 1=su, 2=giu', 4=sinistra, 8=destra; il bit 16 chiede la linea DOPPIA. */
static int cf_box_mask(uint32_t cp, int* dbl)
{
    *dbl = 0;
    switch (cp) {
    /* linee semplici */
    case 0x2500: return 4|8;          /* ─ */
    case 0x2502: return 1|2;          /* │ */
    case 0x250C: return 2|8;          /* ┌ */
    case 0x2510: return 2|4;          /* ┐ */
    case 0x2514: return 1|8;          /* └ */
    case 0x2518: return 1|4;          /* ┘ */
    case 0x251C: return 1|2|8;        /* ├ */
    case 0x2524: return 1|2|4;        /* ┤ */
    case 0x252C: return 2|4|8;        /* ┬ */
    case 0x2534: return 1|4|8;        /* ┴ */
    case 0x253C: return 1|2|4|8;      /* ┼ */
    /* linee marcate (usate dai font "heavy": es. Sixtyfour) */
    case 0x2501: case 0x2503: case 0x250F: case 0x2513: case 0x2517: case 0x251B:
    case 0x2523: case 0x252B: case 0x2533: case 0x253B: case 0x254B:
        *dbl = 0;
        switch (cp) {
        case 0x2501: return 4|8; case 0x2503: return 1|2;
        case 0x250F: return 2|8; case 0x2513: return 2|4;
        case 0x2517: return 1|8; case 0x251B: return 1|4;
        case 0x2523: return 1|2|8; case 0x252B: return 1|2|4;
        case 0x2533: return 2|4|8; case 0x253B: return 1|4|8;
        default:     return 1|2|4|8;
        }
    /* linee doppie (e i misti semplice/doppia, resi come doppi ai fini della connessione) */
    case 0x2550: case 0x2551: case 0x2552: case 0x2553: case 0x2554: case 0x2555:
    case 0x2556: case 0x2557: case 0x2558: case 0x2559: case 0x255A: case 0x255B:
    case 0x255C: case 0x255D: case 0x255E: case 0x255F: case 0x2560: case 0x2561:
    case 0x2562: case 0x2563: case 0x2564: case 0x2565: case 0x2566: case 0x2567:
    case 0x2568: case 0x2569: case 0x256A: case 0x256B: case 0x256C:
        *dbl = 1;
        switch (cp) {
        case 0x2550: return 4|8;
        case 0x2551: return 1|2;
        case 0x2554: return 2|8; case 0x2557: return 2|4;
        case 0x255A: return 1|8; case 0x255D: return 1|4;
        case 0x2560: return 1|2|8; case 0x2563: return 1|2|4;
        case 0x2566: return 2|4|8; case 0x2569: return 1|4|8;
        case 0x256C: return 1|2|4|8;
        /* misti: prevalenza alla direzione "piena" del codepoint */
        case 0x2552: case 0x2555: case 0x2558: case 0x255B: return 2|4|8;
        case 0x2553: case 0x2556: case 0x2559: case 0x255C: return 1|4|8;
        case 0x255E: case 0x2561: return 1|2|8;
        case 0x255F: case 0x2562: return 1|2|4;
        case 0x2564: case 0x2567: return 2|4|8;
        case 0x2565: case 0x2568: return 1|4|8;
        default:     return 1|2|4|8;
        }
    /* linee PARZIALI (semirette): prima dipendevano dai glifi del font */
    case 0x2574: case 0x2578: return 4;      /* ╴ ╸  solo sinistra */
    case 0x2575: case 0x2579: return 1;      /* ╵ ╹  solo su     */
    case 0x2576: case 0x257A: return 8;      /* ╶ ╺  solo destra */
    case 0x2577: case 0x257B: return 2;      /* ╷ ╻  solo giu'   */
    case 0x257C: case 0x257E: return 4|8;    /* ╼ ╾  orizzontale */
    case 0x257D: case 0x257F: return 1|2;    /* ╽ ╿  verticale   */
    default: return 0;
    }
}

static void cf_box(int x, int y, int cw, int ch, int mask, int dbl, uint32_t fg)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || cw <= 0 || ch <= 0) return;
    int t  = (cw >= 12 || ch >= 20) ? 2 : 1;      /* spessore della linea */
    int cx = x + cw / 2;                          /* centro orizzontale della cella */
    int cy = y + ch / 2;                          /* centro verticale della cella */
    int d  = dbl ? t + 1 : 0;                     /* distanza fra le due linee del "doppio" */
    int o  = t / 2;                               /* centratura dello spessore */

    if (mask & 4) {                               /* verso sinistra */
        int w = cx - x;
        if (!dbl) cf_rect(p, x, cy - o, w, t, fg);
        else { cf_rect(p, x, cy - d - o, w, t, fg); cf_rect(p, x, cy + d - o, w, t, fg); }
    }
    if (mask & 8) {                               /* verso destra */
        int w = (x + cw) - cx;
        if (!dbl) cf_rect(p, cx, cy - o, w, t, fg);
        else { cf_rect(p, cx, cy - d - o, w, t, fg); cf_rect(p, cx, cy + d - o, w, t, fg); }
    }
    if (mask & 1) {                               /* verso l'alto */
        int h = cy - y;
        if (!dbl) cf_rect(p, cx - o, y, t, h, fg);
        else { cf_rect(p, cx - d - o, y, t, h, fg); cf_rect(p, cx + d - o, y, t, h, fg); }
    }
    if (mask & 2) {                               /* verso il basso */
        int h = (y + ch) - cy;
        if (!dbl) cf_rect(p, cx - o, cy, t, h, fg);
        else { cf_rect(p, cx - d - o, cy, t, h, fg); cf_rect(p, cx + d - o, cy, t, h, fg); }
    }
}

/* Disegna SOLO il glifo (sfondo gia' riempito). */
void gmb_gfx_cell_glyph(int x, int y, int cw, int ch, uint32_t cp, uint32_t fg)
{
    if (cp == 0 || cp == ' ') return;

    /* 1. box-drawing: SEMPRE procedurale (nessuna dipendenza dalla copertura del font). */
    { int dbl = 0, mask = cf_box_mask(cp, &dbl);
      if (mask) { cf_box(x, y, cw, ch, mask, dbl, fg); return; } }

    extern int gmb_console_mode_get(void);
    if (gmb_console_mode_get() == 1) {   /* %C64: PETSCII + vertical-fill */
        extern int gmb_c64_remap(int cp);
        extern int gmb_c64_is_vfill(int cp);
        extern void gmb_gfx_draw_glyph_vfill(int cp, int x, int y, int cw, int ch, uint32_t color);
        if (gmb_c64_is_vfill((int)cp)) { gmb_gfx_draw_glyph_vfill(gmb_c64_remap((int)cp), x, y, cw, ch, fg); return; }
        cp = (uint32_t)gmb_c64_remap((int)cp);
    }
    gmb_gfx_draw_glyph(cp, x, y, fg);   /* glifo NATURALE del font (come il terminale) */
}

void gmb_gfx_draw_cell(int x, int y, int cw, int ch, uint32_t cp, uint32_t fg, uint32_t bg)
{
    gmb_gfx_cell_bg(x, y, cw, ch, bg);
    gmb_gfx_cell_glyph(x, y, cw, ch, cp, fg);
}
