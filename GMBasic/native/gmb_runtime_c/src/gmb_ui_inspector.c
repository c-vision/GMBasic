/* ============================================================================
 * gmb_ui_inspector.c — inspector della UI (vedi include/gmb_ui_inspector.h).
 *
 * Il pannello è SOLO lettura: nessun controllo viene creato, nessun evento
 * generato, nessun BASIC eseguito. La raccolta delle voci è fatta dal frame a
 * cui appartengono i controlli (TUI o FORM); qui c'è la loro forma comune, il
 * costo del frame, il testo e il disegno su celle o su pixel.
 * ============================================================================ */
#include "gmb_ui_inspector.h"
#include "gmb_a11y.h"        /* gmb_a11y_parse_bool: stessa semantica dei flag */
#include "gmb_runtime.h"     /* gmb_opt_get: registro opzioni condiviso */
#include "gmb_tui_widgets.h" /* GMB_CK_*, tema a celle, stile corrente */
#include "gmb_ui_gfx.h"      /* GMB_GFX_* per il percorso pixel */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

typedef struct {
    char name[40];
    char kind[20];
    int x, y, w, h;
    int flags;
    int cols;            /* flow: colonne della griglia (0 = n/d) */
} insp_item_t;

static insp_item_t g_items[GMB_INSPECTOR_MAX];
static int g_nitems = 0;
static int g_frame_ms = 0, g_frame_controls = 0, g_frames = 0;

/* ---- `ui.inspector`: registro opzioni, con cache sull'ultimo valore visto ---- */
static int  g_on = 0;
static char g_seen[16] = "\x01";

int gmb_inspector_enabled(void)
{
    char v[16];
    v[0] = '\0';
    (void)gmb_opt_get(GMB_INSPECTOR_OPT, v, (int)sizeof v);
    if (strcmp(v, g_seen) != 0) {
        snprintf(g_seen, sizeof g_seen, "%s", v);
        g_on = gmb_a11y_parse_bool(v);
    }
    return g_on;
}

void gmb_inspector_note_frame(int frame_ms, int controls)
{
    g_frame_ms = frame_ms < 0 ? 0 : frame_ms;
    g_frame_controls = controls < 0 ? 0 : controls;
    g_frames++;
}

void gmb_inspector_stats(int* frame_ms, int* controls, int* frames)
{
    if (frame_ms) *frame_ms = g_frame_ms;
    if (controls) *controls = g_frame_controls;
    if (frames)   *frames = g_frames;
}

void gmb_inspector_reset(void)
{
    g_nitems = 0;
}

int gmb_inspector_add(const char* name, const char* kind,
                      int x, int y, int w, int h, int flags, int cols)
{
    if (g_nitems >= GMB_INSPECTOR_MAX) return -1;
    insp_item_t* it = &g_items[g_nitems];
    memset(it, 0, sizeof(*it));
    snprintf(it->name, sizeof it->name, "%s", name && *name ? name : "(anon)");
    snprintf(it->kind, sizeof it->kind, "%s", kind && *kind ? kind : "?");
    it->x = x; it->y = y; it->w = w; it->h = h;
    it->flags = flags;
    it->cols = cols;
    return g_nitems++;
}

int gmb_inspector_count(void) { return g_nitems; }

const char* gmb_inspector_kind_name(int ck)
{
    switch (ck) {
        case GMB_CK_NONE:            return "NONE";
        case GMB_CK_CHECKBOX:        return "CHECK";
        case GMB_CK_TOGGLE:          return "TOGGLE";
        case GMB_CK_RADIO:           return "RADIO";
        case GMB_CK_SLIDER:          return "SLIDER";
        case GMB_CK_COMBOBOX:        return "COMBO";
        case GMB_CK_LISTBOX:         return "LISTBOX";
        case GMB_CK_SPINNER:         return "SPINNER";
        case GMB_CK_TAB:             return "TAB";
        case GMB_CK_GRID:            return "GRID";
        case GMB_CK_TREE:            return "TREE";
        case GMB_CK_COLORPICKER:     return "COLORPCK";
        case GMB_CK_DATEPICKER:      return "DATEPICK";
        case GMB_CK_STATUSBAR:       return "STATUSBAR";
        case GMB_CK_NUMERICUPDOWN:   return "NUMUP";
        case GMB_CK_STARRATING:      return "STAR";
        case GMB_CK_SCROLLBAR:       return "SCROLLBAR";
        case GMB_CK_SCROLLVIEW:      return "SCROLLVIEW";
        case GMB_CK_LABEL:           return "LABEL";
        case GMB_CK_DATALIST:        return "DATALIST";
        case GMB_CK_PROPERTYGRID:    return "PROPGRID";
        case GMB_CK_LOGVIEW:         return "LOGVIEW";
        case GMB_CK_HEXVIEW:         return "HEXVIEW";
        case GMB_CK_BIGTEXT:         return "BIGTEXT";
        case GMB_CK_CODEAREA:        return "CODEAREA";
        case GMB_CK_REORDERLISTBOX:  return "REORDERLB";
        case GMB_CK_ALERTBOX:        return "ALERTBOX";
        case GMB_CK_CONFIRMATIONBOX: return "CONFIRM";
        case GMB_CK_FILEEXPLORER:    return "FILEEXP";
        case GMB_CK_DASHBOARD:       return "DASHBOARD";
        case GMB_CK_TBROWSE:         return "TBROWSE";
        case GMB_CK_MENU:            return "MENU";
        case GMB_CK_FILEDIALOG:      return "FILEDLG";
        case GMB_CK_PRINTERDIALOG:   return "PRINTDLG";
        case GMB_CK_TEXTBOX:         return "TEXTBOX";
        case GMB_CK_CANVAS:          return "CANVAS";
        case GMB_CK_IMAGE:           return "IMAGE";
        case GMB_CK_IMAGEBUTTON:     return "IMAGEBTN";
        case GMB_CK_QR:              return "QR";
        case GMB_CK_RICHTEXT:        return "RICHTEXT";
        case GMB_CK_HALFBLOCK:       return "HALFBLOCK";
        case GMB_CK_TILING:          return "TILING";
        case GMB_CK_USERCONTROL:     return "USERCTRL";
        case GMB_CK_FKEYBAR:         return "FKEYBAR";
        case GMB_CK_CANDLESTICK:     return "CANDLE";
        case GMB_CK_CHART:           return "CHART";
        case GMB_CK_SPLITTER:        return "SPLITTER";
        case GMB_CK_CONTEXTMENU:     return "CTXMENU";
        case GMB_CK_BUTTON:          return "BUTTON";
        default:                     return "?";
    }
}

/* Bandierine compatte: v/e/f/L/Fl/G/a. */
static void item_flags(const insp_item_t* it, char* out, int cap)
{
    int n = 0;
    if (it->flags & GMB_INSPECT_VISIBLE)  { if (n < cap - 1) out[n++] = 'v'; }
    if (it->flags & GMB_INSPECT_ENABLED)  { if (n < cap - 1) out[n++] = 'e'; }
    if (it->flags & GMB_INSPECT_FOCUSED)  { if (n < cap - 1) out[n++] = 'f'; }
    if (it->flags & GMB_INSPECT_LAYOUT)   { if (n < cap - 1) out[n++] = 'L'; }
    if (it->flags & GMB_INSPECT_FLOW)     { if (n < cap - 1) out[n++] = 'F'; if (n < cap - 1) out[n++] = 'l'; }
    if (it->flags & GMB_INSPECT_GROUP)    { if (n < cap - 1) out[n++] = 'G'; }
    if (it->flags & GMB_INSPECT_ANIMATED) { if (n < cap - 1) out[n++] = 'a'; }
    out[n] = '\0';
}

int gmb_inspector_lines(char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int off = 0;
    int ms = g_frame_ms;
    if (g_frames > 0)
        off += snprintf(out + off, (size_t)(cap - off),
                        "INSPECTOR  ctrl=%d  frame=%d.%03dms  frames=%d\n",
                        g_nitems, ms / 1000, ms % 1000, g_frames);
    else
        off += snprintf(out + off, (size_t)(cap - off), "INSPECTOR  ctrl=%d\n", g_nitems);
    if (off >= cap) return cap;
    off += snprintf(out + off, (size_t)(cap - off),
                    "name         kind       x,y    wxh   flags\n");
    if (off >= cap) return cap;
    for (int i = 0; i < g_nitems && off < cap; i++) {
        const insp_item_t* it = &g_items[i];
        char fl[16];
        item_flags(it, fl, sizeof fl);
        if (it->flags & GMB_INSPECT_FLOW && it->cols > 0)
            off += snprintf(out + off, (size_t)(cap - off), "%-12.12s %-10.10s %4d,%-4d %3dx%-3d %s/%d\n",
                            it->name, it->kind, it->x, it->y, it->w, it->h, fl, it->cols);
        else
            off += snprintf(out + off, (size_t)(cap - off), "%-12.12s %-10.10s %4d,%-4d %3dx%-3d %s\n",
                            it->name, it->kind, it->x, it->y, it->w, it->h, fl);
    }
    if (off > cap) off = cap;
    return off;
}

int gmb_inspector_split(char lines[][GMB_INSPECTOR_LINE_MAX], int max)
{
    char text[GMB_INSPECTOR_MAXLINES * GMB_INSPECTOR_LINE_MAX];
    int n = gmb_inspector_lines(text, (int)sizeof text);
    int count = 0, off = 0;
    while (off < n && count < max) {
        int i = 0;
        while (off < n && text[off] != '\n' && i < GMB_INSPECTOR_LINE_MAX - 1)
            lines[count][i++] = text[off++];
        lines[count][i] = '\0';
        if (off < n && text[off] == '\n') off++;
        count++;
    }
    return count;
}

void gmb_inspector_draw_cells(gmb_screen_buffer_t* b)
{
    if (!b || !gmb_inspector_enabled()) return;
    char lines[GMB_INSPECTOR_MAXLINES][GMB_INSPECTOR_LINE_MAX];
    int nl = gmb_inspector_split(lines, GMB_INSPECTOR_MAXLINES);
    if (nl <= 0) return;

    int sw = gmb_screen_width(b), sh = gmb_screen_height(b);
    if (sw < 20 || sh < 6) return;
    int W = 58;
    if (W > sw - 2) W = sw - 2;
    int H = nl + 2;                     /* bordo sopra/sotto */
    if (H > sh - 2) H = sh - 2;
    if (H < 4) return;
    int X = sw - W - 1, Y = sh - H - 1;
    if (X < 0) X = 0;
    if (Y < 0) Y = 0;

    gmb_theme_t th = gmb_ui_theme();
    gmb_cell_color_t fg = th.text, bg = th.window_bg;
    gmb_cell_color_t bd = (th.border.r || th.border.g || th.border.b) ? th.border : fg;
    gmb_cell_color_t tf = (th.title_focus.r || th.title_focus.g || th.title_focus.b) ? th.title_focus : fg;

    for (int y = Y + 1; y < Y + H - 1; y++)
        for (int x = X + 1; x < X + W - 1; x++)
            gmb_screen_setchar(b, x, y, ' ', fg, bg);
    gmb_screen_setchar(b, X, Y, 0x250C, bd, bg);
    gmb_screen_setchar(b, X + W - 1, Y, 0x2510, bd, bg);
    gmb_screen_setchar(b, X, Y + H - 1, 0x2514, bd, bg);
    gmb_screen_setchar(b, X + W - 1, Y + H - 1, 0x2518, bd, bg);
    for (int x = X + 1; x < X + W - 1; x++) {
        gmb_screen_setchar(b, x, Y, 0x2500, bd, bg);
        gmb_screen_setchar(b, x, Y + H - 1, 0x2500, bd, bg);
    }
    for (int y = Y + 1; y < Y + H - 1; y++) {
        gmb_screen_setchar(b, X, y, 0x2502, bd, bg);
        gmb_screen_setchar(b, X + W - 1, y, 0x2502, bd, bg);
    }
    gmb_screen_setstring_n(b, X + 1, Y, " INSPECTOR ", W - 2, tf, bg);
    for (int i = 0; i < nl && Y + 1 + i < Y + H - 1; i++)
        gmb_screen_setstring_n(b, X + 1, Y + 1 + i, lines[i], W - 2, fg, bg);
}

void gmb_inspector_draw_pixels(void)
{
    if (!gmb_inspector_enabled()) return;
    if (!GMB_GFX_SCREEN_ON()) return;

    char lines[GMB_INSPECTOR_MAXLINES][GMB_INSPECTOR_LINE_MAX];
    int nl = gmb_inspector_split(lines, GMB_INSPECTOR_MAXLINES);
    if (nl <= 0) return;

    gmb_widget_style_t st = gmb_widget_style_current();
    int sw = GMB_GFX_SCREEN_W(), sh = GMB_GFX_SCREEN_H();
    int lh = GMB_GFX_TEXT_H();
    if (lh < 8) lh = 14;
    lh += 2;
    int W = 460;
    if (W > sw - 16) W = sw - 16;
    int H = nl * lh + 10;
    if (H > sh - 16) H = sh - 16;
    if (W < 40 || H < lh * 2) return;
    int X = sw - W - 8, Y = sh - H - 8;
    if (X < 0) X = 0;
    if (Y < 0) Y = 0;

    unsigned int bg = st.body ? st.body : st.fill;
    unsigned int fg = st.text ? st.text : 0xFFFFFF;
    unsigned int bd = st.accent ? st.accent : fg;
    GMB_GFX_FILL_RRECT(X, Y, W, H, 4, bg);
    GMB_GFX_STROKE_RRECT(X, Y, W, H, 4, bd);
    for (int i = 0; i < nl && Y + 5 + i * lh < Y + H - 2; i++)
        GMB_GFX_DRAW_TEXT(lines[i], X + 6, Y + 5 + i * lh, 0, fg);
}
