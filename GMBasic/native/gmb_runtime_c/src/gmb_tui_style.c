/* ============================================================================
 * gmb_tui_style.c — gmb_tui_style. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ControlStyle ===================== */

static gmb_cell_color_t style_default_fg(void) { return crgb(200, 208, 220); }

void gmb_control_style_init(gmb_control_style_t* s)
{
    memset(s, 0, sizeof(*s));
    memset(s, 0, sizeof(*s));
    s->fill_char = ' ';
}

#include <ctype.h>
void gmb_draw_string_keys(gmb_screen_buffer_t* buf, int x, int y, const char* s, int maxw,
                          gmb_cell_color_t fg, gmb_cell_color_t bg, gmb_cell_color_t hk)
{
    if (!s || !buf) return;
    int cx = x, i = 0, inHk = 0;
    while (s[i] && cx < x + maxw) {
        if (s[i] == '~') { inHk = !inHk; i++; continue; }
        int len = 0;
        if (!inHk) {
            if (s[i]=='F' && s[i+1]>='0' && s[i+1]<='9') { int j=i+1; while (s[j]>='0'&&s[j]<='9') j++; len=j-i; }
            else if (!strncmp(s+i,"Alt-",4))   { int j=i+4; while (s[j] && (isalnum((unsigned char)s[j])||s[j]=='-')) j++; len=j-i; }
            else if (!strncmp(s+i,"Ctrl-",5))  { int j=i+5; while (s[j] && (isalnum((unsigned char)s[j])||s[j]=='-')) j++; len=j-i; }
            else if (!strncmp(s+i,"Shift-",6)) { int j=i+6; while (s[j] && (isalnum((unsigned char)s[j])||s[j]=='-')) j++; len=j-i; }
        }
        if (inHk) { gmb_screen_setchar(buf, cx++, y, (uint32_t)(unsigned char)s[i], hk, bg); i++; }
        else if (len > 0) { for (int k=0;k<len && cx<x+maxw;k++) gmb_screen_setchar(buf, cx++, y, (uint32_t)(unsigned char)s[i+k], hk, bg); i += len; }
        else { gmb_screen_setchar(buf, cx++, y, (uint32_t)(unsigned char)s[i], fg, bg); i++; }
    }
}

gmb_control_style_t gmb_control_style_default(void)
{
    gmb_control_style_t s;
    memset(&s, 0, sizeof(s));
    s.has_fg = true; s.fg = GMB_CELLCOLOR_BLACK;
    s.has_bg = true; s.bg = crgb(170, 170, 170);
    s.has_fg_focus = true; s.fg_focus = GMB_CELLCOLOR_BLACK;
    s.has_bg_focus = true; s.bg_focus = GMB_CELLCOLOR_CYAN;
    s.has_fg_error = true; s.fg_error = GMB_CELLCOLOR_WHITE;
    s.has_bg_error = true; s.bg_error = GMB_CELLCOLOR_RED;
    s.has_sel_fg = true; s.sel_fg = GMB_CELLCOLOR_BLACK;
    s.has_sel_bg = true; s.sel_bg = GMB_CELLCOLOR_CYAN;
    s.fill_char = ' ';
    return s;
}

gmb_control_style_t gmb_control_style_from_theme(const gmb_theme_t* t)
{
    gmb_control_style_t s;
    memset(&s, 0, sizeof(s));
    /* I controlli TV vivono nei DIALOG (grigi): base dai colori dialog quando definiti,
     * altrimenti finestra. NB: il nero (0,0,0) è un colore VALIDO per il dialog — la
     * sentinella "deriva" è il BG non impostato, non il FG. */
    int has_dlg = (t->dlg_bg.r || t->dlg_bg.g || t->dlg_bg.b) ? 1 : 0;
    gmb_cell_color_t dfg = has_dlg ? t->dlg_fg : t->text;
    gmb_cell_color_t dbg = has_dlg ? t->dlg_bg : t->window_bg;
    s.has_fg = true; s.fg = dfg;
    s.has_bg = true; s.bg = dbg;
    s.has_fg_focus = true; s.fg_focus = t->title_fg;
    s.has_bg_focus = true; s.bg_focus = t->active_bg;
    s.has_fg_error = true; s.fg_error = GMB_CELLCOLOR_WHITE;
    s.has_bg_error = true; s.bg_error = GMB_CELLCOLOR_RED;
    s.has_sel_fg = true; s.sel_fg = t->window_bg;
    s.has_sel_bg = true; s.sel_bg = t->active_bg;
    /* Se il tema ha window_bg == active_bg (es. Turbo Vision: blu su blu), il testo
     * "selezionato" sarebbe INVISIBILE (tab attiva, riga cursore grid/propertygrid).
     * In quel caso si usa il colore titolo (bianco su blu in TV). */
    if (s.sel_fg.r == s.sel_bg.r && s.sel_fg.g == s.sel_bg.g && s.sel_fg.b == s.sel_bg.b)
        s.sel_fg = t->title_fg;
    s.fill_char = ' ';
    return s;
}

gmb_control_style_t gmb_control_style_clone(const gmb_control_style_t* s) { return *s; }

static gmb_theme_t* g_active_theme = NULL;

void gmb_control_style_set_theme(const gmb_theme_t* t)
{
    static gmb_theme_t store;
    if (t) { store = *t; g_active_theme = &store; }
    else g_active_theme = NULL;
}

gmb_control_style_t gmb_control_style_global_default(void)
{
    if (g_active_theme)
        return gmb_control_style_from_theme(g_active_theme);
    return gmb_control_style_default();
}

void gmb_control_style_with_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    s->has_fg = true; s->fg = fg;
    s->has_bg = true; s->bg = bg;
}
void gmb_control_style_with_focus_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    s->has_fg_focus = true; s->fg_focus = fg;
    s->has_bg_focus = true; s->bg_focus = bg;
}
void gmb_control_style_with_error_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    s->has_fg_error = true; s->fg_error = fg;
    s->has_bg_error = true; s->bg_error = bg;
}
void gmb_control_style_with_selected_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    s->has_sel_fg = true; s->sel_fg = fg;
    s->has_sel_bg = true; s->sel_bg = bg;
}
void gmb_control_style_with_fill(gmb_control_style_t* s, char fill) { s->fill_char = fill; }

void gmb_control_style_effective(const gmb_control_style_t* s, bool focused, bool error, bool selected,
                                 gmb_cell_color_t* fg, gmb_cell_color_t* bg)
{
    /* Colori di base dal TEMA attivo (non piu' BLU hardcoded): finestra/controlli seguono
     * il tema TurboVision/SDL ecc. Un controllo con stile esplicito ha la precedenza. */
    gmb_theme_t th = gmb_ui_theme();
    gmb_cell_color_t base_fg = s->has_fg ? s->fg : th.text;
    gmb_cell_color_t base_bg = s->has_bg ? s->bg : th.window_bg;
    if (error)
    {
        *fg = s->has_fg_error ? s->fg_error : base_fg;
        *bg = s->has_bg_error ? s->bg_error : base_bg;
    }
    else if (selected)
    {
        *fg = s->has_sel_fg ? s->sel_fg : base_fg;
        *bg = s->has_sel_bg ? s->sel_bg : base_bg;
    }
    else if (focused)
    {
        *fg = s->has_fg_focus ? s->fg_focus : base_fg;
        *bg = s->has_bg_focus ? s->bg_focus : base_bg;
    }
    else
    {
        *fg = base_fg;
        *bg = base_bg;
    }
}

/* ===================== Palette CONTROLLI Turbo Vision =====================
 * Valori RISOLTI dal sorgente ufficiale (magiblot/tvision) con la catena di palette reale
 * (cpGrayDialog → cpAppColor), non "a occhio":
 *   TDialog     indice 1 → 0x70  nero su grigio chiaro
 *   TInputLine  0x13     → 0x1F  bianco su blu
 *   TButton     0x0A     → 0x20  nero su VERDE (focus 0x2B = ciano chiaro su verde)
 *   TCluster    0x10     → 0x30  nero su CIANO (selezionato 0x2F = bianco su verde)
 *   TListBox    0x10     → 0x30  nero su ciano (selezionato 0x2F = bianco su verde)
 *   TScrollBar  0x13     → 0x13  ciano su blu
 * Si applica al DISEGNO (non alla creazione) così il cambio tema a runtime è rispettato.
 * Per gli altri temi non fa nulla: restano i colori derivati dal tema. */
static int tv_is_turbovision(void)
{
    const char* n = gmb_ui_theme_name();
    return n && strcmp(n, "turbovision") == 0;
}

void gmb_tui_apply_tv_style(gmb_control_t* c)
{
    if (!c || !tv_is_turbovision()) return;
    gmb_cell_color_t black = crgb(0, 0, 0);
    gmb_cell_color_t white = crgb(255, 255, 255);
    gmb_cell_color_t blue  = crgb(0, 0, 170);
    gmb_cell_color_t green = crgb(0, 170, 0);
    gmb_cell_color_t cyan  = crgb(0, 170, 170);
    gmb_cell_color_t lcyan = crgb(85, 255, 255);
    switch (c->ctrl_kind)
    {
    case GMB_CK_BUTTON:
        gmb_control_style_with_colors(&c->style, black, green);
        gmb_control_style_with_focus_colors(&c->style, lcyan, green);
        gmb_control_style_with_selected_colors(&c->style, white, green);
        break;
    case GMB_CK_TEXTBOX:
        gmb_control_style_with_colors(&c->style, white, blue);
        gmb_control_style_with_focus_colors(&c->style, white, blue);
        gmb_control_style_with_selected_colors(&c->style, black, cyan);
        break;
    case GMB_CK_CHECKBOX:
    case GMB_CK_TOGGLE:
    case GMB_CK_RADIO:
        gmb_control_style_with_colors(&c->style, black, cyan);
        gmb_control_style_with_focus_colors(&c->style, white, cyan);
        gmb_control_style_with_selected_colors(&c->style, white, green);
        break;
    case GMB_CK_LISTBOX:
    case GMB_CK_COMBOBOX:
    case GMB_CK_DATALIST:
    case GMB_CK_REORDERLISTBOX:
        gmb_control_style_with_colors(&c->style, black, cyan);
        gmb_control_style_with_focus_colors(&c->style, black, cyan);
        gmb_control_style_with_selected_colors(&c->style, white, green);
        break;
    case GMB_CK_SCROLLBAR:
    case GMB_CK_SLIDER:
    case GMB_CK_SPINNER:
    case GMB_CK_NUMERICUPDOWN:
        gmb_control_style_with_colors(&c->style, cyan, blue);
        gmb_control_style_with_focus_colors(&c->style, cyan, blue);
        break;
    default:
        break;
    }
}
