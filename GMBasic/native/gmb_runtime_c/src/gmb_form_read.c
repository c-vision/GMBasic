/* gmb_form_read.c — form Clipper pixel: SAY/GET (buffer campi).
 *  Il loop modale READ (`gmb_form_read`) e il rendering diretto dei campi sono stati
 *  RIMOSSI (2026-10-09): il path READ è migrato al modello unificato CONTROLS+park
 *  (gmb_read_run / IlRuntime.ReadRun); l'emissione @SAY/@GET dei backend usa le funzioni
 *  residue (gmb_form_say/get/clear). */
#include "gmb_form_read.h"
#include "gmb_form.h"
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FR_MAX_SAY 64
#define FR_MAX_FIELD 32
#define FR_MAX_ITEMS 16

enum { F_TEXT = 0, F_CHECK, F_SPIN, F_LIST, F_COMBO, F_COLOR };

typedef struct { int row, col; char text[160]; } fr_say_t;
typedef struct {
    int row, col, kind, is_string;
    void* ptr;
    char picture[48];
    char items[FR_MAX_ITEMS][64]; int nitems;
    int width, height;
    double vmin, vmax, vstep;
    char caption[64];
    char edit[512]; int cur;
    int idx;              /* list/combo/color: indice corrente */
    int open;             /* popup aperto */
    int phover;           /* voce popup sotto il mouse */
    unsigned int curcolor;   /* colorpicker: colore corrente 0xAARRGGBB */
    int prx, pry, prw, prh;  /* rect popup palette (impostato dal draw) */
} fr_field_t;

static fr_say_t g_says[FR_MAX_SAY]; static int g_nsay = 0;
static fr_field_t g_fields[FR_MAX_FIELD]; static int g_nfield = 0;

void gmb_form_clear(void) { g_nsay = 0; g_nfield = 0; }

void gmb_form_say(int row, int col, const char* text)
{
    if (g_nsay >= FR_MAX_SAY) return;
    fr_say_t* s = &g_says[g_nsay++];
    s->row = row; s->col = col;
    snprintf(s->text, sizeof(s->text), "%s", text ? text : "");
}

/* Paint delle etichette @SAY sulla pagina grafica (celle -> pixel con la stessa
 * metrica usata da gmb_tui_draw_px_all). Le etichette non hanno stato: si ridisegnano. */
void gmb_form_cell_px(int* out_cw, int* out_ch)
{
    extern int gmb_gfx_screen_on(void);
    extern int gmb_gfx_screen_width(void);
    extern int gmb_gfx_screen_height(void);
    extern gmb_clipper_state_t* gmb_clipper_peek_state(void);
    int cw = 8, ch = 16;
    if (gmb_gfx_screen_on()) {
        /* Cella = griglia CONSOLE proiettata sulla pagina LOGICA: (riga,col) dei
         * comandi Clipper sono celle di console, non pixel ne' font. */
        gmb_clipper_state_t* st = gmb_clipper_peek_state();
        extern float gmb_gfx_render_scale(void);
        float rsc = gmb_gfx_render_scale(); if (rsc <= 0.0f) rsc = 1.0f;
        int pw = (int)(gmb_gfx_screen_width() * rsc + 0.5f);
        int ph = (int)(gmb_gfx_screen_height() * rsc + 0.5f);
        if (st && st->buffer && pw > 0 && ph > 0) {
            int bw = gmb_screen_width(st->buffer), bh = gmb_screen_height(st->buffer);
            if (bw > 0) cw = pw / bw;
            if (bh > 0) ch = ph / bh;
        }
        if (cw < 4) cw = 8;
        if (ch < 8) ch = 16;
        ch += 14;   /* il campo e' piu' alto della riga: spazio tra le righe */
    }
    if (out_cw) *out_cw = cw;
    if (out_ch) *out_ch = ch;
}


void gmb_form_paint_says_px(void)
{
    extern int gmb_gfx_screen_on(void);
    extern void gmb_gfx_draw_text(const char* text, int x, int y, int align, int color);
    if (!gmb_gfx_screen_on()) return;
    int cw = 8, ch = 16;
    gmb_form_cell_px(&cw, &ch);          /* STESSA metrica dei campi */
    for (int i = 0; i < g_nsay; i++)
        gmb_gfx_draw_text(g_says[i].text, g_says[i].col * cw, g_says[i].row * ch + 8, 0, (int)0x303040);
}

static int fr_split(const char* items, char out[][64], int cap)
{
    int n = 0; const char* p = items ? items : "";
    while (*p && n < cap) { const char* s = p; while (*p && *p != '|') p++; int l = (int)(p - s); if (l > 63) l = 63; memcpy(out[n], s, (size_t)l); out[n][l] = 0; n++; if (*p == '|') p++; }
    return n;
}

int gmb_form_get(int row, int col, void* ptr, int is_string, const char* type,
                 const char* picture, const char* items, int width, int height,
                 double vmin, double vmax, double step, const char* caption)
{
    if (g_nfield >= FR_MAX_FIELD) return 0;
    fr_field_t* f = &g_fields[g_nfield];
    memset(f, 0, sizeof(*f));
    f->row = row; f->col = col; f->ptr = ptr; f->is_string = is_string;
    f->width = width > 0 ? width : 14; f->height = height > 0 ? height : 1;
    f->vmin = vmin; f->vmax = vmax; f->vstep = step > 0 ? step : 1;
    snprintf(f->picture, sizeof(f->picture), "%s", picture ? picture : "");
    snprintf(f->caption, sizeof(f->caption), "%s", caption ? caption : "");
    if (type && !strcasecmp(type, "checkbox")) f->kind = F_CHECK;
    else if (type && !strcasecmp(type, "spinner")) f->kind = F_SPIN;
    else if (type && !strcasecmp(type, "listbox")) { f->kind = F_LIST; f->nitems = fr_split(items, f->items, FR_MAX_ITEMS); }
    else if (type && !strcasecmp(type, "combobox")) { f->kind = F_COMBO; f->nitems = fr_split(items, f->items, FR_MAX_ITEMS); }
    else if (type && !strcasecmp(type, "colorpicker")) f->kind = F_COLOR;
    else f->kind = F_TEXT;

    if (f->kind == F_TEXT) {
        if (is_string) { const char* v = ((gmb_string_t*)ptr)->data; snprintf(f->edit, sizeof(f->edit), "%s", v ? v : ""); }
        else snprintf(f->edit, sizeof(f->edit), "%.2f", *(double*)ptr);
        f->cur = (int)strlen(f->edit);
        while (f->cur > 0 && f->edit[f->cur - 1] == ' ') f->edit[--f->cur] = 0;
    } else if (f->kind == F_LIST || f->kind == F_COMBO) {
        int sel = 0;
        if (is_string) { const char* v = ((gmb_string_t*)ptr)->data; for (int k = 0; k < f->nitems; k++) if (v && strcmp(v, f->items[k]) == 0) { sel = k; break; } }
        else sel = (int)(*(double*)ptr + 0.5);
        f->idx = (sel >= 0 && sel < f->nitems) ? sel : 0;
    } else if (f->kind == F_COLOR) {
        const char* v = is_string ? ((gmb_string_t*)ptr)->data : "";
        unsigned int cur = v ? (unsigned int)strtoul(v, NULL, 16) : 0;
        f->curcolor = 0xFF000000u | (cur & 0xFFFFFFu);
    }
    return ++g_nfield;
}
