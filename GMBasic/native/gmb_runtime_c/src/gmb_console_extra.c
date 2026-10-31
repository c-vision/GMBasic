/* gmb_console_extra.c — parità CommonLib/Consoles: ScreenSnapshot/TeeWriter/Headless/SystemConsole
 * Port completo in C, nessun bridge C↔C#.
 */
#include "gmb_console_extra.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===================== ScreenSnapshot (ScreenSnapshot.cs:91) ===================== */

int gmb_screen_snapshot_capture(const char* label, gmb_screen_buffer_t* buffer, gmb_screen_snapshot_t* out)
{
    if (!out || !buffer) return 0;
    int w = gmb_screen_width(buffer), h = gmb_screen_height(buffer);
    if (w <= 0 || h <= 0) return 0;
    memset(out, 0, sizeof(*out));
    if (label) snprintf(out->label, sizeof(out->label), "%s", label);
    out->width = w;
    out->height = h;
    out->chars = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    if (!out->chars) return 0;
    gmb_screen_cell_t* cells = gmb_screen_cells(buffer);
    for (int i = 0; i < w * h; i++) out->chars[i] = cells[i].character;
    return 1;
}

void gmb_screen_snapshot_free(gmb_screen_snapshot_t* s)
{
    if (!s) return;
    free(s->chars);
    s->chars = NULL;
}

static int snap_append(const gmb_screen_snapshot_t* s, char* out, int cap, int trim)
{
    if (!s || !out || cap <= 0) return 0;
    int o = 0;
    int last_nonblank = 0;
    /* passata 1: calcola ultima riga non vuota (trim) */
    if (trim) {
        int last_line_nb = -1;
        for (int r = 0; r < s->height; r++) {
            int blank = 1;
            for (int c = 0; c < s->width; c++)
                if (s->chars[(size_t)r * s->width + c] != 0 && s->chars[(size_t)r * s->width + c] != ' ') { blank = 0; break; }
            if (!blank) last_line_nb = r;
        }
        if (last_line_nb < 0) { out[0] = 0; return 0; }
        for (int r = 0; r <= last_line_nb && o < cap - 1; r++) {
            for (int c = 0; c < s->width && o < cap - 1; c++) {
                uint32_t cp = s->chars[(size_t)r * s->width + c];
                if (cp == 0) cp = ' ';
                if (cp < 0x80) out[o++] = (char)cp;
                else if (o + 3 < cap) { out[o++] = (char)0xE2; out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[o++] = (char)(0x80 | (cp & 0x3F)); }
            }
            if (o < cap - 1) out[o++] = '\n';
            last_nonblank = o;
        }
        if (last_nonblank > 0) o = last_nonblank;
        out[o] = 0;
        /* trima newline/spazi finali */
        while (o > 0 && (out[o-1] == '\n' || out[o-1] == ' ')) out[--o] = 0;
        return o;
    }
    for (int r = 0; r < s->height && o < cap - 1; r++) {
        for (int c = 0; c < s->width && o < cap - 1; c++) {
            uint32_t cp = s->chars[(size_t)r * s->width + c];
            if (cp == 0) cp = ' ';
            if (cp < 0x80) out[o++] = (char)cp;
            else if (o + 3 < cap) { out[o++] = (char)0xE2; out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[o++] = (char)(0x80 | (cp & 0x3F)); }
        }
        if (o < cap - 1) out[o++] = '\n';
    }
    out[o] = 0;
    return o;
}

int gmb_screen_snapshot_render_text(const gmb_screen_snapshot_t* s, char* out, int cap)
{ return snap_append(s, out, cap, 1); }
int gmb_screen_snapshot_render_text_raw(const gmb_screen_snapshot_t* s, char* out, int cap)
{ return snap_append(s, out, cap, 0); }

int gmb_screen_snapshot_contains(const gmb_screen_snapshot_t* s, const char* text)
{
    if (!s || !text) return 0;
    char tmp[4096];
    int n = gmb_screen_snapshot_render_text(s, tmp, (int)sizeof(tmp));
    (void)n;
    return strstr(tmp, text) != NULL;
}

int gmb_screen_snapshot_contains_error_indicators(const gmb_screen_snapshot_t* s)
{
    if (!s) return 0;
    char tmp[4096];
    gmb_screen_snapshot_render_text(s, tmp, (int)sizeof(tmp));
    if (!strstr(tmp, "?")) return 0;
    return strstr(tmp, "Errore") || strstr(tmp, "FATAL") || strstr(tmp, "RUNTIME ERROR") ||
           strstr(tmp, "eccezione") || strstr(tmp, "Exception") || strstr(tmp, "ERRORE") ||
           strstr(tmp, "STOP") || strstr(tmp, "INTERNAL ERROR");
}

/* ===================== TeeWriter (TeeWriter.cs:95) ===================== */

void gmb_tee_writer_init(gmb_tee_writer_t* t, gmb_text_writer_t a, gmb_text_writer_t b)
{
    if (!t) return;
    t->a = a;
    t->b = b;
}
int gmb_tee_writer_write(gmb_tee_writer_t* t, const char* s, int len)
{
    if (!t || !s) return 0;
    if (t->a.write) t->a.write(t->a.ctx, s, len);
    if (t->b.write) t->b.write(t->b.ctx, s, len);
    return len;
}

/* ===================== HeadlessConsole (Headless.cs) ===================== */

void gmb_headless_console_init(gmb_headless_console_t* c, gmb_text_writer_t linear_out)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->linear_out = linear_out;
    c->fg = 15;
    c->bg = 0;
}
void gmb_headless_console_write(gmb_headless_console_t* c, const char* s)
{
    if (!c || !s) return;
    if (c->linear_out.write) c->linear_out.write(c->linear_out.ctx, s, (int)strlen(s));
}
void gmb_headless_console_write_line(gmb_headless_console_t* c, const char* s)
{
    gmb_headless_console_write(c, s);
    gmb_headless_console_write(c, "\n");
}

/* ===================== SystemConsole (SystemConsole.cs) ===================== */

int gmb_system_console_key_available(void)
{
    /* Console.KeyAvailable: richiede input non-bloccante del terminale. Headless: 0. */
    return 0;
}
int gmb_system_console_read(void)
{
    int c = getchar();
    return c == EOF ? -1 : c;
}
char* gmb_system_console_read_line(void)
{
    gmb_string_t s = gmb_console_readline();
    if (!s.data) return NULL;
    char* r = (char*)malloc(s.length + 1);
    if (r) { memcpy(r, s.data, s.length + 1); }
    gmb_string_free(&s);
    return r;
}
void gmb_system_console_beep(void)
{
    gmb_console_beep();
}
