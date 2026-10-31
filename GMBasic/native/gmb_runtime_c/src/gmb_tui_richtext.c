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

void gmb_richtext_render_to_buffer(gmb_screen_buffer_t* buf, int x, int y, gmb_rich_span_t* spans, int count, int max_width)
{
    int cx = x;
    int limit = x + max_width;
    for (int s = 0; s < count; s++)
    {
        for (const char* p = spans[s].text; *p; p++)
        {
            if (cx >= limit) return;
            gmb_screen_setchar(buf, cx, y, (unsigned char)*p,
                spans[s].has_fg ? spans[s].fg : GMB_CELLCOLOR_DEFAULT,
                spans[s].has_bg ? spans[s].bg : GMB_CELLCOLOR_DEFAULT);
            cx++;
        }
    }
}
