/* ============================================================================
 * gmb_tui_text.c — modello testo stilizzato (porting ClipperLib/Clipper Text.cs
 * + Styles.cs). Puro, TTY-free.
 * ============================================================================ */
#include "gmb_tui_text.h"
#include <stdio.h>
#include <string.h>

/* ===================== Style ===================== */

gmb_style_t gmb_style_new(void)
{
    gmb_style_t s;
    memset(&s, 0, sizeof(s));
    return s;
}

gmb_style_t gmb_style_default(void) { return gmb_style_new(); }

gmb_style_t gmb_style_reset(void)
{
    gmb_style_t s;
    memset(&s, 0, sizeof(s));
    s.has_fg = true; s.fg = GMB_CELLCOLOR_DEFAULT;
    s.has_bg = true; s.bg = GMB_CELLCOLOR_DEFAULT;
    s.sub_mod = 0x1FFu;
    return s;
}

gmb_style_t gmb_style_with_fg(gmb_style_t s, gmb_cell_color_t color)
{
    s.has_fg = true;
    s.fg = color;
    return s;
}

gmb_style_t gmb_style_with_bg(gmb_style_t s, gmb_cell_color_t color)
{
    s.has_bg = true;
    s.bg = color;
    return s;
}

gmb_style_t gmb_style_with_add_modifier(gmb_style_t s, gmb_modifier_t m)
{
    s.sub_mod &= (gmb_modifier_t)~m;
    s.add_mod |= m;
    return s;
}

gmb_style_t gmb_style_with_remove_modifier(gmb_style_t s, gmb_modifier_t m)
{
    s.add_mod &= (gmb_modifier_t)~m;
    s.sub_mod |= m;
    return s;
}

gmb_style_t gmb_style_patch(gmb_style_t s, gmb_style_t other)
{
    if (other.has_fg) { s.has_fg = true; s.fg = other.fg; }
    if (other.has_bg) { s.has_bg = true; s.bg = other.bg; }
    s.add_mod &= (gmb_modifier_t)~other.sub_mod;
    s.add_mod |= other.add_mod;
    s.sub_mod &= (gmb_modifier_t)~other.add_mod;
    s.sub_mod |= other.sub_mod;
    return s;
}

void gmb_style_patch_ptr(const gmb_style_t* s, const gmb_style_t* other, gmb_style_t* out)
{
    if (out) *out = gmb_style_patch(*s, *other);
}

bool gmb_style_eq(gmb_style_t a, gmb_style_t b)
{
    return a.has_fg == b.has_fg && a.has_bg == b.has_bg
        && gmb_cellcolor_eq(a.fg, b.fg) && gmb_cellcolor_eq(a.bg, b.bg)
        && a.add_mod == b.add_mod && a.sub_mod == b.sub_mod;
}

/* ===================== Span / Line / Text ===================== */

void gmb_line_raw(gmb_line_t* out, const char* content)
{
    gmb_line_t l;
    memset(&l, 0, sizeof(l));
    l.span_count = 1;
    snprintf(l.spans[0].content, sizeof(l.spans[0].content), "%s", content ? content : "");
    l.spans[0].style = gmb_style_default();
    *out = l;
}

void gmb_line_styled(gmb_line_t* out, const char* content, gmb_style_t style)
{
    gmb_line_t l;
    gmb_line_raw(&l, content);
    l.spans[0].style = style;
    *out = l;
}

gmb_line_t gmb_line_from_span(gmb_span_t span)
{
    gmb_line_t l;
    memset(&l, 0, sizeof(l));
    l.span_count = 1;
    l.spans[0] = span;
    return l;
}

int gmb_line_width(const gmb_line_t* l)
{
    int w = 0;
    for (int i = 0; i < l->span_count; i++)
        w += (int)strlen(l->spans[i].content);
    return w;
}

int gmb_line_text(const gmb_line_t* l, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int o = 0;
    for (int i = 0; i < l->span_count; i++)
        for (const char* p = l->spans[i].content; *p && o < cap - 1; p++)
            out[o++] = *p;
    out[o] = '\0';
    return o;
}

int gmb_line_style_at(const gmb_line_t* l, int column, gmb_style_t* out)
{
    if (column < 0) return 0;
    int col = 0;
    for (int i = 0; i < l->span_count; i++)
    {
        int w = (int)strlen(l->spans[i].content);
        if (column < col + w)
        {
            if (out) *out = gmb_style_patch(l->style, l->spans[i].style);
            return 1;
        }
        col += w;
    }
    return 0;
}

void gmb_text_raw(gmb_text_t* out, const char* content)
{
    gmb_text_t t;
    memset(&t, 0, sizeof(t));
    char copy[4096];
    snprintf(copy, sizeof(copy), "%s", content ? content : "");
    char* line = copy;
    while (line && t.line_count < GMB_TEXT_MAX_LINES)
    {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        gmb_line_raw(&t.lines[t.line_count++], line);
        if (!nl) break;
        line = nl + 1;
    }
    *out = t;
}

void gmb_text_styled(gmb_text_t* out, const char* content, gmb_style_t style)
{
    gmb_text_t t;
    gmb_text_raw(&t, content);
    for (int i = 0; i < t.line_count; i++)
        t.lines[i].spans[0].style = style;
    *out = t;
}

void gmb_text_push(gmb_text_t* t, gmb_line_t line)
{
    if (t->line_count < GMB_TEXT_MAX_LINES)
        t->lines[t->line_count++] = line;
}

void gmb_text_append(gmb_text_t* t, const gmb_text_t* other)
{
    for (int i = 0; i < other->line_count && t->line_count < GMB_TEXT_MAX_LINES; i++)
        t->lines[t->line_count++] = other->lines[i];
}

int gmb_text_height(const gmb_text_t* t) { return t->line_count; }

int gmb_text_width(const gmb_text_t* t)
{
    int max = 0;
    for (int i = 0; i < t->line_count; i++)
    {
        int w = gmb_line_width(&t->lines[i]);
        if (w > max) max = w;
    }
    return max;
}

/* ===================== Styles ===================== */

int gmb_styles_style_code(const char* style)
{
    if (!style) return 0;
    if (strcmp(style, "bold") == 0) return 1;
    if (strcmp(style, "dim") == 0) return 2;
    if (strcmp(style, "italic") == 0) return 3;
    if (strcmp(style, "underline") == 0) return 4;
    if (strcmp(style, "blink") == 0) return 5;
    if (strcmp(style, "reverse") == 0) return 7;
    if (strcmp(style, "hidden") == 0) return 8;
    return 0;
}

static int styles_wrap(const char* pre, const char* s, const char* post, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int o = 0;
    for (const char* p = pre; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = 0; i < (int)strlen(s) && o < cap - 1; i++) out[o++] = s[i];
    for (const char* p = post; *p && o < cap - 1; p++) out[o++] = *p;
    out[o] = '\0';
    return o;
}

int gmb_styles_set_style(const char* s, const char* style, char* out, int cap)
{
    char pre[16];
    snprintf(pre, sizeof(pre), "\x1b[%dm", gmb_styles_style_code(style));
    return styles_wrap(pre, s, "\x1b[0m", out, cap);
}

int gmb_styles_bold(const char* s, char* out, int cap) { return styles_wrap(GMB_ANSI_BOLD, s, GMB_ANSI_RESET, out, cap); }
int gmb_styles_dim(const char* s, char* out, int cap) { return styles_wrap(GMB_ANSI_DIM, s, GMB_ANSI_RESET, out, cap); }
int gmb_styles_italic(const char* s, char* out, int cap) { return styles_wrap(GMB_ANSI_ITALIC, s, GMB_ANSI_RESET, out, cap); }
int gmb_styles_underline(const char* s, char* out, int cap) { return styles_wrap(GMB_ANSI_UNDERLINE, s, GMB_ANSI_RESET, out, cap); }
int gmb_styles_blink(const char* s, char* out, int cap) { return styles_wrap(GMB_ANSI_BLINK, s, GMB_ANSI_RESET, out, cap); }
int gmb_styles_reverse(const char* s, char* out, int cap) { return styles_wrap(GMB_ANSI_REVERSE, s, GMB_ANSI_RESET, out, cap); }
int gmb_styles_hidden(const char* s, char* out, int cap) { return styles_wrap(GMB_ANSI_HIDDEN, s, GMB_ANSI_RESET, out, cap); }

int gmb_styles_set_fg_rgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap)
{
    char pre[64];
    gmb_ansi_fgrgb(r, g, b, pre, sizeof(pre));
    return styles_wrap(pre, s, GMB_ANSI_RESET, out, cap);
}

int gmb_styles_set_bg_rgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap)
{
    char pre[64];
    gmb_ansi_bgrgb(r, g, b, pre, sizeof(pre));
    return styles_wrap(pre, s, GMB_ANSI_RESET, out, cap);
}

int gmb_styles_set_fg256(const char* s, uint8_t color, char* out, int cap)
{
    char pre[64];
    gmb_ansi_fg256(color, pre, sizeof(pre));
    return styles_wrap(pre, s, GMB_ANSI_RESET, out, cap);
}

int gmb_styles_set_bg256(const char* s, uint8_t color, char* out, int cap)
{
    char pre[64];
    gmb_ansi_bg256(color, pre, sizeof(pre));
    return styles_wrap(pre, s, GMB_ANSI_RESET, out, cap);
}

int gmb_styles_set_fg(const char* s, gmb_cell_color_t c, char* out, int cap)
{
    if (c.is_default)
        return styles_wrap("\x1b[39m", s, GMB_ANSI_RESET, out, cap);
    return gmb_styles_set_fg_rgb(s, c.r, c.g, c.b, out, cap);
}

int gmb_styles_set_bg(const char* s, gmb_cell_color_t c, char* out, int cap)
{
    if (c.is_default)
        return styles_wrap("\x1b[49m", s, GMB_ANSI_RESET, out, cap);
    return gmb_styles_set_bg_rgb(s, c.r, c.g, c.b, out, cap);
}

int gmb_styles_set_colors(const char* s, gmb_cell_color_t fg, gmb_cell_color_t bg, char* out, int cap)
{
    char pre[128];
    int o = 0;
    if (fg.is_default)
    {
        for (const char* p = "\x1b[39m"; *p && o < 127; p++) pre[o++] = *p;
    }
    else
    {
        char t[64];
        gmb_ansi_fgrgb(fg.r, fg.g, fg.b, t, sizeof(t));
        for (char* p = t; *p && o < 127; p++) pre[o++] = *p;
    }
    if (bg.is_default)
    {
        for (const char* p = "\x1b[49m"; *p && o < 127; p++) pre[o++] = *p;
    }
    else
    {
        char t[64];
        gmb_ansi_bgrgb(bg.r, bg.g, bg.b, t, sizeof(t));
        for (char* p = t; *p && o < 127; p++) pre[o++] = *p;
    }
    pre[o] = '\0';
    return styles_wrap(pre, s, GMB_ANSI_RESET, out, cap);
}
