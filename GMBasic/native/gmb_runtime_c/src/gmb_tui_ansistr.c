/* ============================================================================
 * gmb_tui_ansistr.c — gmb_tui_ansistr. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== AnsiStr ===================== */

static int utf8_decode_at(const char* s, int i, int len, uint32_t* cp)
{
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xC2 && c <= 0xDF && i + 1 < len) { *cp = ((uint32_t)(c & 0x1F) << 6) | ((unsigned char)s[i+1] & 0x3F); return 2; }
    if (c >= 0xE0 && c <= 0xEF && i + 2 < len) { *cp = ((uint32_t)(c & 0x0F) << 12) | (((unsigned char)s[i+1] & 0x3F) << 6) | ((unsigned char)s[i+2] & 0x3F); return 3; }
    if (c >= 0xF0 && c <= 0xF4 && i + 3 < len) { *cp = ((uint32_t)(c & 0x07) << 18) | (((unsigned char)s[i+1] & 0x3F) << 12) | (((unsigned char)s[i+2] & 0x3F) << 6) | ((unsigned char)s[i+3] & 0x3F); return 4; }
    *cp = c;
    return 1;
}

int gmb_ansistr_char_width(uint32_t cp)
{
    if (cp < 0x20 || (cp >= 0x7f && cp <= 0x9f)) return 0;
    if (cp < 0x1100) return 1;
    if (cp <= 0x115f) return 2;
    if (cp == 0x2329 || cp == 0x232a) return 2;
    if (cp >= 0x2e80 && cp <= 0x303e) return 2;
    if (cp >= 0x3040 && cp <= 0x33ff) return 2;
    if (cp >= 0x3400 && cp <= 0x4db5) return 2;
    if (cp >= 0x4e00 && cp <= 0x9fff) return 2;
    if (cp >= 0xa000 && cp <= 0xa4cf) return 2;
    if (cp >= 0xac00 && cp <= 0xd7a3) return 2;
    if (cp >= 0xd800 && cp <= 0xdfff) return 2;
    if (cp >= 0xf900 && cp <= 0xfaff) return 2;
    if (cp >= 0xfe30 && cp <= 0xfe6f) return 2;
    if (cp >= 0xff01 && cp <= 0xff60) return 2;
    if (cp >= 0xffe0 && cp <= 0xffe6) return 2;
    if (cp >= 0x1b000 && cp <= 0x1b0ff) return 2;
    if (cp >= 0x1d000 && cp <= 0x1d1ff) return 2;
    if (cp >= 0x20000 && cp <= 0x2fffd) return 2;
    if (cp >= 0x30000 && cp <= 0x3fffd) return 2;
    return 1;
}

static int ansi_skip_escape(const char* s, int i)
{
    int len = (int)strlen(s);
    if (i >= len) return i;
    if (s[i] == '[')
    {
        i++;
        while (i < len)
        {
            unsigned char c = (unsigned char)s[i];
            if ((c >= '0' && c <= '9') || c == ';' || c == ':' || c == '?' || c == '=') i++;
            else break;
        }
        if (i < len) i++;
        return i;
    }
    if (s[i] == ']' || s[i] == 'P' || s[i] == 'X' || s[i] == '^' || s[i] == '_')
    {
        while (i < len && s[i] != '\x07' && s[i] != '\\') i++;
        if (i < len) i++;
        return i;
    }
    i++;
    return i;
}

static int ansi_take_escape(const char* s, int* i, char* out, int cap)
{
    int len = (int)strlen(s);
    int o = 0;
    if (o < cap - 1) out[o++] = '\x1b';
    if (*i < len)
    {
        if (o < cap - 1) out[o++] = s[*i];
        (*i)++;
    }
    while (*i < len)
    {
        unsigned char c = (unsigned char)s[*i];
        if (c >= 0x20 && c <= 0x3f) { if (o < cap - 1) out[o++] = s[*i]; (*i)++; }
        else break;
    }
    if (*i < len)
    {
        unsigned char c = (unsigned char)s[*i];
        if (c >= 0x40 && c <= 0x7e) { if (o < cap - 1) out[o++] = s[*i]; (*i)++; }
    }
    out[o] = '\0';
    return o;
}

int gmb_ansistr_width(const char* s)
{
    if (!s) return 0;
    int width = 0;
    int len = (int)strlen(s);
    int i = 0;
    while (i < len)
    {
        if (s[i] == '\x1b')
        {
            i = ansi_skip_escape(s, i + 1);
            continue;
        }
        uint32_t cp;
        int adv = utf8_decode_at(s, i, len, &cp);
        width += gmb_ansistr_char_width(cp);
        i += adv;
    }
    return width;
}

int gmb_ansistr_truncate(const char* s, int max_width, char* out, int cap)
{
    int o = 0;
    int col = 0;
    int emitted = 0;
    int len = (int)strlen(s);
    int i = 0;
    while (i < len)
    {
        if (s[i] == '\x1b')
        {
            int i0 = i;
            (void)i0;
            char seq[64];
            ansi_take_escape(s, &i, seq, (int)sizeof(seq));
            for (int k = 0; seq[k] && o < cap - 1; k++) out[o++] = seq[k];
            emitted = 1;
            continue;
        }
        uint32_t cp;
        int adv = utf8_decode_at(s, i, len, &cp);
        int w = gmb_ansistr_char_width(cp);
        if (col + w <= max_width)
        {
            for (int k = 0; k < adv && o < cap - 1; k++) out[o++] = s[i + k];
            col += w;
            emitted = 1;
            i += adv;
        }
        else break;
    }
    if (emitted && o + 4 < cap)
    {
        const char* reset = "\x1b[0m";
        for (int k = 0; reset[k]; k++) out[o++] = reset[k];
    }
    out[o] = '\0';
    return o;
}

int gmb_ansistr_cut(const char* s, int start, int end, char* out, int cap)
{
    int o = 0;
    int col = 0;
    int emitting = 0;
    int len = (int)strlen(s);
    int i = 0;
    while (i < len)
    {
        if (s[i] == '\x1b')
        {
            char seq[64];
            ansi_take_escape(s, &i, seq, (int)sizeof(seq));
            if (emitting)
                for (int k = 0; seq[k] && o < cap - 1; k++) out[o++] = seq[k];
            continue;
        }
        uint32_t cp;
        int adv = utf8_decode_at(s, i, len, &cp);
        int w = gmb_ansistr_char_width(cp);
        int next = col + w;
        if (col >= start && col < end)
        {
            emitting = 1;
            for (int k = 0; k < adv && o < cap - 1; k++) out[o++] = s[i + k];
        }
        col = next;
        if (col >= end) break;
        i += adv;
    }
    if (emitting && o + 4 < cap)
    {
        const char* reset = "\x1b[0m";
        for (int k = 0; reset[k]; k++) out[o++] = reset[k];
    }
    out[o] = '\0';
    return o;
}

int gmb_ansistr_center(const char* s, int width, char* out, int cap)
{
    int vw = gmb_ansistr_width(s);
    if (vw >= width)
    {
        snprintf(out, (size_t)cap, "%s", s);
        return (int)strlen(out);
    }
    int left = (width - vw) / 2;
    int right = width - vw - left;
    int o = 0;
    for (int k = 0; k < left && o < cap - 1; k++) out[o++] = ' ';
    int sl = (int)strlen(s);
    for (int k = 0; k < sl && o < cap - 1; k++) out[o++] = s[k];
    const char* reset = "\x1b[0m";
    for (int k = 0; reset[k] && o < cap - 1; k++) out[o++] = reset[k];
    for (int k = 0; k < right && o < cap - 1; k++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
