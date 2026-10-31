/* ============================================================================
 * gmb_tui_clipper.c — emulatore Clipper 5.3 core (porting ClipperLib/Clipper).
 * ============================================================================ */
#include "gmb_tui_clipper.h"
#include "gmb_tui_datatypes.h"
#include "gmb_tui_mouse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===================== ClipperNumbers ===================== */

int gmb_clipper_num_abs_int(int n) { return n < 0 ? -n : n; }
double gmb_clipper_num_abs(double n) { return fabs(n); }

int gmb_clipper_num_int(double n)
{
    return n >= 0 ? (int)n : (int)ceil(n);
}

double gmb_clipper_num_round(double n, int decimals)
{
    if (decimals < 0) decimals = 0;
    if (decimals > 15) decimals = 15;
    double p = pow(10.0, decimals);
    return round(n * p) / p;
}

double gmb_clipper_num_round1(double n) { return gmb_clipper_num_round(n, 0); }
double gmb_clipper_num_sqrt(double n) { return sqrt(n); }
double gmb_clipper_num_exp(double n) { return exp(n); }
double gmb_clipper_num_log(double n) { return log(n); }
double gmb_clipper_num_log10(double n) { return log10(n); }

double gmb_clipper_num_max(const double* vals, int n)
{
    if (!vals || n <= 0) return 0;
    double m = vals[0];
    for (int i = 1; i < n; i++) if (vals[i] > m) m = vals[i];
    return m;
}

double gmb_clipper_num_min(const double* vals, int n)
{
    if (!vals || n <= 0) return 0;
    double m = vals[0];
    for (int i = 1; i < n; i++) if (vals[i] < m) m = vals[i];
    return m;
}

int gmb_clipper_num_min_i(const int* vals, int n)
{
    if (!vals || n <= 0) return 0;
    int m = vals[0];
    for (int i = 1; i < n; i++) if (vals[i] < m) m = vals[i];
    return m;
}

double gmb_clipper_num_mod(double dividend, double divisor)
{
    if (divisor == 0) return 0;
    double r = fmod(dividend, divisor);
    if (r != 0 && (r < 0) != (divisor < 0)) r += divisor;
    return r;
}

int gmb_clipper_num_mod_i(int dividend, int divisor)
{
    if (divisor == 0) return 0;
    int r = dividend % divisor;
    if (r != 0 && (r < 0) != (divisor < 0)) r += divisor;
    return r;
}

/* ===================== ClipperState ===================== */

static int picture_apply_string(const char* value, const char* picture, char* out, int cap);

gmb_clipper_state_t* gmb_clipper_state_new(int width, int height)
{
    gmb_clipper_state_t* st = (gmb_clipper_state_t*)calloc(1, sizeof(gmb_clipper_state_t));
    if (!st) return NULL;
    if (width <= 0) width = 80;
    if (height <= 0) height = 24;
    st->buffer = gmb_screen_new(width, height);
    strcpy(st->color_spec, "W/N");
    st->fg = GMB_CELLCOLOR_WHITE;
    st->bg = GMB_CELLCOLOR_BLACK;
    st->set_escape = true;
    st->set_bell = true;
    st->set_typeahead = 16;
    st->set_intensity = true;
    strcpy(st->set_date_format, "mm-dd-yyyy");
    st->set_epoch = 1900;
    st->set_decimals = -1;
    strcpy(st->printer_device, "LPT1");
    st->blink_on = true;
    st->blink_enabled = true;
    st->cursor_visible = true;
    st->mouse_cursor_visible = true;
    st->get_cap = 16;
    st->gets = (gmb_get_t*)calloc((size_t)st->get_cap, sizeof(gmb_get_t));
    st->get_active = -1;
    return st;
}

void gmb_clipper_state_free(gmb_clipper_state_t* st)
{
    if (!st) return;
    gmb_screen_free(st->buffer);
    free(st->gets);
    free(st);
}

void gmb_clipper_state_clear_buffer(gmb_clipper_state_t* st)
{
    if (st) gmb_screen_clear(st->buffer, st->fg, st->bg);
}

/* ===================== ClipperGlobal ===================== */

static gmb_clipper_state_t* g_clipper_state = NULL;

gmb_clipper_state_t* gmb_clipper_global_state(void)
{
    if (!g_clipper_state)
        g_clipper_state = gmb_clipper_state_new(80, 24);
    return g_clipper_state;
}

gmb_clipper_state_t* gmb_clipper_peek_state(void)
{
    return g_clipper_state;
}

void gmb_clipper_global_set_screen_size(int rows, int cols)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    gmb_screen_resize(st->buffer, cols, rows);
}

void gmb_clipper_global_screen_size(int* rows, int* cols)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (rows) *rows = gmb_screen_height(st->buffer);
    if (cols) *cols = gmb_screen_width(st->buffer);
}

/* ===================== ClipperSet ===================== */

bool gmb_clipper_set_confirm(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->set_confirm = on != 0;
    return st->set_confirm;
}

bool gmb_clipper_set_wrap(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->set_wrap = on != 0;
    return st->set_wrap;
}

const char* gmb_clipper_set_delimiters(gmb_clipper_state_t* st, const char* value)
{
    if (value)
    {
        int vl = (int)strlen(value);
        if (vl == 0) st->set_delimiters[0] = '\0';
        else if (vl == 1) { st->set_delimiters[0] = value[0]; st->set_delimiters[1] = value[0]; st->set_delimiters[2] = '\0'; }
        else { st->set_delimiters[0] = value[0]; st->set_delimiters[1] = value[1]; st->set_delimiters[2] = '\0'; }
    }
    return st->set_delimiters;
}

bool gmb_clipper_set_escape(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->set_escape = on != 0;
    return st->set_escape;
}

bool gmb_clipper_set_bell(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->set_bell = on != 0;
    return st->set_bell;
}

int gmb_clipper_set_typeahead(gmb_clipper_state_t* st, int size)
{
    if (size >= 0) st->set_typeahead = size;
    return st->set_typeahead;
}

bool gmb_clipper_set_intensity(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->set_intensity = on != 0;
    return st->set_intensity;
}

const char* gmb_clipper_set_date_format(gmb_clipper_state_t* st, const char* fmt)
{
    if (fmt && fmt[0])
        snprintf(st->set_date_format, sizeof(st->set_date_format), "%s", fmt);
    return st->set_date_format;
}

bool gmb_clipper_set_century(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->set_century = on != 0;
    return st->set_century;
}

int gmb_clipper_set_epoch(gmb_clipper_state_t* st, int year)
{
    if (year >= 0) st->set_epoch = year;
    return st->set_epoch;
}

void gmb_clipper_set_fg_rgb(gmb_clipper_state_t* st, int r, int g, int b, int is_default)
{
    st->fg.r = (uint8_t)r; st->fg.g = (uint8_t)g; st->fg.b = (uint8_t)b;
    st->fg.is_default = is_default != 0;
}

void gmb_clipper_set_bg_rgb(gmb_clipper_state_t* st, int r, int g, int b, int is_default)
{
    st->bg.r = (uint8_t)r; st->bg.g = (uint8_t)g; st->bg.b = (uint8_t)b;
    st->bg.is_default = is_default != 0;
}

int gmb_clipper_get_fg_packed(gmb_clipper_state_t* st) { return (st->fg.r << 16) | (st->fg.g << 8) | st->fg.b; }
int gmb_clipper_get_bg_packed(gmb_clipper_state_t* st) { return (st->bg.r << 16) | (st->bg.g << 8) | st->bg.b; }
int gmb_clipper_get_fg_default(gmb_clipper_state_t* st) { return st->fg.is_default ? 1 : 0; }
int gmb_clipper_get_bg_default(gmb_clipper_state_t* st) { return st->bg.is_default ? 1 : 0; }

int gmb_clipper_set_decimals(gmb_clipper_state_t* st, int n)
{
    if (n >= 0) st->set_decimals = n;
    return st->set_decimals;
}

bool gmb_clipper_set_fixed(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->set_fixed = on != 0;
    return st->set_fixed;
}

int gmb_clipper_set_format_number(gmb_clipper_state_t* st, double d, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    /* Non finiti: stessa resa di IL/.NET ("NaN", "Infinity", "-Infinity"), senza spazio di segno — printf
     * darebbe "NAN"/"INF". */
    if (isnan(d)) { snprintf(out, cap, "NaN"); return (int)strlen(out); }
    if (isinf(d)) { snprintf(out, cap, d > 0 ? "Infinity" : "-Infinity"); return (int)strlen(out); }
    int n = st->set_decimals;
    char body[128];
    if (n < 0 && !st->set_fixed)
    {
        /* legacy: G15 + segno-spazio */
        snprintf(body, sizeof(body), "%.15G", d);
    }
    else
    {
        if (n < 0) n = 0;
        if (st->set_fixed)
            snprintf(body, sizeof(body), "%.*f", n, d);
        else
        {
            char fmt[16];
            snprintf(fmt, sizeof(fmt), "%%.%df", n);
            snprintf(body, sizeof(body), fmt, d);
            /* rimuovi zeri finali */
            char* dot = strchr(body, '.');
            if (dot)
            {
                char* p = body + strlen(body) - 1;
                while (p > dot && *p == '0') *p-- = '\0';
                if (p == dot) *p = '\0';
            }
        }
    }
    if (d >= 0.0)
        snprintf(out, cap, " %s", body);
    else
        snprintf(out, cap, "%s", body);
    return (int)strlen(out);
}

bool gmb_clipper_set_printer(gmb_clipper_state_t* st, int on)
{
    if (on >= 0) st->printer_on = on != 0;
    return st->printer_on;
}

const char* gmb_clipper_set_device(gmb_clipper_state_t* st, const char* device)
{
    if (device && device[0])
    {
        snprintf(st->printer_device, sizeof(st->printer_device), "%s", device);
        for (char* p = st->printer_device; *p; p++)
            *p = (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
    }
    return st->printer_device;
}

bool gmb_clipper_set_message(gmb_clipper_state_t* st, int row)
{
    if (row >= 0)
    {
        if (row == 0) st->has_message_row = false;
        else { st->has_message_row = true; st->message_row = row; }
    }
    return st->has_message_row;
}

const char* gmb_clipper_set_message_text(gmb_clipper_state_t* st, const char* text)
{
    if (text) snprintf(st->message_text, sizeof(st->message_text), "%s", text);
    return st->message_text;
}

/* ===================== ClipperPrinter ===================== */

int gmb_clipper_printer_prow(gmb_clipper_state_t* st) { return st->printer_row; }
int gmb_clipper_printer_pcol(gmb_clipper_state_t* st) { return st->printer_col; }

void gmb_clipper_printer_eject(gmb_clipper_state_t* st)
{
    st->printer_buffer_len = 0;
    st->printer_row = 0;
    st->printer_col = 0;
    st->printer_on = false;
}

const char* gmb_clipper_printer_text(gmb_clipper_state_t* st)
{
    st->printer_buffer[st->printer_buffer_len] = '\0';
    return st->printer_buffer;
}

bool gmb_clipper_printer_routing(gmb_clipper_state_t* st)
{
    return st->printer_on || strcmp(st->printer_device, "PRINTER") == 0;
}

bool gmb_clipper_printer_only(gmb_clipper_state_t* st)
{
    return strcmp(st->printer_device, "PRINTER") == 0;
}

void gmb_clipper_printer_write(gmb_clipper_state_t* st, const char* s, bool new_line)
{
    if (!gmb_clipper_printer_routing(st)) return;
    if (!s) s = "";
    if (new_line)
    {
        int sl = (int)strlen(s);
        for (int i = 0; i < sl && st->printer_buffer_len < (int)sizeof(st->printer_buffer) - 2; i++)
            st->printer_buffer[st->printer_buffer_len++] = s[i];
        if (st->printer_buffer_len < (int)sizeof(st->printer_buffer) - 1)
            st->printer_buffer[st->printer_buffer_len++] = '\n';
        st->printer_row++;
        st->printer_col = 0;
    }
    else
    {
        int sl = (int)strlen(s);
        for (int i = 0; i < sl && st->printer_buffer_len < (int)sizeof(st->printer_buffer) - 1; i++)
            st->printer_buffer[st->printer_buffer_len++] = s[i];
        st->printer_col += sl;
    }
}

/* ===================== accesso celle (via gmb_screen_cells) ===================== */
static uint32_t cell_char(gmb_clipper_state_t* st, int x, int y)
{
    gmb_screen_cell_t* cells = gmb_screen_cells(st->buffer);
    int w = gmb_screen_width(st->buffer);
    return cells[(size_t)y * (size_t)w + (size_t)x].character;
}
static gmb_cell_color_t cell_fg(gmb_clipper_state_t* st, int x, int y)
{
    gmb_screen_cell_t* cells = gmb_screen_cells(st->buffer);
    int w = gmb_screen_width(st->buffer);
    return cells[(size_t)y * (size_t)w + (size_t)x].fg_rgb;
}
static gmb_cell_color_t cell_bg(gmb_clipper_state_t* st, int x, int y)
{
    gmb_screen_cell_t* cells = gmb_screen_cells(st->buffer);
    int w = gmb_screen_width(st->buffer);
    return cells[(size_t)y * (size_t)w + (size_t)x].bg_rgb;
}

/* ===================== ClipperScrn ===================== */


int gmb_clipper_scrn_max_col(gmb_clipper_state_t* st) { return gmb_screen_width(st->buffer) - 1; }
int gmb_clipper_scrn_max_row(gmb_clipper_state_t* st) { return gmb_screen_height(st->buffer) - 1; }

void gmb_clipper_scrn_scroll(gmb_clipper_state_t* st, int top, int left, int bottom, int right, int rows)
{
    if (rows == 0) return;
    int H = gmb_screen_height(st->buffer);
    int W = gmb_screen_width(st->buffer);
    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom > H - 1) bottom = H - 1;
    if (right > W - 1) right = W - 1;
    if (top > bottom || left > right) return;

    int height = bottom - top + 1;
    int shift = rows;
    if (shift > height) shift = height;
    if (shift < -height) shift = -height;

    int w = right - left + 1;
    for (int y = 0; y < height; y++)
    {
        int i = shift < 0 ? height - 1 - y : y;
        int src_y = i + shift;
        if (src_y >= 0 && src_y < height)
        {
            for (int x = 0; x < w; x++)
                gmb_screen_setchar(st->buffer, left + x, top + i,
                    cell_char(st, left + x, top + src_y),
                    st->fg, st->bg);
        }
        else
        {
            for (int x = 0; x < w; x++)
                gmb_screen_setchar(st->buffer, left + x, top + i, ' ', st->fg, st->bg);
        }
    }
}

int gmb_clipper_scrn_save_screen(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                                 char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int H = gmb_screen_height(st->buffer);
    int W = gmb_screen_width(st->buffer);
    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom > H - 1) bottom = H - 1;
    if (right > W - 1) right = W - 1;
    if (top > bottom || left > right)
    {
        snprintf(out, cap, "S0;0|");
        return (int)strlen(out);
    }
    int w = right - left + 1;
    int h = bottom - top + 1;
    int o = snprintf(out, cap, "S%d;%d|", w, h);
    for (int y = top; y <= bottom && o < cap - 1; y++)
        for (int x = left; x <= right && o < cap - 1; x++)
        {
            uint32_t ch = cell_char(st, x, y);
            gmb_cell_color_t fg = cell_fg(st, x, y);
            gmb_cell_color_t bg = cell_bg(st, x, y);
            if (o < cap - 1) out[o++] = (char)ch;
            if (o < cap - 1) out[o++] = (char)(fg.r + 32);
            if (o < cap - 1) out[o++] = (char)(fg.g + 32);
            if (o < cap - 1) out[o++] = (char)(fg.b + 32);
            if (o < cap - 1) out[o++] = fg.is_default ? '1' : '0';
            if (o < cap - 1) out[o++] = (char)(bg.r + 32);
            if (o < cap - 1) out[o++] = (char)(bg.g + 32);
            if (o < cap - 1) out[o++] = (char)(bg.b + 32);
            if (o < cap - 1) out[o++] = bg.is_default ? '1' : '0';
        }
    out[o] = '\0';
    return o;
}

int gmb_clipper_scrn_save_screen_all(gmb_clipper_state_t* st, char* out, int cap)
{
    return gmb_clipper_scrn_save_screen(st, 0, 0, gmb_screen_height(st->buffer) - 1,
                                        gmb_screen_width(st->buffer) - 1, out, cap);
}

int gmb_clipper_scrn_rest_screen(gmb_clipper_state_t* st, const char* s, int top, int left)
{
    if (!s || !*s || s[0] != 'S') return 0;
    const char* semi = strchr(s, '|');
    if (!semi) return 0;
    char dims[64];
    int dn = (int)(semi - s) - 1;
    if (dn < 0 || dn > 63) return 0;
    memcpy(dims, s + 1, (size_t)dn);
    dims[dn] = '\0';
    char* semi2 = strchr(dims, ';');
    if (!semi2) return 0;
    *semi2 = '\0';
    int w = atoi(dims);
    int h = atoi(semi2 + 1);
    if (w <= 0 || h <= 0) return 0;
    const char* idx = semi + 1;
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
        {
            if (idx + 8 > s + strlen(s)) return 0;
            gmb_cell_color_t fg, bg;
            fg.r = (unsigned char)(idx[1] - 32);
            fg.g = (unsigned char)(idx[2] - 32);
            fg.b = (unsigned char)(idx[3] - 32);
            fg.is_default = idx[4] == '1';
            bg.r = (unsigned char)(idx[5] - 32);
            bg.g = (unsigned char)(idx[6] - 32);
            bg.b = (unsigned char)(idx[7] - 32);
            bg.is_default = idx[8] == '1';
            gmb_screen_setchar(st->buffer, left + dx, top + dy, (unsigned char)idx[0], fg, bg);
            idx += 9;
        }
    return 1;
}

const char* gmb_clipper_scrn_tcolor(gmb_clipper_state_t* st) { return st->color_spec; }

bool gmb_clipper_scrn_set_blink(gmb_clipper_state_t* st, int on)
{
    if (on >= 0)
    {
        st->blink_enabled = on != 0;
        if (!on) st->blink_on = false;
    }
    return st->blink_enabled;
}

void gmb_clipper_scrn_prompt(gmb_clipper_state_t* st, int row, int col, const char* text)
{
    if (st->has_message_row && st->message_row == row)
    {
        gmb_cell_color_t fg = st->fg, bg = st->bg;
        if (st->message_text[0])
        {
            char blank[512];
            int bl = (int)strlen(st->message_text);
            memset(blank, ' ', (size_t)bl);
            blank[bl] = '\0';
            gmb_screen_setstring(st->buffer, st->message_col, st->message_row, blank, fg, bg);
        }
        gmb_screen_setstring(st->buffer, col, st->message_row, text, fg, bg);
        st->message_col = col;
    }
    snprintf(st->message_text, sizeof(st->message_text), "%s", text ? text : "");
}

/* ===================== ClipperOutput ===================== */

void gmb_clipper_output_set_color(gmb_clipper_state_t* st, const char* spec)
{
    snprintf(st->color_spec, sizeof(st->color_spec), "%s", spec ? spec : "W/N");
    const char* comma = strchr(st->color_spec, ',');
    char pair[64];
    if (comma)
    {
        int pn = (int)(comma - st->color_spec);
        if (pn > 63) pn = 63;
        memcpy(pair, st->color_spec, (size_t)pn);
        pair[pn] = '\0';
    }
    else
        snprintf(pair, sizeof(pair), "%s", st->color_spec);
    /* trim */
    {
        char* b = pair;
        while (*b == ' ' || *b == '\t') b++;
        int l = (int)strlen(b);
        while (l > 0 && (b[l - 1] == ' ' || b[l - 1] == '\t')) b[--l] = '\0';
        gmb_color_parse_pair(b, &st->fg, &st->bg);
    }
}

int gmb_clipper_output_row(gmb_clipper_state_t* st) { return st->row; }
int gmb_clipper_output_col(gmb_clipper_state_t* st) { return st->col; }

void gmb_clipper_output_set_pos(gmb_clipper_state_t* st, int row, int col)
{
    st->row = row;
    st->col = col;
}

void gmb_clipper_output_say(gmb_clipper_state_t* st, int row, int col, const char* expr)
{
    int nx = gmb_screen_setstring_n(st->buffer, col, row, expr, INT32_MAX, st->fg, st->bg);
    st->row = row;
    st->col = nx;
}

void gmb_clipper_output_say_pic(gmb_clipper_state_t* st, int row, int col, const char* expr,
                                const char* picture)
{
    char formatted[1024];
    picture_apply_string(expr, picture, formatted, (int)sizeof(formatted));
    gmb_clipper_output_say(st, row, col, formatted);
}

void gmb_clipper_output_say_color(gmb_clipper_state_t* st, int row, int col, const char* expr,
                                  const char* color)
{
    gmb_cell_color_t fg, bg;
    gmb_color_parse_pair(color, &fg, &bg);
    int nx = gmb_screen_setstring_n(st->buffer, col, row, expr, INT32_MAX, fg, bg);
    st->row = row;
    st->col = nx;
}

void gmb_clipper_output_dev_out(gmb_clipper_state_t* st, const char* s)
{
    if (!gmb_clipper_printer_only(st))
    {
        int nx = gmb_screen_setstring_n(st->buffer, st->col, st->row, s, INT32_MAX, st->fg, st->bg);
        st->col = nx;
    }
    gmb_clipper_printer_write(st, s, false);
}

void gmb_clipper_output_dev_out_pic(gmb_clipper_state_t* st, const char* s, const char* pic)
{
    char formatted[1024];
    picture_apply_string(s, pic, formatted, (int)sizeof(formatted));
    gmb_clipper_output_dev_out(st, formatted);
}

void gmb_clipper_output_qout(gmb_clipper_state_t* st, const char* s)
{
    if (!gmb_clipper_printer_only(st))
    {
        gmb_screen_setstring_n(st->buffer, st->col, st->row, s, INT32_MAX, st->fg, st->bg);
        if (st->row + 1 < gmb_screen_height(st->buffer))
            st->row++;
        st->col = 0;
    }
    gmb_clipper_printer_write(st, s, true);
}

void gmb_clipper_output_qqout(gmb_clipper_state_t* st, const char* s)
{
    gmb_clipper_output_dev_out(st, s);
}

void gmb_clipper_output_text_block(gmb_clipper_state_t* st, const char* s)
{
    char copy[2048];
    snprintf(copy, sizeof(copy), "%s", s ? s : "");
    char* line = copy;
    while (line)
    {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        gmb_clipper_output_qout(st, line);
        if (!nl) break;
        line = nl + 1;
    }
}

/* ParseBorder: ritorna i 9 caratteri */
static void parse_border(const char* border, char out[9], char* fill)
{
    if (border && border[0] && strlen(border) >= 1)
    {
        int bl = (int)strlen(border);
        if (bl == 1)
        {
            char c = border[0];
            for (int i = 0; i < 8; i++) out[i] = c;
            *fill = ' ';
            return;
        }
        if (bl >= 9)
        {
            for (int i = 0; i < 8; i++) out[i] = border[i];
            *fill = border[8];
            return;
        }
    }
    /* default single: ┌─┐│┘─└│ */
    out[0] = 0xCC; out[1] = 0xC4; out[2] = 0xB9;   /* ┌ ─ ┐  (CP437) */
    out[3] = 0xB3; out[4] = 0xD9; out[5] = 0xC4;   /* │ ┘ ─ */
    out[6] = 0xC0; out[7] = 0xB3;                   /* └ │ */
    *fill = ' ';
}

static void draw_box_impl(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                          const char* border)
{
    char bc[9]; char fill;
    parse_border(border, bc, &fill);
    gmb_screen_setchar(st->buffer, left, top, (unsigned char)bc[0], st->fg, st->bg);
    gmb_screen_setchar(st->buffer, right, top, (unsigned char)bc[2], st->fg, st->bg);
    gmb_screen_setchar(st->buffer, left, bottom, (unsigned char)bc[6], st->fg, st->bg);
    gmb_screen_setchar(st->buffer, right, bottom, (unsigned char)bc[4], st->fg, st->bg);
    for (int x = left + 1; x < right; x++)
    {
        gmb_screen_setchar(st->buffer, x, top, (unsigned char)bc[1], st->fg, st->bg);
        gmb_screen_setchar(st->buffer, x, bottom, (unsigned char)bc[5], st->fg, st->bg);
    }
    for (int y = top + 1; y < bottom; y++)
    {
        gmb_screen_setchar(st->buffer, left, y, (unsigned char)bc[3], st->fg, st->bg);
        gmb_screen_setchar(st->buffer, right, y, (unsigned char)bc[7], st->fg, st->bg);
    }
    int fi_top = top + 1 > 0 ? top + 1 : 0;
    int fi_left = left + 1 > 0 ? left + 1 : 0;
    int fi_bottom = bottom - 1 > 0 ? bottom - 1 : 0;
    int fi_right = right - 1 > 0 ? right - 1 : 0;
    if (fi_top <= fi_bottom && fi_left <= fi_right)
        gmb_screen_fillrect(st->buffer, fi_top, fi_left, fi_bottom, fi_right, fill, st->fg, st->bg);
}

void gmb_clipper_output_box(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                            const char* style)
{
    const char* border = (style && style[0]) ? style : NULL;
    draw_box_impl(st, top, left, bottom, right, border);
}

void gmb_clipper_output_disp_box(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                                 const char* spec)
{
    const char* border = (spec && spec[0]) ? spec : NULL;
    draw_box_impl(st, top, left, bottom, right, border);
}

void gmb_clipper_output_clear_screen(gmb_clipper_state_t* st)
{
    gmb_screen_clear(st->buffer, st->fg, st->bg);
}

void gmb_clipper_output_clear_region(gmb_clipper_state_t* st, int top, int left, int bottom, int right)
{
    gmb_screen_fillrect(st->buffer, top, left, bottom, right, ' ', st->fg, st->bg);
}

void gmb_clipper_output_set_cursor(gmb_clipper_state_t* st, bool on) { st->cursor_visible = on; }
bool gmb_clipper_output_get_cursor(gmb_clipper_state_t* st) { return st->cursor_visible; }

/* ===================== ClipperInput (headless) ===================== */

void gmb_clipper_input_keyboard(gmb_clipper_state_t* st, const char* s)
{
    if (!s) return;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++)
    {
        if (st->typeahead_count >= GMB_CLIPPER_TYPEAHEAD_MAX) break;
        st->typeahead[st->typeahead_tail] = *p;
        st->typeahead_tail = (st->typeahead_tail + 1) % GMB_CLIPPER_TYPEAHEAD_MAX;
        st->typeahead_count++;
    }
}

static int typeahead_pop(gmb_clipper_state_t* st)
{
    if (st->typeahead_count == 0) return -1;
    int b = st->typeahead[st->typeahead_head];
    st->typeahead_head = (st->typeahead_head + 1) % GMB_CLIPPER_TYPEAHEAD_MAX;
    st->typeahead_count--;
    return b;
}

int gmb_clipper_input_next_key(gmb_clipper_state_t* st)
{
    if (st->has_pending) return st->pending_key;
    if (st->typeahead_count > 0)
    {
        int b = st->typeahead[st->typeahead_head];
        switch (b)
        {
            case '\n': case '\r': return GMB_K_ENTER;
            case 27: return GMB_K_ESC;
            case 9: return GMB_K_TAB;
            case 127: return GMB_K_BS;
            default: return b;
        }
    }
    return 0;
}

int gmb_clipper_input_inkey(gmb_clipper_state_t* st)
{
    int k;
    if (st->has_pending)
    {
        k = st->pending_key;
        st->has_pending = false;
    }
    else
    {
        int b = typeahead_pop(st);
        if (b < 0) return 0;
        switch (b)
        {
            case '\n': case '\r': k = GMB_K_ENTER; break;
            case 27: k = GMB_K_ESC; break;
            case 9: k = GMB_K_TAB; break;
            case 127: case 8: k = GMB_K_BS; break;
            default: k = b; break;
        }
    }
    st->last_key = k;
    st->read_key = k;
    return k;
}

int gmb_clipper_input_last_key(gmb_clipper_state_t* st) { return st->last_key; }
int gmb_clipper_input_read_key(gmb_clipper_state_t* st) { return st->read_key; }

void gmb_clipper_input_set_key(gmb_clipper_state_t* st, int key, void (*fn)(void* ctx), void* ctx)
{
    for (int i = 0; i < 16; i++)
    {
        if (st->set_key_used[i] && st->set_key_codes[i] == key)
        {
            st->set_key_fns[i] = fn;
            st->set_key_ctxs[i] = ctx;
            return;
        }
        if (!st->set_key_used[i])
        {
            st->set_key_codes[i] = key;
            st->set_key_fns[i] = fn;
            st->set_key_ctxs[i] = ctx;
            st->set_key_used[i] = true;
            return;
        }
    }
}

/* ===================== Get / ClipperRead ===================== */

void gmb_get_init(gmb_get_t* g)
{
    memset(g, 0, sizeof(*g));
    g->fill = ' ';
    g->valid_address = 0;
}

int gmb_clipper_read_get_width(const gmb_get_t* g)
{
    if (g->has_width) return g->width;
    return (int)strlen(g->value);
}

bool gmb_clipper_read_is_valid(const gmb_get_t* g)
{
    if (g->valid && !g->valid(g->valid_ctx, g->value)) return false;
    if (g->has_range)
    {
        const char* v = g->value;
        while (*v == ' ' || *v == '\t') v++;
        char* end = NULL;
        double n = strtod(v, &end);
        bool ok = (end && end != v);
        if (!ok && v[0] == '\0') { n = 0; ok = true; }
        if (!ok) return false;
        return n >= g->range_lo && n <= g->range_hi;
    }
    return true;
}

int gmb_clipper_input_accept(gmb_clipper_state_t* st, char* buf, int cap)
{
    if (!buf || cap <= 0) return 0;
    buf[0] = '\0';
    int blen = 0;
    int start_row = st->row, start_col = st->col;
    while (true)
    {
        int c = st->col;
        int r = st->row;
        int w = gmb_screen_width(st->buffer);
        gmb_cell_color_t fg = st->fg, bg = st->bg;
        for (int x = c; x < w; x++)
            gmb_screen_setchar(st->buffer, x, r, ' ', fg, bg);
        gmb_screen_setstring_n(st->buffer, c, r, buf, INT32_MAX, fg, bg);

        int k = gmb_clipper_input_inkey(st);
        if (k == 0) continue;
        if (k == GMB_K_ENTER) return 1;
        if (k == GMB_K_ESC) return 0;
        if (k == GMB_K_BS) { if (blen > 0) blen--; }
        else if (k >= 32 && k <= 126 && blen < cap - 1) buf[blen++] = (char)k;
        buf[blen] = '\0';
        st->row = start_row;
        st->col = start_col;
    }
}

/* ===================== READ: picture editing helpers ===================== */

static bool read_is_placeholder(char t)
{
    return t == 'A' || t == 'N' || t == 'X' || t == '9' || t == '#' ||
           t == 'L' || t == 'Y' || t == '!' || t == '0';
}

static int read_placeholder_count(const gmb_picture_spec_t* p)
{
    int n = 0;
    if (!p->template) return 0;
    for (char* t = p->template; *t; t++)
        if (read_is_placeholder(*t)) n++;
    return n;
}

static bool read_has_placeholders(const gmb_picture_spec_t* p)
{
    if (!p->template) return false;
    for (char* t = p->template; *t; t++)
        if (read_is_placeholder(*t)) return true;
    return false;
}

static char read_placeholder_type_at(const gmb_picture_spec_t* p, int raw_idx)
{
    if (!p->template) return '\0';
    int n = 0;
    for (char* t = p->template; *t; t++)
    {
        if (!read_is_placeholder(*t)) continue;
        if (n == raw_idx) return *t;
        n++;
    }
    return '\0';
}

static bool read_template_accepts(const gmb_picture_spec_t* p, char ch, int raw_idx)
{
    char t = read_placeholder_type_at(p, raw_idx);
    if (t == '\0') return !read_has_placeholders(p);
    switch (t)
    {
        case 'A': return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        case 'N': return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        case 'X': case '!': return true;
        case '9': case '0': return ch >= '0' && ch <= '9';
        case '#': return (ch >= '0' && ch <= '9') || ch == '-' || ch == '+';
        case 'L': { char u = (char)((ch >= 'a' && ch <= 'z') ? ch - 32 : ch); return u == 'T' || u == 'F'; }
        case 'Y': { char u = (char)((ch >= 'a' && ch <= 'z') ? ch - 32 : ch); return u == 'Y' || u == 'N'; }
        default: return true;
    }
}

static int read_map_to_display(const gmb_picture_spec_t* p, int raw_idx)
{
    if (!p->template) return 0;
    int n = -1;
    for (char* t = p->template; *t; t++)
    {
        if (read_is_placeholder(*t))
        {
            n++;
            if (n == raw_idx) return (int)(t - p->template);
        }
    }
    return (int)strlen(p->template);
}

/* Apply(string value, string picture) — seleziona numerico/testuale come il C# */
static int picture_apply_string(const char* value, const char* picture, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    gmb_picture_spec_t spec = gmb_picture_parse(picture);
    int r;
    if (spec.is_numeric && !spec.literal_insert)
    {
        const char* v = value;
        while (*v == ' ' || *v == '\t') v++;
        char* end = NULL;
        double n = strtod(v, &end);
        bool ok = (end && end != v);
        for (const char* p = end; *p; p++)
            if (!(*p == ' ' || *p == '\t')) { ok = false; break; }
        if (ok)
            r = gmb_picture_apply_number(n, picture, out, cap);
        else
            r = gmb_picture_apply_text(value, picture, out, cap);
    }
    else
        r = gmb_picture_apply_text(value, picture, out, cap);
    gmb_picture_free(&spec);
    return r;
}

/* ===================== READ: disegno dei GET ===================== */

static void read_draw_get(gmb_clipper_state_t* st, int idx)
{
    gmb_get_t* g = &st->gets[idx];
    gmb_picture_spec_t pic;
    bool has_pic = g->has_picture;
    if (has_pic) pic = gmb_picture_parse(g->picture);

    char disp[1024];
    if (g->control_type[0] && strcmp(g->control_type, "CheckBox") == 0)
        snprintf(disp, sizeof(disp), "%s%s", (g->value[0] == '1' ? "[X] " : "[ ] "), g->caption);
    else if (has_pic)
        picture_apply_string(g->value, g->picture, disp, (int)sizeof(disp));
    else
        snprintf(disp, sizeof(disp), "%s", g->value);

    gmb_cell_color_t base_fg = st->fg, base_bg = st->bg;
    if (g->color[0]) gmb_color_parse_pair(g->color, &base_fg, &base_bg);

    bool valid = gmb_clipper_read_is_valid(g);
    bool active = st->get_active == idx;

    gmb_cell_color_t fg = base_fg, bg = base_bg;
    if (!valid && g->error_color[0]) gmb_color_parse_pair(g->error_color, &fg, &bg);
    else if (active && g->focus_color[0]) gmb_color_parse_pair(g->focus_color, &fg, &bg);

    int clear_w = gmb_clipper_read_get_width(g);
    if (clear_w > 0)
    {
        char blank[1024];
        int bl = clear_w < 1023 ? clear_w : 1023;
        memset(blank, g->fill, (size_t)bl);
        blank[bl] = '\0';
        gmb_screen_setstring(st->buffer, g->col, g->row, blank, fg, bg);
    }

    const char* delims = st->set_delimiters;
    if (active && delims[0])
    {
        gmb_screen_setchar(st->buffer, g->col - 1, g->row, (unsigned char)delims[0], fg, bg);
        gmb_screen_setchar(st->buffer, g->col + clear_w, g->row, (unsigned char)delims[1], fg, bg);
    }

    int clen = (int)strlen(disp);
    int view_start = 0;
    int view_len = clear_w;

    if (has_pic && pic.scroll)
    {
        if (g->cur < g->scroll_pos) g->scroll_pos = g->cur;
        if (g->cur >= g->scroll_pos + clear_w) g->scroll_pos = g->cur - clear_w + 1;
        if (g->scroll_pos > 0)
        {
            int m = clen - clear_w;
            if (m < 0) m = 0;
            g->scroll_pos = g->scroll_pos < m ? g->scroll_pos : m;
        }
        view_start = g->scroll_pos < clen ? g->scroll_pos : clen;
        view_len = clear_w < (clen - view_start > 0 ? clen - view_start : 0) ? clear_w : (clen - view_start > 0 ? clen - view_start : 0);
        if (view_len > 0)
        {
            char vwin[1024];
            memcpy(vwin, disp + view_start, (size_t)view_len);
            vwin[view_len] = '\0';
            gmb_screen_setstring(st->buffer, g->col, g->row, vwin, fg, bg);
        }
    }
    else
    {
        int sel_n = g->sel < clen ? g->sel : clen;
        if (sel_n > 0)
        {
            char s1[1024];
            memcpy(s1, disp, (size_t)sel_n);
            s1[sel_n] = '\0';
            gmb_screen_setstring(st->buffer, g->col, g->row, s1, bg, fg);
        }
        if (sel_n < clen)
        {
            char s2[1024];
            snprintf(s2, sizeof(s2), "%s", disp + sel_n);
            gmb_screen_setstring(st->buffer, g->col + sel_n, g->row, s2, fg, bg);
        }
    }

    if (active && st->blink_on && st->cursor_visible)
    {
        int vlen = (int)strlen(g->value);
        int display_cur = (has_pic && pic.literal_insert)
            ? read_map_to_display(&pic, g->cur < vlen ? g->cur : vlen)
            : g->cur;
        int cx = g->col + (display_cur - view_start);
        if (cx >= g->col && cx < g->col + clear_w)
        {
            int idx_in_view = display_cur - view_start;
            bool over_sel = (has_pic && !pic.literal_insert) && display_cur < (g->sel < clen ? g->sel : clen);
            char c = ' ';
            if (idx_in_view < view_len && view_start + idx_in_view < clen)
                c = disp[view_start + idx_in_view];
            char cb[2] = { c, '\0' };
            gmb_screen_setstring(st->buffer, cx, g->row, cb, over_sel ? fg : bg, over_sel ? bg : fg);
        }
        else if (cx < g->col)
            gmb_screen_setstring(st->buffer, g->col, g->row, " ", bg, fg);
        else if (cx >= g->col + clear_w)
            gmb_screen_setstring(st->buffer, g->col + clear_w - 1, g->row, " ", bg, fg);
    }

    if (has_pic) gmb_picture_free(&pic);
    st->row = g->row;
    st->col = g->col + clear_w;
}

static void read_draw_all_gets(gmb_clipper_state_t* st)
{
    for (int i = 0; i < st->get_count; i++)
        read_draw_get(st, i);
}

void gmb_clipper_read_add_get(gmb_clipper_state_t* st, const gmb_get_t* g)
{
    if (st->get_count >= st->get_cap)
    {
        int newcap = st->get_cap * 2;
        gmb_get_t* ng = (gmb_get_t*)realloc(st->gets, (size_t)newcap * sizeof(gmb_get_t));
        if (!ng) return;
        st->gets = ng;
        st->get_cap = newcap;
    }
    st->gets[st->get_count++] = *g;
}

int gmb_clipper_read_get_active(gmb_clipper_state_t* st) { return st->get_active; }

int gmb_clipper_read_get_at(gmb_clipper_state_t* st, int col, int row)
{
    for (int i = 0; i < st->get_count; i++)
    {
        gmb_get_t* g = &st->gets[i];
        int w = gmb_clipper_read_get_width(g);
        if (g->row == row && col >= g->col && col < g->col + w)
            return i;
    }
    return -1;
}

const char* gmb_clipper_read_read_var(gmb_clipper_state_t* st) { return st->read_var; }

void gmb_clipper_read_reset_gets(gmb_clipper_state_t* st)
{
    st->get_count = 0;
    st->get_active = -1;
    st->updated = false;
    st->read_key = 0;
    st->last_key = 0;
}

void gmb_clipper_read_apply_all(gmb_clipper_state_t* st)
{
    for (int i = 0; i < st->get_count; i++)
    {
        gmb_get_t* g = &st->gets[i];
        if (g->apply) g->apply(g->apply_ctx, g->value);
        g->updated = true;
        g->changed = true;
    }
    st->updated = true;
}

void gmb_clipper_read_draw_gets(gmb_clipper_state_t* st)
{
    if (st->get_count == 0) return;
    if (st->get_active < 0) st->get_active = 0;
    int i = st->get_active;
    st->gets[i].cur = 0;
    st->gets[i].sel = (int)strlen(st->gets[i].value);
    st->blink_on = true;
    read_draw_all_gets(st);
}

bool gmb_clipper_read_updated(gmb_clipper_state_t* st) { return st->updated; }
int  gmb_clipper_read_read_exit(gmb_clipper_state_t* st) { return st->read_exit_key; }

static void read_mark_changed(gmb_clipper_state_t* st, gmb_get_t* g, int idx)
{
    g->changed = true;
    st->updated = true;
    read_draw_get(st, idx);
}

static void read_step_control(gmb_clipper_state_t* st, gmb_get_t* g, int idx, int dir)
{
    const char* ctrl = g->control_type;
    if (strcmp(ctrl, "spinner") == 0)
    {
        double min = g->has_min ? g->min : -1e308;
        double max = g->has_max ? g->max : 1e308;
        double step = g->has_step ? g->step : 1.0;
        double cur = atof(g->value);
        if (cur == 0.0 && (g->value[0] != '0' || g->value[1])) cur = min;
        double nv = cur + dir * step;
        if (nv < min) nv = min;
        if (nv > max) nv = max;
        snprintf(g->value, sizeof(g->value), "%.17G", nv);
        g->cur = (int)strlen(g->value);
        g->sel = 0;
    }
    else if (strcmp(ctrl, "combobox") == 0 || strcmp(ctrl, "listbox") == 0)
    {
        if (g->items_count > 0)
        {
            int cur_idx = -1;
            for (int i = 0; i < g->items_count; i++)
                if (strcmp(g->items[i], g->value) == 0) { cur_idx = i; break; }
            int ni = cur_idx + dir;
            if (ni < 0) ni = g->items_count - 1;
            if (ni >= g->items_count) ni = 0;
            snprintf(g->value, sizeof(g->value), "%s", g->items[ni]);
            g->cur = (int)strlen(g->value);
            g->sel = 0;
        }
    }
    read_mark_changed(st, g, idx);
}

int gmb_clipper_read_read(gmb_clipper_state_t* st)
{
    if (st->get_count == 0) { st->read_key = 0; return 0; }
    if (st->get_active < 0) st->get_active = 0;
    int i = st->get_active;
    st->gets[i].cur = 0;
    st->gets[i].sel = (int)strlen(st->gets[i].value);

    while (true)
    {
        if (st->get_count == 0) { st->read_key = 0; return 0; }
        int idx = st->get_active < 0 ? 0 : st->get_active;
        if (idx >= st->get_count) idx = 0;

        gmb_get_t* g = &st->gets[idx];

        /* WHEN: salta il campo se il predicato è false */
        if (g->when && !g->when(g->when_ctx))
        {
            int next = idx + 1;
            st->get_active = next >= st->get_count ? 0 : next;
            continue;
        }

        st->row = g->row;
        st->col = g->col;
        read_draw_all_gets(st);
        st->get_active = idx;

        int k = gmb_clipper_input_inkey(st);
        if (k == 0) continue;

        /* SET KEY handlers */
        for (int si = 0; si < 16; si++)
        {
            if (st->set_key_used[si] && st->set_key_codes[si] == k && st->set_key_fns[si])
                st->set_key_fns[si](st->set_key_ctxs[si]);
        }

        int g_idx = st->get_active < 0 ? idx : st->get_active;
        g = &st->gets[g_idx];
        gmb_picture_spec_t pic;
        bool has_pic = g->has_picture;
        if (has_pic) pic = gmb_picture_parse(g->picture);
        const char* ctrl = g->control_type;
        bool is_step_control = strcmp(ctrl, "spinner") == 0 || strcmp(ctrl, "combobox") == 0 || strcmp(ctrl, "listbox") == 0;
        int max_len = (has_pic && pic.literal_insert)
            ? read_placeholder_count(&pic)
            : (g->has_width ? g->width : INT32_MAX);

        switch (k)
        {
            case GMB_K_ENTER:
            {
                bool ok = gmb_clipper_read_is_valid(g);
                if (ok)
                {
                    int next = g_idx + 1;
                    if (next >= st->get_count)
                    {
                        if (st->set_confirm) { continue; }
                        st->blink_on = false;
                        read_draw_all_gets(st);
                        gmb_clipper_read_apply_all(st);
                        st->read_key = 0;
                        st->read_exit_key = GMB_K_ENTER;
                        st->get_active = -1;
                        if (has_pic) gmb_picture_free(&pic);
                        return 0;
                    }
                    if (g->apply) g->apply(g->apply_ctx, g->value);
                    g->updated = true;
                    g->changed = true;
                    st->updated = true;
                    st->gets[next].cur = 0;
                    st->gets[next].sel = (int)strlen(st->gets[next].value);
                    st->get_active = next;
                }
                break;
            }

            case GMB_K_ESC:
                if (!st->set_escape) break;
                st->blink_on = false;
                read_draw_all_gets(st);
                st->read_key = 6;
                st->read_exit_key = GMB_K_ESC;
                st->get_active = -1;
                if (has_pic) gmb_picture_free(&pic);
                return 6;

            case GMB_K_BS:
                if (g->sel > 0)
                {
                    snprintf(g->value, sizeof(g->value), "%s", g->value + g->sel);
                    g->cur = 0;
                    g->sel = 0;
                    read_mark_changed(st, g, g_idx);
                }
                else if (g->cur > 0)
                {
                    g->cur--;
                    memmove(g->value + g->cur, g->value + g->cur + 1, strlen(g->value + g->cur + 1) + 1);
                    read_mark_changed(st, g, g_idx);
                }
                break;

            case GMB_K_DEL:
                if (g->sel > 0)
                {
                    snprintf(g->value, sizeof(g->value), "%s", g->value + g->sel);
                    g->cur = 0;
                    g->sel = 0;
                    read_mark_changed(st, g, g_idx);
                }
                else if (g->cur < (int)strlen(g->value))
                {
                    memmove(g->value + g->cur, g->value + g->cur + 1, strlen(g->value + g->cur + 1) + 1);
                    read_mark_changed(st, g, g_idx);
                }
                break;

            case GMB_K_LEFT:
                g->sel = 0;
                if (g->cur > 0) g->cur--;
                else if (st->set_wrap && !(has_pic && pic.literal_insert))
                {
                    int m = (int)strlen(g->value);
                    if (m > max_len) m = max_len;
                    g->cur = m;
                }
                break;

            case GMB_K_RIGHT:
                g->sel = 0;
                {
                    int m = (int)strlen(g->value);
                    if (m > max_len) m = max_len;
                    if (g->cur < m) g->cur++;
                    else if (st->set_wrap && !(has_pic && pic.literal_insert)) g->cur = 0;
                }
                break;

            case GMB_K_HOME:
                g->sel = 0;
                g->cur = 0;
                break;

            case GMB_K_END:
                g->sel = 0;
                {
                    int m = (int)strlen(g->value);
                    if (m > max_len) m = max_len;
                    g->cur = m;
                }
                break;

            case GMB_K_TAB:
            case GMB_K_DOWN:
                if (is_step_control) { read_step_control(st, g, g_idx, 1); break; }
                g->sel = 0;
                g->cur = 0;
                {
                    int nxt = g_idx + 1 >= st->get_count ? 0 : g_idx + 1;
                    st->gets[nxt].cur = 0;
                    st->gets[nxt].sel = (int)strlen(st->gets[nxt].value);
                    st->get_active = nxt;
                }
                break;

            case GMB_K_UP:
            case GMB_K_BTAB:
                if (is_step_control) { read_step_control(st, g, g_idx, -1); break; }
                g->sel = 0;
                g->cur = 0;
                {
                    int prv = g_idx == 0 ? st->get_count - 1 : g_idx - 1;
                    st->gets[prv].cur = 0;
                    st->gets[prv].sel = (int)strlen(st->gets[prv].value);
                    st->get_active = prv;
                }
                break;

            default:
                if (k >= 32 && k <= 126)
                {
                    char ch = (char)k;
                    if (ctrl[0] && strcmp(ctrl, "checkbox") == 0 && ch == ' ')
                    {
                        g->value[0] = g->value[0] == '1' ? '0' : '1';
                        g->value[1] = '\0';
                        g->cur = (int)strlen(g->value);
                        g->sel = 0;
                        read_mark_changed(st, g, g_idx);
                        break;
                    }
                    bool refused = false;
                    if (has_pic)
                    {
                        if (pic.alpha && !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))) refused = true;
                        else if (pic.upper && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
                    }
                    if (!refused && has_pic && pic.template && pic.template[0] &&
                        !read_template_accepts(&pic, ch, g->cur))
                        refused = true;

                    if (!refused && g->accept_key && !g->accept_key(g->accept_key_ctx, ch))
                        refused = true;

                    if (refused)
                    {
                        if (g->rejected) g->rejected(g->rejected_ctx, ch);
                        continue;
                    }

                    if (has_pic && pic.literal_insert)
                    {
                        if ((int)strlen(g->value) >= max_len) continue;
                        int at = g->cur < (int)strlen(g->value) ? g->cur : (int)strlen(g->value);
                        memmove(g->value + at + 1, g->value + at, strlen(g->value + at) + 1);
                        g->value[at] = ch;
                        g->cur = g->cur + 1 < max_len ? g->cur + 1 : max_len;
                        g->sel = 0;
                    }
                    else if (has_pic && pic.insert_mode)
                    {
                        if ((int)strlen(g->value) >= max_len) continue;
                        int at = g->cur < (int)strlen(g->value) ? g->cur : (int)strlen(g->value);
                        memmove(g->value + at + 1, g->value + at, strlen(g->value + at) + 1);
                        g->value[at] = ch;
                        g->cur++;
                        g->sel = 0;
                    }
                    else
                    {
                        int vlen = (int)strlen(g->value);
                        if (g->sel > 0)
                        {
                            g->value[0] = ch;
                            snprintf(g->value + 1, sizeof(g->value) - 1, "%s", g->value + g->sel);
                            g->cur = 1;
                            g->sel = 0;
                        }
                        else if (g->cur < vlen)
                        {
                            g->value[g->cur] = ch;
                            g->cur++;
                        }
                        else
                        {
                            if (vlen >= max_len) continue;
                            g->value[vlen] = ch;
                            g->value[vlen + 1] = '\0';
                            g->cur++;
                        }
                    }
                    read_mark_changed(st, g, g_idx);
                }
                break;
        }
        if (has_pic) gmb_picture_free(&pic);
    }
}

/* ===================== READ: builder per test/uso semplice ===================== */

/* Aggiunge un Get con parametri base. Ritorna l'indice, -1 se pieno. */
int gmb_clipper_read_add_field(gmb_clipper_state_t* st, int row, int col, const char* value,
                               const char* picture, bool has_range, double lo, double hi)
{
    if (st->get_count >= st->get_cap)
    {
        int newcap = st->get_cap * 2;
        gmb_get_t* ng = (gmb_get_t*)realloc(st->gets, (size_t)newcap * sizeof(gmb_get_t));
        if (!ng) return -1;
        st->gets = ng;
        st->get_cap = newcap;
    }
    gmb_get_t* g = &st->gets[st->get_count];
    gmb_get_init(g);
    g->row = row;
    g->col = col;
    if (value) snprintf(g->value, sizeof(g->value), "%s", value);
    if (picture && picture[0])
    {
        snprintf(g->picture, sizeof(g->picture), "%s", picture);
        g->has_picture = true;
    }
    g->fill = ' ';
    if (has_range)
    {
        g->has_range = true;
        g->range_lo = lo;
        g->range_hi = hi;
    }
    return st->get_count++;
}

/* Imposta il callback APPLY (assegnazione della variabile READ). */
void gmb_clipper_read_set_field_apply(gmb_clipper_state_t* st, int idx,
                                      void (*apply)(void* ctx, const char* value), void* ctx)
{
    if (idx >= 0 && idx < st->get_count)
    {
        st->gets[idx].apply = apply;
        st->gets[idx].apply_ctx = ctx;
    }
}

/* Imposta la larghezza e il tipo controllo (opzionali). */
void gmb_clipper_read_set_field_options(gmb_clipper_state_t* st, int idx, int width,
                                        const char* control_type, const char* const* items, int items_count,
                                        double min, double max, double step,
                                        bool has_min, bool has_max, bool has_step)
{
    if (idx < 0 || idx >= st->get_count) return;
    gmb_get_t* g = &st->gets[idx];
    if (width >= 0) { g->has_width = true; g->width = width; }
    if (control_type && control_type[0]) snprintf(g->control_type, sizeof(g->control_type), "%s", control_type);
    if (items && items_count > 0)
    {
        g->items_count = items_count > 16 ? 16 : items_count;
        for (int i = 0; i < g->items_count; i++)
            snprintf(g->items[i], sizeof(g->items[i]), "%s", items[i] ? items[i] : "");
    }
    if (has_min) { g->has_min = true; g->min = min; }
    if (has_max) { g->has_max = true; g->max = max; }
    if (has_step) { g->has_step = true; g->step = step; }
}

gmb_get_t* gmb_clipper_read_get_ptr(gmb_clipper_state_t* st, int idx)
{
    if (!st || idx < 0 || idx >= st->get_count) return NULL;
    return &st->gets[idx];
}

void gmb_clipper_read_set_field_colors(gmb_clipper_state_t* st, int idx,
                                       const char* color, const char* focus_color,
                                       const char* error_color, const char* message)
{
    if (idx < 0 || idx >= st->get_count) return;
    gmb_get_t* g = &st->gets[idx];
    if (color) snprintf(g->color, sizeof(g->color), "%s", color);
    if (focus_color) snprintf(g->focus_color, sizeof(g->focus_color), "%s", focus_color);
    if (error_color) snprintf(g->error_color, sizeof(g->error_color), "%s", error_color);
    if (message) snprintf(g->message, sizeof(g->message), "%s", message);
}

void gmb_clipper_read_set_field_validator(gmb_clipper_state_t* st, int idx,
                                          bool (*valid)(void* ctx, const char* value), void* ctx)
{
    if (idx < 0 || idx >= st->get_count) return;
    st->gets[idx].valid = valid;
    st->gets[idx].valid_ctx = ctx;
}

void gmb_clipper_read_set_field_accept(gmb_clipper_state_t* st, int idx,
                                       bool (*accept)(void* ctx, char ch), void* ctx)
{
    if (idx < 0 || idx >= st->get_count) return;
    st->gets[idx].accept_key = accept;
    st->gets[idx].accept_key_ctx = ctx;
}

void gmb_clipper_read_set_field_rejected(gmb_clipper_state_t* st, int idx,
                                         void (*rejected)(void* ctx, char ch), void* ctx)
{
    if (idx < 0 || idx >= st->get_count) return;
    st->gets[idx].rejected = rejected;
    st->gets[idx].rejected_ctx = ctx;
}

void gmb_clipper_read_set_field_when(gmb_clipper_state_t* st, int idx,
                                     bool (*when)(void* ctx), void* ctx)
{
    if (idx < 0 || idx >= st->get_count) return;
    st->gets[idx].when = when;
    st->gets[idx].when_ctx = ctx;
}

/* GetBuilder legacy: serializza un Get base come "row;col;value" (utilità di lettura) */
int gmb_clipper_get_builder(int row, int col, const char* value, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    return snprintf(out, cap, "%d;%d;%s", row, col, value ? value : "");
}

/* ===================== ClipperDates ===================== */

#include <time.h>

gmb_clipper_date_t gmb_clipper_dates_today(void)
{
    gmb_clipper_date_t dt = { 1, 1, 1 };
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (t) { dt.y = t->tm_year + 1900; dt.m = t->tm_mon + 1; dt.d = t->tm_mday; }
    return dt;
}

int gmb_clipper_dates_time(char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (t)
        snprintf(out, cap, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    else
        out[0] = '\0';
    return (int)strlen(out);
}

int gmb_clipper_dates_seconds(void)
{
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (!t) return 0;
    return t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
}

int gmb_clipper_dates_year(int y, int m, int d) { (void)m; (void)d; return y; }
int gmb_clipper_dates_month(int y, int m, int d) { (void)y; (void)d; return m; }
int gmb_clipper_dates_day(int y, int m, int d) { (void)y; (void)m; return d; }

int gmb_clipper_dates_dow(int y, int m, int d)
{
    /* gmb_dates_weekday: 0=Sunday..6=Saturday; DOW Clipper = +1 */
    return gmb_dates_weekday(y, m, d) + 1;
}

int gmb_clipper_dates_cdow(int y, int m, int d, char* out, int cap)
{
    static const char* const names[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
    int wd = gmb_dates_weekday(y, m, d);
    snprintf(out, cap, "%s", names[wd]);
    return (int)strlen(out);
}

int gmb_clipper_dates_cmonth(int y, int m, int d, char* out, int cap)
{
    (void)y; (void)d;
    static const char* const names[12] = { "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December" };
    if (m < 1 || m > 12) { out[0] = '\0'; return 0; }
    snprintf(out, cap, "%s", names[m - 1]);
    return (int)strlen(out);
}

static int dates_format_date(gmb_clipper_date_t d, const char* format, bool century, char* out, int cap)
{
    int o = 0;
    int fl = (int)strlen(format);
    for (int i = 0; i < fl;)
    {
        char c = format[i];
        char lc = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
        if (lc == 'm' || lc == 'd' || lc == 'y')
        {
            int j = i;
            while (j < fl)
            {
                char cj = format[j];
                char lcj = (char)((cj >= 'A' && cj <= 'Z') ? cj + 32 : cj);
                if (lcj != lc) break;
                j++;
            }
            int len = j - i;
            char buf[16];
            switch (lc)
            {
                case 'm':
                    if (len >= 3) snprintf(buf, sizeof(buf), "%s", "MMM");
                    else snprintf(buf, sizeof(buf), "%02d", d.m);
                    break;
                case 'd':
                    if (len >= 3) snprintf(buf, sizeof(buf), "%s", "ddd");
                    else snprintf(buf, sizeof(buf), "%02d", d.d);
                    break;
                case 'y':
                    if (len >= 4 || century) snprintf(buf, sizeof(buf), "%04d", d.y);
                    else snprintf(buf, sizeof(buf), "%02d", d.y % 100);
                    break;
                default: buf[0] = '\0'; break;
            }
            /* per MMM/ddd usiamo i nomi C# invariant */
            if (lc == 'm' && len >= 3) { gmb_clipper_dates_cmonth(d.y, d.m, d.d, buf, (int)sizeof(buf)); buf[3] = '\0'; }
            else if (lc == 'd' && len >= 3) { gmb_clipper_dates_cdow(d.y, d.m, d.d, buf, (int)sizeof(buf)); buf[3] = '\0'; }
            for (char* p = buf; *p && o < cap - 1; p++) out[o++] = *p;
            i = j;
        }
        else
        {
            if (o < cap - 1) out[o++] = c;
            i++;
        }
    }
    out[o] = '\0';
    return o;
}

int gmb_clipper_dates_dtoc(gmb_clipper_state_t* st, int y, int m, int d, char* out, int cap)
{
    gmb_clipper_date_t dt; dt.y = y; dt.m = m; dt.d = d;
    return dates_format_date(dt, st->set_date_format, st->set_century, out, cap);
}

int gmb_clipper_dates_dtos(int y, int m, int d, char* out, int cap)
{
    snprintf(out, cap, "%04d%02d%02d", y, m, d);
    return (int)strlen(out);
}

/* Tokenize: estrae token kind ("mm"/"dd"/"yy"/"yyyy"/"lit") */
typedef struct date_tok { char kind[8]; char lit[8]; } date_tok_t;

static int dates_tokenize(const char* format, date_tok_t* toks, int max)
{
    int n = 0;
    int fl = (int)strlen(format);
    for (int i = 0; i < fl;)
    {
        char c = format[i];
        char lc = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
        if (lc == 'm' || lc == 'd' || lc == 'y')
        {
            int j = i;
            while (j < fl)
            {
                char cj = format[j];
                char lcj = (char)((cj >= 'A' && cj <= 'Z') ? cj + 32 : cj);
                if (lcj != lc) break;
                j++;
            }
            int len = j - i;
            date_tok_t* t = &toks[n++];
            if (lc == 'y' && len >= 4) snprintf(t->kind, sizeof(t->kind), "yyyy");
            else if (lc == 'y') snprintf(t->kind, sizeof(t->kind), "yy");
            else if (lc == 'm') snprintf(t->kind, sizeof(t->kind), "mm");
            else snprintf(t->kind, sizeof(t->kind), "dd");
            i = j;
        }
        else
        {
            date_tok_t* t = &toks[n++];
            snprintf(t->kind, sizeof(t->kind), "lit");
            t->lit[0] = c; t->lit[1] = '\0';
            i++;
        }
    }
    return n;
}

int gmb_clipper_dates_ctod(gmb_clipper_state_t* st, const char* s, gmb_clipper_date_t* out)
{
    if (!s || !*s) return 0;
    date_tok_t toks[32];
    int ntok = dates_tokenize(st->set_date_format, toks, 32);
    /* digit runs di s */
    int runs[8];
    int nruns = 0;
    int run_len = 0;
    int sl = (int)strlen(s);
    for (int i = 0; i <= sl; i++)
    {
        if (i < sl && s[i] >= '0' && s[i] <= '9') { run_len++; continue; }
        if (run_len > 0 && nruns < 8) runs[nruns++] = run_len;
        run_len = 0;
    }
    /* digit tokens */
    int dk[8];
    char dkind[8][8];
    int ndk = 0;
    for (int i = 0; i < ntok && ndk < 8; i++)
        if (strcmp(toks[i].kind, "lit") != 0)
        {
            snprintf(dkind[ndk], sizeof(dkind[ndk]), "%s", toks[i].kind);
            dk[ndk] = i;
            ndk++;
        }
    if (nruns < ndk) return 0;

    int mm = 1, dd = 1, yyyy = 0;
    bool has_year = false;
    int pos = 0;
    for (int i = 0; i < ndk; i++)
    {
        int len = runs[i];
        char digits[8];
        /* estrai i digits dell'i-esimo run, avanzando da pos */
        int count = 0;
        int p2 = pos;
        while (p2 < sl && !(s[p2] >= '0' && s[p2] <= '9')) p2++;  /* skip separatori */
        while (p2 < sl && s[p2] >= '0' && s[p2] <= '9')
        {
            if (count < (int)sizeof(digits) - 1) digits[count++] = s[p2];
            p2++;
        }
        digits[count] = '\0';
        pos = p2;
        if (strcmp(dkind[i], "mm") == 0)
        {
            if (len != 2) return 0;
            mm = atoi(digits);
        }
        else if (strcmp(dkind[i], "dd") == 0)
        {
            if (len != 2) return 0;
            dd = atoi(digits);
        }
        else if (strcmp(dkind[i], "yyyy") == 0)
        {
            has_year = true;
            if (len != 4) return 0;
            yyyy = atoi(digits);
        }
        else if (strcmp(dkind[i], "yy") == 0)
        {
            has_year = true;
            if (len != 2) return 0;
            int yy = atoi(digits);
            int epoch = st->set_epoch;
            yyyy = epoch - (epoch % 100) + yy;
            if (yyyy < epoch) yyyy += 100;
        }
    }

    if (!has_year)
    {
        gmb_clipper_date_t today = gmb_clipper_dates_today();
        yyyy = today.y;
    }
    if (mm < 1 || mm > 12 || dd < 1 || dd > gmb_dates_days_in_month(yyyy, mm)) return 0;
    if (out) { out->y = yyyy; out->m = mm; out->d = dd; }
    return 1;
}

/* ===================== MouseCapture (stub) ===================== */

static bool g_mouse_enabled = false;
static bool g_mouse_motion = false;

void gmb_clipper_mouse_enable(void)
{
    g_mouse_enabled = true;
    g_mouse_motion = false;
}

void gmb_clipper_mouse_enable_motion(void)
{
    g_mouse_motion = true;
    g_mouse_enabled = true;
}

void gmb_clipper_mouse_disable(void)
{
    g_mouse_enabled = false;
    g_mouse_motion = false;
}

/* ===================== Confun (parte TTY-free) ===================== */

int gmb_confun_center(const char* s, int width, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int len = (int)strlen(s);
    if (width <= 0 || len >= width) { snprintf(out, cap, "%s", s); return (int)strlen(out); }
    int left = (width - len) / 2;
    int right = width - len - left;
    int o = 0;
    for (int i = 0; i < left && o < cap - 1; i++) out[o++] = ' ';
    for (int i = 0; i < len && o < cap - 1; i++) out[o++] = s[i];
    for (int i = 0; i < right && o < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

int gmb_confun_right(const char* s, int width, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int len = (int)strlen(s);
    if (width <= 0 || len >= width) { snprintf(out, cap, "%s", s); return (int)strlen(out); }
    int pad = width - len;
    int o = 0;
    for (int i = 0; i < pad && o < cap - 1; i++) out[o++] = ' ';
    for (int i = 0; i < len && o < cap - 1; i++) out[o++] = s[i];
    out[o] = '\0';
    return o;
}

int gmb_confun_left(const char* s, int width, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int len = (int)strlen(s);
    if (width <= 0) { snprintf(out, cap, "%s", s); return (int)strlen(out); }
    if (len >= width) { return gmb_confun_clip(s, width, out, cap); }
    int o = 0;
    for (int i = 0; i < len && o < cap - 1; i++) out[o++] = s[i];
    for (int i = len; i < width && o < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

int gmb_confun_clip(const char* s, int width, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (width <= 0) { out[0] = '\0'; return 0; }
    if (!s) s = "";
    int len = (int)strlen(s);
    if (len <= width) { snprintf(out, cap, "%s", s); return len; }
    int m = width < cap - 1 ? width : cap - 1;
    memcpy(out, s, (size_t)m);
    out[m] = '\0';
    return m;
}

/* glyph box: "┌─┐│┘─└│ " (indici come BORDER_SINGLE C#) */
static const char* const CONFUN_BOX[9] = {
    "\xE2\x94\x8C", "\xE2\x94\x80", "\xE2\x94\x90", "\xE2\x94\x82",
    "\xE2\x94\x98", "\xE2\x94\x80", "\xE2\x94\x94", "\xE2\x94\x82", " "
};

int gmb_confun_boxed(const char* s, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int len = (int)strlen(s);
    int o = 0;
    /* top = tl + h*(len+2) + tr */
    for (const char* p = CONFUN_BOX[0]; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = 0; i < len + 2 && o < cap - 1; i++)
        for (const char* p = CONFUN_BOX[1]; *p && o < cap - 1; p++) out[o++] = *p;
    for (const char* p = CONFUN_BOX[2]; *p && o < cap - 1; p++) out[o++] = *p;
    if (o < cap - 1) out[o++] = '\n';
    /* mid = v + " " + s + " " + v */
    for (const char* p = CONFUN_BOX[3]; *p && o < cap - 1; p++) out[o++] = *p;
    if (o < cap - 1) out[o++] = ' ';
    for (int i = 0; i < len && o < cap - 1; i++) out[o++] = s[i];
    if (o < cap - 1) out[o++] = ' ';
    for (const char* p = CONFUN_BOX[3]; *p && o < cap - 1; p++) out[o++] = *p;
    if (o < cap - 1) out[o++] = '\n';
    /* bot = bl + h*(len+2) + br ; bl = box[5]="─", br = box[6]="└" (fedele al C#) */
    for (const char* p = CONFUN_BOX[5]; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = 0; i < len + 2 && o < cap - 1; i++)
        for (const char* p = CONFUN_BOX[1]; *p && o < cap - 1; p++) out[o++] = *p;
    for (const char* p = CONFUN_BOX[6]; *p && o < cap - 1; p++) out[o++] = *p;
    out[o] = '\0';
    return o;
}

int gmb_confun_bold(const char* s, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int o = 0;
    for (const char* p = GMB_ANSI_BOLD; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = 0; i < (int)strlen(s) && o < cap - 1; i++) out[o++] = s[i];
    for (const char* p = GMB_ANSI_RESET; *p && o < cap - 1; p++) out[o++] = *p;
    out[o] = '\0';
    return o;
}

int gmb_confun_fgrgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    char pre[64];
    gmb_ansi_fgrgb(r, g, b, pre, sizeof(pre));
    int o = 0;
    for (char* p = pre; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = 0; i < (int)strlen(s) && o < cap - 1; i++) out[o++] = s[i];
    for (const char* p = GMB_ANSI_RESET; *p && o < cap - 1; p++) out[o++] = *p;
    out[o] = '\0';
    return o;
}

int gmb_confun_bgrgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    char pre[64];
    gmb_ansi_bgrgb(r, g, b, pre, sizeof(pre));
    int o = 0;
    for (char* p = pre; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = 0; i < (int)strlen(s) && o < cap - 1; i++) out[o++] = s[i];
    for (const char* p = GMB_ANSI_RESET; *p && o < cap - 1; p++) out[o++] = *p;
    out[o] = '\0';
    return o;
}

void gmb_confun_print(const char* s)
{
    if (s) fputs(s, stdout);
}

/* ===================== ClipperMemoEdit (headless) ===================== */

#define MEMO_MAX_LINES 64
#define MEMO_MAX_LEN   512

static int memo_text_width(int width, int height, int line_count)
{
    return line_count > height ? width - 1 : width;
}

static void memo_draw(gmb_clipper_state_t* st, int row, int col, int width, int height,
                      const char lines[MEMO_MAX_LINES][MEMO_MAX_LEN], int line_count,
                      int cur_line, int cur_col, int scroll_offset, int left_offset)
{
    int text_w = memo_text_width(width, height, line_count);
    char blank[512];
    int bl = text_w < 511 ? text_w : 511;
    memset(blank, ' ', (size_t)bl);
    blank[bl] = '\0';
    for (int y = 0; y < height; y++)
        gmb_screen_setstring(st->buffer, col, row + y, blank, st->fg, st->bg);

    for (int y = 0; y < height; y++)
    {
        int line_idx = scroll_offset + y;
        if (line_idx >= line_count) break;
        const char* line = lines[line_idx];
        int ll = (int)strlen(line);
        if (left_offset > 0 && left_offset < ll)
            line += left_offset;
        else if (left_offset >= ll)
            line = "";
        char shown[512];
        snprintf(shown, sizeof(shown), "%s", line);
        int sn = (int)strlen(shown);
        if (sn > text_w) shown[text_w] = '\0';
        gmb_screen_setstring(st->buffer, col, row + y, shown, st->fg, st->bg);
    }

    if (line_count > height)
    {
        int bar_x = col + width - 1;
        int range = line_count > 1 ? line_count : 1;
        int thumb_h = height * height / range;
        if (thumb_h < 1) thumb_h = 1;
        int scroll_space = line_count - height;
        if (scroll_space < 1) scroll_space = 1;
        int thumb_y = (height - thumb_h) * scroll_offset / scroll_space;
        for (int y = 0; y < height; y++)
        {
            uint32_t ch = (y >= thumb_y && y < thumb_y + thumb_h) ? 0x2588 : 0x2502;
            gmb_screen_setchar(st->buffer, bar_x, row + y, ch, st->fg, st->bg);
        }
    }

    if (st->blink_on)
    {
        int vis_y = cur_line - scroll_offset;
        if (vis_y >= 0 && vis_y < height)
        {
            int cx = col + (cur_col - left_offset);
            int cy = row + vis_y;
            int text_w2 = memo_text_width(width, height, line_count);
            if (cx >= col && cx < col + text_w2)
            {
                int clen = (int)strlen(lines[cur_line]);
                uint32_t ch = cur_col < clen ? (unsigned char)lines[cur_line][cur_col] : ' ';
                gmb_screen_setchar(st->buffer, cx, cy, ch, st->bg, st->fg);
            }
        }
    }
}

int gmb_clipper_memo_edit(gmb_clipper_state_t* st, const char* text, int row, int col,
                          int width, int height, char* out, int cap)
{
    char lines[MEMO_MAX_LINES][MEMO_MAX_LEN];
    int line_count = 0;
    char copy[8192];
    snprintf(copy, sizeof(copy), "%s", text ? text : "");
    if (copy[0] == '\0') { strcpy(lines[0], ""); line_count = 1; }
    else
    {
        char* line = copy;
        while (line && line_count < MEMO_MAX_LINES)
        {
            char* nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            snprintf(lines[line_count++], MEMO_MAX_LEN, "%s", line);
            if (!nl) break;
            line = nl + 1;
        }
    }

    int cur_line = 0, cur_col = 0, scroll_offset = 0, left_offset = 0;
    while (true)
    {
        memo_draw(st, row, col, width, height, lines, line_count, cur_line, cur_col,
                  scroll_offset, left_offset);
        int k = gmb_clipper_input_inkey(st);
        if (k == 0) continue;

        if (k == GMB_K_ENTER || k == GMB_K_ESC) break;

        if (k == GMB_K_BS)
        {
            if (cur_col > 0)
            {
                memmove(lines[cur_line] + cur_col - 1, lines[cur_line] + cur_col,
                        strlen(lines[cur_line] + cur_col) + 1);
                cur_col--;
            }
            else if (cur_line > 0)
            {
                cur_col = (int)strlen(lines[cur_line - 1]);
                strncat(lines[cur_line - 1], lines[cur_line], MEMO_MAX_LEN - strlen(lines[cur_line - 1]) - 1);
                for (int i = cur_line; i < line_count - 1; i++)
                    snprintf(lines[i], MEMO_MAX_LEN, "%s", lines[i + 1]);
                line_count--;
                cur_line--;
            }
        }
        else if (k == GMB_K_UP)
        {
            if (cur_line > 0) cur_line--;
            int cl = (int)strlen(lines[cur_line]);
            if (cur_col > cl) cur_col = cl;
            if (cur_line < scroll_offset) scroll_offset = cur_line;
        }
        else if (k == GMB_K_DOWN)
        {
            if (cur_line < line_count - 1) cur_line++;
            int cl = (int)strlen(lines[cur_line]);
            if (cur_col > cl) cur_col = cl;
            if (cur_line >= scroll_offset + height) scroll_offset = cur_line - height + 1;
        }
        else if (k == GMB_K_LEFT)
        {
            if (cur_col > 0) cur_col--;
        }
        else if (k == GMB_K_RIGHT)
        {
            int cl = (int)strlen(lines[cur_line]);
            if (cur_col < cl) cur_col++;
        }
        else if (k == GMB_K_HOME)
        {
            cur_col = 0;
        }
        else if (k == GMB_K_END)
        {
            cur_col = (int)strlen(lines[cur_line]);
        }
        else if (k >= 32 && k <= 126)
        {
            int cl = (int)strlen(lines[cur_line]);
            if (cl + 1 < MEMO_MAX_LEN)
            {
                memmove(lines[cur_line] + cur_col + 1, lines[cur_line] + cur_col, (size_t)(cl - cur_col) + 1);
                lines[cur_line][cur_col] = (char)k;
                cur_col++;
            }
        }

        int text_w = memo_text_width(width, height, line_count);
        if (cur_col < left_offset) left_offset = cur_col;
        if (cur_col >= left_offset + text_w) left_offset = cur_col - text_w + 1;
    }

    int o = 0;
    for (int i = 0; i < line_count; i++)
    {
        int ll = (int)strlen(lines[i]);
        for (int j = 0; j < ll && o < cap - 1; j++) out[o++] = lines[i][j];
        if (i < line_count - 1 && o < cap - 1) out[o++] = '\n';
    }
    out[o] = '\0';
    return o;
}

/* ===================== ClipperAChoice (headless) ===================== */

int gmb_clipper_achoice(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                        const char* const* items, int items_count)
{
    if (!items || items_count <= 0) return 0;
    int sel = 0;
    /* bordi single-line: ┌ ─ ┐ │ ┘ ─ └ │ (come BORDER_SINGLE) */
    while (true)
    {
        gmb_screen_setchar(st->buffer, left, top, 0x250C, st->fg, st->bg);
        gmb_screen_setchar(st->buffer, right, top, 0x2510, st->fg, st->bg);
        gmb_screen_setchar(st->buffer, left, bottom, 0x2514, st->fg, st->bg);
        gmb_screen_setchar(st->buffer, right, bottom, 0x2518, st->fg, st->bg);
        for (int x = left + 1; x < right; x++)
        {
            gmb_screen_setchar(st->buffer, x, top, 0x2500, st->fg, st->bg);
            gmb_screen_setchar(st->buffer, x, bottom, 0x2500, st->fg, st->bg);
        }
        for (int y = top + 1; y < bottom; y++)
        {
            gmb_screen_setchar(st->buffer, left, y, 0x2502, st->fg, st->bg);
            gmb_screen_setchar(st->buffer, right, y, 0x2502, st->fg, st->bg);
        }
        int max_rows = bottom - top - 1;
        for (int i = 0; i < items_count && i <= max_rows; i++)
        {
            int y = top + 1 + i;
            gmb_cell_color_t fg = i == sel ? st->bg : st->fg;
            gmb_cell_color_t bg = i == sel ? st->fg : st->bg;
            char line[600];
            snprintf(line, sizeof(line), "%s%s", "  ", items[i]);
            gmb_screen_setstring(st->buffer, left + 1, y, line, fg, bg);
        }

        int k = gmb_clipper_input_inkey(st);
        if (k == 0) continue;
        switch (k)
        {
            case GMB_K_UP: case GMB_K_LEFT:
                sel = sel == 0 ? items_count - 1 : sel - 1;
                break;
            case GMB_K_DOWN: case GMB_K_RIGHT:
                sel = (sel + 1) % items_count;
                break;
            case GMB_K_ENTER:
                return sel + 1;
            case GMB_K_ESC:
                return 0;
            default:
                break;
        }
    }
}

/* ===================== ClipperMouse ===================== */

bool gmb_clipper_mouse_is_mouse(gmb_clipper_state_t* st) { return st->mouse_present; }
int gmb_clipper_mouse_mrow(gmb_clipper_state_t* st) { return st->mouse_row; }
int gmb_clipper_mouse_mcol(gmb_clipper_state_t* st) { return st->mouse_col; }
bool gmb_clipper_mouse_mleft(gmb_clipper_state_t* st) { return st->mouse_down[0]; }
bool gmb_clipper_mouse_mright(gmb_clipper_state_t* st) { return st->mouse_down[2]; }
bool gmb_clipper_mouse_mmiddle(gmb_clipper_state_t* st) { return st->mouse_down[1]; }
bool gmb_clipper_mouse_mleftpress(gmb_clipper_state_t* st) { return st->mouse_pressed[0]; }
bool gmb_clipper_mouse_mrightpress(gmb_clipper_state_t* st) { return st->mouse_pressed[2]; }
bool gmb_clipper_mouse_mmiddlepress(gmb_clipper_state_t* st) { return st->mouse_pressed[1]; }
bool gmb_clipper_mouse_mleftless(gmb_clipper_state_t* st) { return st->mouse_released[0]; }
bool gmb_clipper_mouse_mrightless(gmb_clipper_state_t* st) { return st->mouse_released[2]; }
bool gmb_clipper_mouse_mmiddleless(gmb_clipper_state_t* st) { return st->mouse_released[1]; }
bool gmb_clipper_mouse_mleftshift(gmb_clipper_state_t* st) { return st->mouse_shift[0]; }
bool gmb_clipper_mouse_mrightshift(gmb_clipper_state_t* st) { return st->mouse_shift[2]; }
bool gmb_clipper_mouse_mmiddleshift(gmb_clipper_state_t* st) { return st->mouse_shift[1]; }
bool gmb_clipper_mouse_mleftcontrol(gmb_clipper_state_t* st) { return st->mouse_ctrl[0]; }
bool gmb_clipper_mouse_mrightcontrol(gmb_clipper_state_t* st) { return st->mouse_ctrl[2]; }
bool gmb_clipper_mouse_mmiddlecontrol(gmb_clipper_state_t* st) { return st->mouse_ctrl[1]; }
bool gmb_clipper_mouse_mleftm(gmb_clipper_state_t* st) { return st->mouse_down[0] && st->mouse_moved; }
bool gmb_clipper_mouse_mrightm(gmb_clipper_state_t* st) { return st->mouse_down[2] && st->mouse_moved; }
bool gmb_clipper_mouse_mmiddlem(gmb_clipper_state_t* st) { return st->mouse_down[1] && st->mouse_moved; }

bool gmb_clipper_mouse_moustat(gmb_clipper_state_t* st, int button, int* row, int* col, int* mods)
{
    if (row) *row = st->mouse_row;
    if (col) *col = st->mouse_col;
    if (mods)
    {
        *mods = (gmb_clipper_mouse_mleftshift(st) ? 1 : 0)
              | (gmb_clipper_mouse_mleftcontrol(st) ? 2 : 0);
    }
    switch (button)
    {
        case 1: return gmb_clipper_mouse_mleft(st);
        case 2: return gmb_clipper_mouse_mright(st);
        case 3: return gmb_clipper_mouse_mmiddle(st);
        default:
            return st->mouse_down[0] || st->mouse_down[1] || st->mouse_down[2];
    }
}

void gmb_clipper_mouse_mposition(gmb_clipper_state_t* st, int* row, int* col)
{
    if (row) st->mouse_row = *row;
    if (col) st->mouse_col = *col;
}

bool gmb_clipper_mouse_mousecursor(gmb_clipper_state_t* st, int* row, int* col, const char* type)
{
    if (row) st->mouse_row = *row;
    if (col) st->mouse_col = *col;
    if (type)
    {
        char t[16];
        snprintf(t, sizeof(t), "%s", type);
        for (char* p = t; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        if (strcmp(t, "off") == 0 || strcmp(t, "hide") == 0 || strcmp(t, "hidden") == 0)
            st->mouse_cursor_visible = false;
        else if (strcmp(t, "on") == 0 || strcmp(t, "show") == 0 || strcmp(t, "visible") == 0)
            st->mouse_cursor_visible = true;
        else
            snprintf(st->mouse_cursor_shape, sizeof(st->mouse_cursor_shape), "%s", t);
    }
    return st->mouse_cursor_visible;
}

int gmb_clipper_mouse_mrelease(gmb_clipper_state_t* st, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    for (int b = 0; b < 3; b++)
    {
        if (st->mouse_down[b] || st->mouse_pressed[b])
        {
            st->mouse_down[b] = false;
            st->mouse_pressed[b] = false;
            st->mouse_released[b] = true;
            st->mouse_moved = false;
            const char* s = b == 0 ? "L" : (b == 2 ? "R" : "M");
            snprintf(out, cap, "%s", s);
            return 1;
        }
    }
    out[0] = '\0';
    return 0;
}

void gmb_clipper_mouse_update_from_event(gmb_clipper_state_t* st, const gmb_mouse_action_t* ma)
{
    st->mouse_present = true;
    for (int i = 0; i < 3; i++) { st->mouse_pressed[i] = false; st->mouse_released[i] = false; }
    st->mouse_double_clicked = false;
    st->mouse_wheel_delta = 0;
    st->mouse_row = ma->y;
    st->mouse_col = ma->x;
    st->mouse_moved = false;

    switch (ma->kind)
    {
        case GMB_MA_CLICK: case GMB_MA_DOUBLECLICK: case GMB_MA_RIGHTCLICK: case GMB_MA_MIDDLECLICK:
        {
            int b = (ma->kind == GMB_MA_RIGHTCLICK) ? 2 : (ma->kind == GMB_MA_MIDDLECLICK ? 1 : 0);
            st->mouse_down[b] = true;
            st->mouse_pressed[b] = true;
            st->last_click_x = ma->x;
            st->last_click_y = ma->y;
            st->mouse_double_clicked = (ma->kind == GMB_MA_DOUBLECLICK);
            break;
        }
        case GMB_MA_DRAG:
            st->mouse_moved = true;
            break;
        case GMB_MA_RELEASE:
            for (int b = 0; b < 3; b++)
                if (st->mouse_down[b]) { st->mouse_down[b] = false; st->mouse_released[b] = true; }
            break;
        case GMB_MA_SCROLLUP:
            st->mouse_wheel_delta = 1;
            break;
        case GMB_MA_SCROLLDOWN:
            st->mouse_wheel_delta = -1;
            break;
        default:
            break;
    }
}

int gmb_clipper_mouse_mousex(gmb_clipper_state_t* st) { return st->mouse_col; }
int gmb_clipper_mouse_mousey(gmb_clipper_state_t* st) { return st->mouse_row; }

/* Variante per il BRIDGE C# (P/Invoke): costruisce l'azione (mods=0) e aggiorna lo stato
 * (evita il marshalling di gmb_mouse_action_t a C#). kind = GMB_MA_*, (x,y) = coordinate. */
void gmb_clipper_mouse_update_simple(gmb_clipper_state_t* st, int kind, int x, int y)
{
    gmb_mouse_action_t ma; ma.kind = kind; ma.x = x; ma.y = y; ma.mods = 0;
    gmb_clipper_mouse_update_from_event(st, &ma);
}

int gmb_clipper_mouse_mousebutton(gmb_clipper_state_t* st, int has_id, int button_id)
{
    if (has_id)
    {
        int b = button_id < 0 ? 0 : (button_id > 2 ? 2 : button_id);
        return st->mouse_down[b] ? 1 : 0;
    }
    return (st->mouse_down[0] ? 1 : 0) | (st->mouse_down[1] ? 2 : 0) | (st->mouse_down[2] ? 4 : 0);
}

int gmb_clipper_mouse_mousewheel(gmb_clipper_state_t* st) { return st->mouse_wheel_delta; }
bool gmb_clipper_mouse_mouseclick(gmb_clipper_state_t* st) { return st->mouse_pressed[0]; }

bool gmb_clipper_mouse_mouseclick_area(gmb_clipper_state_t* st, int x1, int y1, int x2, int y2)
{
    int min_x = x1 < x2 ? x1 : x2;
    int max_x = x1 > x2 ? x1 : x2;
    int min_y = y1 < y2 ? y1 : y2;
    int max_y = y1 > y2 ? y1 : y2;
    return st->last_click_x >= min_x && st->last_click_x <= max_x &&
           st->last_click_y >= min_y && st->last_click_y <= max_y;
}
/* MOUSE.CLICK / MOUSE.CLICK(area): wrappers con la stessa semantica di
 * IlRuntime.MouseClick → ClipperMouseClick/Area (una sola sorgente, §0bis). */
double gmb_mouse_click(void) { return gmb_clipper_mouse_mouseclick(gmb_clipper_global_state()) ? -1.0 : 0.0; }
double gmb_mouse_click_area(int x1, int y1, int x2, int y2)
{
    return gmb_clipper_mouse_mouseclick_area(gmb_clipper_global_state(), x1, y1, x2, y2) ? -1.0 : 0.0;
}

bool gmb_clipper_mouse_mousedblclick(gmb_clipper_state_t* st) { return st->mouse_double_clicked; }
bool gmb_clipper_mouse_mousemove(gmb_clipper_state_t* st) { return st->mouse_moved; }
bool gmb_clipper_mouse_mousepresent(gmb_clipper_state_t* st) { return st->mouse_present; }
bool gmb_clipper_mouse_mouseleft(gmb_clipper_state_t* st) { return st->mouse_down[0]; }
bool gmb_clipper_mouse_mouseright(gmb_clipper_state_t* st) { return st->mouse_down[2]; }
bool gmb_clipper_mouse_mousemiddle(gmb_clipper_state_t* st) { return st->mouse_down[1]; }

/* ── MOUSE getters (MOUSE.BUTTON/WHEEL/DBLCLICK/PRESENT/MIDDLE) ──────────────────
 * Parità ClipperMouse/oracolo IlRuntime: stato del mouse TUI corrente (0 in headless).
 * Condivisi da VM/C (gmb_bf_393), C99Native (§0bis: una sola
 * implementazione). Lo stato vive nel clipper globale (gmb_clipper_global_state). */
double gmb_mouse_button(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    return (st->mouse_down[0] || st->mouse_down[1] || st->mouse_down[2]) ? -1.0 : 0.0;
}
double gmb_mouse_wheel(void)
{
    return (double)gmb_clipper_mouse_mousewheel(gmb_clipper_global_state());
}
double gmb_mouse_middle(void)
{
    return gmb_clipper_mouse_mousemiddle(gmb_clipper_global_state()) ? -1.0 : 0.0;
}
double gmb_mouse_present(void)
{
    return gmb_clipper_mouse_mousepresent(gmb_clipper_global_state()) ? -1.0 : 0.0;
}
double gmb_mouse_dblclick(void)
{
    return gmb_clipper_mouse_mousedblclick(gmb_clipper_global_state()) ? -1.0 : 0.0;
}
/* MOUSE.LEFT/RIGHT/MOVE: accessor mancanti nel runtime condiviso (VMC non li instradava:
 * 401/402/399 cadevano a 0). Booleani canonici -1/0 come gli altri MOUSE.*. */
double gmb_mouse_left(void)
{
    return gmb_clipper_mouse_mouseleft(gmb_clipper_global_state()) ? -1.0 : 0.0;
}
double gmb_mouse_right(void)
{
    return gmb_clipper_mouse_mouseright(gmb_clipper_global_state()) ? -1.0 : 0.0;
}
double gmb_mouse_move(void)
{
    return gmb_clipper_mouse_mousemove(gmb_clipper_global_state()) ? -1.0 : 0.0;
}
