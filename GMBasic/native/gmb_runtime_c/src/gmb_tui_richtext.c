/* ============================================================================
 * gmb_tui_richtext.c — gmb_tui_richtext. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== RichText ===================== */

int gmb_richtext_parse_color(const char* tok, gmb_cell_color_t* out)
{
    if (!tok || !*tok || !out) return 0;
    if (tok[0] == '#')
    {
        const char* hex = tok + 1;
        if (strlen(hex) == 6)
        {
            char buf[3];
            buf[0] = hex[0]; buf[1] = hex[1]; buf[2] = '\0';
            int r = (int)strtol(buf, NULL, 16);
            buf[0] = hex[2]; buf[1] = hex[3];
            int g = (int)strtol(buf, NULL, 16);
            buf[0] = hex[4]; buf[1] = hex[5];
            int b = (int)strtol(buf, NULL, 16);
            out->r = (uint8_t)r; out->g = (uint8_t)g; out->b = (uint8_t)b;
            out->is_default = false;
            return 1;
        }
        return 0;
    }
    char n[32];
    snprintf(n, sizeof(n), "%s", tok);
    for (char* p = n; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    if (strcmp(n, "black") == 0) { *out = GMB_CELLCOLOR_BLACK; return 1; }
    if (strcmp(n, "white") == 0) { *out = GMB_CELLCOLOR_WHITE; return 1; }
    if (strcmp(n, "red") == 0) { *out = GMB_CELLCOLOR_RED; return 1; }
    if (strcmp(n, "green") == 0) { *out = GMB_CELLCOLOR_GREEN; return 1; }
    if (strcmp(n, "blue") == 0) { *out = GMB_CELLCOLOR_BLUE; return 1; }
    if (strcmp(n, "yellow") == 0) { *out = GMB_CELLCOLOR_YELLOW; return 1; }
    if (strcmp(n, "cyan") == 0) { *out = GMB_CELLCOLOR_CYAN; return 1; }
    if (strcmp(n, "magenta") == 0) { *out = GMB_CELLCOLOR_MAGENTA; return 1; }
    if (strcmp(n, "grey") == 0 || strcmp(n, "gray") == 0) { *out = GMB_CELLCOLOR_GREY; return 1; }
    if (strcmp(n, "darkred") == 0) { *out = GMB_CELLCOLOR_DARKRED; return 1; }
    if (strcmp(n, "darkgreen") == 0) { *out = GMB_CELLCOLOR_DARKGREEN; return 1; }
    if (strcmp(n, "darkblue") == 0) { *out = GMB_CELLCOLOR_DARKBLUE; return 1; }
    if (strcmp(n, "darkyellow") == 0) { *out = GMB_CELLCOLOR_DARKYELLOW; return 1; }
    if (strcmp(n, "darkcyan") == 0) { *out = GMB_CELLCOLOR_DARKCYAN; return 1; }
    if (strcmp(n, "darkmagenta") == 0) { *out = GMB_CELLCOLOR_DARKMAGENTA; return 1; }
    return 0;
}

int gmb_richtext_parse(const char* markup, gmb_rich_span_t* out, int max)
{
    int span_count = 0;
    char text[256];
    int tlen = 0;
    bool bold = false, italic = false, underline = false;
    bool has_fg = false, has_bg = false;
    gmb_cell_color_t fg = GMB_CELLCOLOR_DEFAULT, bg = GMB_CELLCOLOR_DEFAULT;
    if (!markup) return 0;
    int len = (int)strlen(markup);
    for (int i = 0; i < len; i++)
    {
        char c = markup[i];
        if (c == '*')
        {
            if (tlen > 0 && span_count < max)
            {
                gmb_rich_span_t* s = &out[span_count++];
                snprintf(s->text, sizeof(s->text), "%.*s", tlen, text);
                s->has_fg = has_fg; s->fg = fg;
                s->has_bg = has_bg; s->bg = bg;
                s->bold = bold; s->italic = italic; s->underline = underline;
                tlen = 0;
            }
            bold = !bold;
            continue;
        }
        if (c == '_')
        {
            if (tlen > 0 && span_count < max)
            {
                gmb_rich_span_t* s = &out[span_count++];
                snprintf(s->text, sizeof(s->text), "%.*s", tlen, text);
                s->has_fg = has_fg; s->fg = fg;
                s->has_bg = has_bg; s->bg = bg;
                s->bold = bold; s->italic = italic; s->underline = underline;
                tlen = 0;
            }
            italic = !italic;
            continue;
        }
        if (c == '{')
        {
            int close = -1;
            for (int j = i + 1; j < len; j++) if (markup[j] == '}') { close = j; break; }
            if (close < 0)
            {
                if (tlen < 255) text[tlen++] = c;
                continue;
            }
            char inner[256];
            int inl = close - i - 1;
            if (inl > 255) inl = 255;
            memcpy(inner, markup + i + 1, (size_t)inl);
            inner[inl] = '\0';
            char* sp = strchr(inner, ' ');
            if (!sp)
            {
                gmb_cell_color_t nc;
                if (gmb_richtext_parse_color(inner, &nc)) { has_fg = true; fg = nc; }
                i = close;
                continue;
            }
            *sp = '\0';
            char* color_tok = inner;
            char* content = sp + 1;
            gmb_cell_color_t nc;
            bool prev_has = has_fg;
            gmb_cell_color_t prev_fg = fg;
            /* Il testo pendente ha lo stile PRECEDENTE: va chiuso prima dello span
             * colorato, altrimenti l'ordine dei run si inverte. */
            if (tlen > 0 && span_count < max)
            {
                gmb_rich_span_t* s0 = &out[span_count++];
                snprintf(s0->text, sizeof(s0->text), "%.*s", tlen, text);
                s0->has_fg = has_fg; s0->fg = fg;
                s0->has_bg = has_bg; s0->bg = bg;
                s0->bold = bold; s0->italic = italic; s0->underline = underline;
                tlen = 0;
            }
            if (gmb_richtext_parse_color(color_tok, &nc)) { has_fg = true; fg = nc; }
            if (content[0] && span_count < max)
            {
                gmb_rich_span_t* s = &out[span_count++];
                snprintf(s->text, sizeof(s->text), "%s", content);
                s->has_fg = has_fg; s->fg = fg;
                s->has_bg = has_bg; s->bg = bg;
                s->bold = bold; s->italic = italic; s->underline = underline;
            }
            has_fg = prev_has;
            fg = prev_fg;
            i = close;
            continue;
        }
        if (tlen < 255) text[tlen++] = c;
    }
    if (tlen > 0 && span_count < max)
    {
        gmb_rich_span_t* s = &out[span_count++];
        snprintf(s->text, sizeof(s->text), "%.*s", tlen, text);
        s->has_fg = has_fg; s->fg = fg;
        s->has_bg = has_bg; s->bg = bg;
        s->bold = bold; s->italic = italic; s->underline = underline;
    }
    return span_count;
}

/* Decodifica UN codepoint UTF-8; ritorna i byte consumati (>=1). */
static int rt_utf8(const char* s, unsigned* cp)
{
    const unsigned char* p = (const unsigned char*)s;
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
    { *cp = ((unsigned)(p[0] & 0x1F) << 6) | (unsigned)(p[1] & 0x3F); return 2; }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
    { *cp = ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) | (unsigned)(p[2] & 0x3F); return 3; }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80)
    { *cp = ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12)
           | ((unsigned)(p[2] & 0x3F) << 6) | (unsigned)(p[3] & 0x3F); return 4; }
    *cp = p[0];
    return 1;
}

/* Scrive una cella: i colori NON specificati mantengono quelli della cella
 * (parità con ScreenBuffer.SetChar, che usa il colore corrente). */
static void rt_put(gmb_screen_buffer_t* buf, int x, int y, unsigned cp,
                   bool has_fg, gmb_cell_color_t fg, bool has_bg, gmb_cell_color_t bg)
{
    if (x < 0 || y < 0 || x >= gmb_screen_width(buf) || y >= gmb_screen_height(buf)) return;
    if (!has_fg || !has_bg)
    {
        int cell[10];
        gmb_screen_get_cell(buf, x, y, cell);
        if (!has_fg) { fg.r = (uint8_t)cell[1]; fg.g = (uint8_t)cell[2]; fg.b = (uint8_t)cell[3]; fg.is_default = cell[4] != 0; }
        if (!has_bg) { bg.r = (uint8_t)cell[5]; bg.g = (uint8_t)cell[6]; bg.b = (uint8_t)cell[7]; bg.is_default = cell[8] != 0; }
    }
    gmb_screen_setchar(buf, x, y, cp, fg, bg);
}

void gmb_richtext_render_to_buffer(gmb_screen_buffer_t* buf, int x, int y, gmb_rich_span_t* spans, int count, int max_width)
{
    int cx = x;
    int limit = x + max_width;
    for (int s = 0; s < count; s++)
    {
        const char* p = spans[s].text;
        while (*p)
        {
            if (cx >= limit) return;
            unsigned cp; int adv = rt_utf8(p, &cp);
            rt_put(buf, cx, y, cp, spans[s].has_fg, spans[s].fg, spans[s].has_bg, spans[s].bg);
            cx++;
            p += adv;
        }
    }
}

int gmb_richtext_render_paragraph(gmb_screen_buffer_t* buf, int x, int y, int w, int h,
                                  gmb_rich_span_t* spans, int count, int wrap)
{
    if (!buf || w <= 0) return 0;
    if (h <= 0) h = 1;
    if (!wrap) { gmb_richtext_render_to_buffer(buf, x, y, spans, count, w); return 1; }

    int cx = x, cy = y;
    int limit = x + w, bottom = y + h - 1;
    int drawn = 0;

    for (int s = 0; s < count; s++)
    {
        const char* p = spans[s].text;
        while (*p)
        {
            /* parola (sequenza di byte fino allo spazio) */
            const char* ws = p;
            while (*p && *p != ' ') p++;
            int wbytes = (int)(p - ws);
            int wcols = 0;
            for (int k = 0; k < wbytes; )
            {
                unsigned tmp; int adv = rt_utf8(ws + k, &tmp);
                k += adv; wcols++;
            }
            if (cx + wcols > limit && cx > x)
            {
                cx = x; cy++;
                if (cy > bottom) return drawn;
            }
            for (int k = 0; k < wbytes; )
            {
                if (cx >= limit) break;
                unsigned cp; int adv = rt_utf8(ws + k, &cp);
                rt_put(buf, cx, cy, cp, spans[s].has_fg, spans[s].fg, spans[s].has_bg, spans[s].bg);
                cx++; drawn++;
                k += adv;
            }
            if (*p == ' ')
            {
                p++;
                /* lo spazio separa dalla parola successiva (anche se è in un altro span) */
                if (cx >= limit) { cx = x; cy++; if (cy > bottom) return drawn; }
                else { rt_put(buf, cx, cy, ' ', spans[s].has_fg, spans[s].fg, spans[s].has_bg, spans[s].bg); cx++; drawn++; }
            }
        }
    }
    return drawn;
}
