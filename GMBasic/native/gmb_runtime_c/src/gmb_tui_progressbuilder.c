/* ============================================================================
 * gmb_tui_progressbuilder.c — gmb_tui_progressbuilder. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ProgressBuilder ===================== */

gmb_bar_builder_t gmb_bar_builder_new(void)
{
    gmb_bar_builder_t b;
    b.width = 20;
    b.filled_char = 0x2588;
    b.empty_char = 0x2591;
    b.left_bracket = '[';
    b.right_bracket = ']';
    return b;
}
gmb_bar_builder_t gmb_bar_builder_with_width(gmb_bar_builder_t b, int w) { b.width = w; return b; }
gmb_bar_builder_t gmb_bar_builder_with_filled(gmb_bar_builder_t b, uint32_t c) { b.filled_char = c; return b; }
gmb_bar_builder_t gmb_bar_builder_with_empty(gmb_bar_builder_t b, uint32_t c) { b.empty_char = c; return b; }
gmb_bar_builder_t gmb_bar_builder_with_brackets(gmb_bar_builder_t b, char l, char r)
{ b.left_bracket = l; b.right_bracket = r; return b; }

/* Codepoint BMP → UTF-8 (1..3 byte). Ritorna il numero di byte scritti. */
static int bar_utf8(char* out, int o, int cap, uint32_t cp)
{
    if (cp < 0x80) { if (o < cap - 1) out[o++] = (char)cp; }
    else if (cp < 0x800) {
        if (o < cap - 2) { out[o++] = (char)(0xC0 | (cp >> 6)); out[o++] = (char)(0x80 | (cp & 0x3F)); }
    } else {
        if (o < cap - 3) { out[o++] = (char)(0xE0 | (cp >> 12)); out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[o++] = (char)(0x80 | (cp & 0x3F)); }
    }
    return o;
}

int gmb_bar_render(gmb_bar_builder_t b, double fraction, char* out, int cap)
{
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    int filled = (int)(fraction * b.width + 0.5);
    if (filled > b.width) filled = b.width;
    int o = 0;
    if (o < cap - 1) out[o++] = b.left_bracket;
    for (int i = 0; i < filled; i++) o = bar_utf8(out, o, cap, b.filled_char);
    for (int i = filled; i < b.width; i++) o = bar_utf8(out, o, cap, b.empty_char);
    if (o < cap - 1) out[o++] = b.right_bracket;
    out[o] = '\0';
    return o;
}
int gmb_bar_render_with_percent(gmb_bar_builder_t b, double fraction, char* out, int cap)
{
    char bar[384];
    gmb_bar_render(b, fraction, bar, (int)sizeof(bar));
    if (b.width < 4) { snprintf(out, cap, "%s", bar); return (int)strlen(out); }
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", (int)(fraction * 100 + 0.5));
    int pl = (int)strlen(pct);
    if (pl > b.width) { snprintf(out, cap, "%s", bar); return (int)strlen(out); }
    /* cella di partenza del pct (centrato sul segmento tra le parentesi) */
    int start = 1 + (b.width - pl) / 2;
    /* offset in BYTE corrispondente alla cella `start`: somma le larghezze UTF-8
     * delle prime `start` celle (filled/empty) + 1 per la parentesi sinistra. */
    int bo = 1;
    int cell = 0;
    while (cell < start && bar[bo]) {
        unsigned char c = (unsigned char)bar[bo];
        int w = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
        bo += w;
        cell++;
    }
    for (int j = 0; j < pl && bo + j < cap - 1; j++) bar[bo + j] = pct[j];
    snprintf(out, cap, "%s", bar);
    return (int)strlen(out);
}
int gmb_byte_bar_render(long done, long total, int width, char* out, int cap)
{
    double fraction = total == 0 ? 1.0 : (double)done / total;
    gmb_bar_builder_t b = gmb_bar_builder_new();
    b.width = width;
    b.left_bracket = '|';
    b.right_bracket = '|';
    return gmb_bar_render(b, fraction, out, cap);
}
