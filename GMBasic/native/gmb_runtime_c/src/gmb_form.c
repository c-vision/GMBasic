#include "gmb_runtime.h"
/* gmb_form.c — vedi gmb_form.h per il contratto GPM. */
#include "gmb_form.h"
#include "gmb_graphics.h"
#include "gmb_gfx_input.h"
#include "gmb_ui_gfx.h"
#include "gmb_mouse_input.h"
#include "gmb_c99gpm.h"
#include <stdint.h>
#include <time.h>
static long long form_now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (long long)ts.tv_sec*1000 + ts.tv_nsec/1000000; }
#include "gmb_tui_widgets.h"   /* gmb_widget_style_t + tema corrente */
#include "gmb_tui_tbrowse.h"   /* gmb_browse_t (props CONTROL SET/GET TBROWSE) */
#ifdef GMB_GFX_SDL_PRESENT
#include "gmb_gfx_present.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

/* ── UI OPZIONALE: riferimenti DEBOLI ──────────────────────────────────────────────────────────
 * FORM/READ usano i controlli TUI e il present grafico (SDL/Vulkan). In un eseguibile
 * TESTUALE quelle librerie non devono entrare nel link (SDL2/SDL2_ttf/MoltenVK = ~8 MB):
 * con riferimenti deboli gli oggetti UI non vengono trascinati dentro. Se l'UI non è linkata
 * le funzioni valgono NULL e si avvisa ESPLICITAMENTE (mai un no-op silenzioso). */
extern void gmb_gfx_drop_shadow_rounded(int,int,int,int,int,unsigned int,int,int);
#ifdef GMB_WITH_UI
#define GMB_UI_PRESENT(fn) 1
#else
#define GMB_UI_PRESENT(fn) 0
#endif
static void gmb_ui_missing_warn(void)
{
    static int warned;
    if (!warned) { warned = 1; fprintf(stderr, "[form] UI non inclusa in questa build: controlli TUI/ombra non disponibili.\n"); }
}


/* TUI flush (dichiarazione esplicita: evita di includere l'intero gmb_tui.h). */
extern void gmb_tui_screen_flush(void);

#define GMB_FORM_MAX_BUTTONS 32

typedef struct {
    gmb_control_t base;         /* base Control UNICA (geometry/visible/enabled/tab/eventi/handle) */
    char name[32], tag[32], tooltip[64];   /* proprietà base (VB) del Control */
    char label[64];
    gmb_form_cb cb;
    int hover, pressed;
    gmb_widget_style_t style;   /* default dal tema corrente al momento della creazione */
} gmb_form_btn_t;

static gmb_form_btn_t g_btns[GMB_FORM_MAX_BUTTONS];
static int g_nbtn = 0;
static int g_closed = 0;   /* headless / richiesta di chiusura */
static int g_dirty = 1;    /* ridisegna solo quando serve (niente repaint continuo) */
static int g_form_exclusive = 0;   /* il render del form non tocca la pagina (READ attivo) */
static int g_last_added = -1;      /* ultimo widget/bottone aggiunto (per STYLE) */
static int g_focus_wid = -1;       /* widget col focus (edit inline) */
static int g_read_mode = 0;        /* READ Clipper: Enter/ESC/Tab gestiti dal loop READ */
static int g_read_pending_key = 0;
void gmb_form_set_read_mode(int on) { g_read_mode = on; g_read_pending_key = 0; }
int  gmb_form_take_read_key(void) { int k = g_read_pending_key; g_read_pending_key = 0; return k; }
static int g_focus_btn = -1;       /* Control BUTTON con focus (SetFocus) */
static int g_click_wid = -1; static long g_click_ms = 0; static int g_down_wid = -1; static int g_dragging = 0;
static int g_firing_control = -1;  /* controllo il cui evento sta scattando (per il thunk managed) */
static int g_firing_event = 0;    /* evento in corso */
static int g_last_mouse_event = 0; /* ultimo evento mouse globale (polling MOUSECLICKED/...) */
static int g_last_key_event = 0;   /* ultimo evento tastiera globale (polling KEYPRESSED) */

void gmb_form_set_exclusive(int on) { g_form_exclusive = on; }

/* ---- Widget pixel (checkbox/radio/track/dropdown/progress) ---- */
#define GMB_FORM_MAX_WIDGETS 32
enum { W_CHECK = 1, W_RADIO, W_TRACK, W_DROPDOWN, W_PROGRESS,
       W_TOGGLE, W_SPINNER, W_STATUSBAR, W_TABS, W_LISTBOX, W_COLORPICKER, W_DATE, W_GRID, W_FILE, W_TREE, W_MENUBAR, W_TEXT,
       W_CANVAS, W_IMAGE, W_IMAGEBUTTON, W_QR, W_RICHTEXT, W_HALFBLOCK, W_TILING, W_PANEL };
typedef struct {
    gmb_control_t base;         /* base Control UNICA (geometry/eventi/handle) */
    int kind;
    char name[32];                 /* Name (VB) */
    char tag[32], tooltip[64];
    char label[64];               /* Caption/Text */
    char items[8][40]; int nitems;
    unsigned int colors[16]; int ncolors;   /* colorpicker */
    double* var; double vmin, vmax, vstep; double ival; char itext[256];   /* valore interno (accesso per nome quando var==NULL, gambe gestite) */
    void* strptr; int dy, dm, dday, sy, sm, sd; char dfmt[16]; char dmask[16]; int dcur;   /* datepicker */
    char ghead[8][40]; int gnc;   /* grid: headers */
    char gdata[1536]; int gnr, gsort_col, gsort_dir; int gorder[64];  /* grid: dati + ordine */
    char big[64][56]; int nbig; int bsel, bfirst;   /* file list */
    int tdepth[64];   /* tree: profondita' */ int tcol[64];   /* tree: nodi collassati */
    int hover, press, open; int tcur;   /* textbox: cursore (indice char) */
    int show_percent; char bar_style[16]; int align; int is_valid;   /* progress: show_percent/bar_style; align generico; textbox: valid */
    int first;                 /* listbox: prima riga visibile */
    gmb_widget_style_t style;
    int dbl_ms;                     /* ultimo click per dblclick */
} gmb_pix_widget_t;
static int grid_cell(const gmb_pix_widget_t* wd, int r, int c, char* out, int cap);
static gmb_pix_widget_t g_wid[GMB_FORM_MAX_WIDGETS];
static int g_nwid = 0;
static int g_last_handle = -1;   /* ID handle del controllo appena creato (widget=i, button=1000+i) */
static int g_last_is_tui = 0;    /* 1 = l'ultimo controllo creato vive nel registry TUI (handle 0..63) */

static void set_focus_wid(int i);
static const char* form_stristr(const char* h, const char* n)
{
    size_t nl = strlen(n);
    if (!nl) return h;
    for (; *h; h++) if (strncasecmp(h, n, nl) == 0) return h;
    return NULL;
}
static void apply_base_props(gmb_pix_widget_t* wd, const char* spec, int idx);

static void fire_ev(int widx, int ev);
/* ---- valore interno + coda eventi (pull) per le gambe gestite ---- */
static double wd_num(const gmb_pix_widget_t* wd) { return wd->var ? *(wd->var) : wd->ival; }
static const char* wd_str(const gmb_pix_widget_t* wd) { if (wd->strptr && ((gmb_string_t*)wd->strptr)->data) return ((gmb_string_t*)wd->strptr)->data; return wd->itext; }
static void wd_set_num(gmb_pix_widget_t* wd, double v) { wd->ival = v; if (wd->var) *(wd->var) = v; }
static void wd_add_num(gmb_pix_widget_t* wd, double d) { wd->ival += d; if (wd->var) *(wd->var) = wd->ival; }
static void wd_set_str(gmb_pix_widget_t* wd, const char* s) { snprintf(wd->itext, sizeof(wd->itext), "%s", s ? s : ""); if (wd->strptr) *(gmb_string_t*)wd->strptr = gmb_string_alloc(wd->itext); }
#define GMB_FORM_EVQ 512
static int g_evq_ev[GMB_FORM_EVQ], g_evq_wid[GMB_FORM_EVQ];
static int g_evq_head = 0, g_evq_tail = 0;
int gmb_form_poll_event(int* ev, int* widx) {
    if (g_evq_head == g_evq_tail) return 0;
    if (ev) *ev = g_evq_ev[g_evq_head];
    if (widx) *widx = g_evq_wid[g_evq_head];
    g_evq_head = (g_evq_head + 1) % GMB_FORM_EVQ;
    return 1;
}

/* --- tree: percorso "a/b", ESEMPIO di nodo = item (intero path) ----------------- */
static int tree_depth(const char* p){ int d=0; for(const char* s=p; s&&*s; s++) if(*s=='/') d++; return d; }
static const char* tree_name(const char* p){ const char* s=strrchr(p,'/'); return s? s+1 : p; }
static int tree_pfx(const char* a, const char* b){ size_t la=strlen(a); return strncmp(a,b,la)==0 && (b[la]==0 || b[la]=='/'); }
static int tree_has_children(const gmb_pix_widget_t* wd, int j){ for(int k=0;k<wd->nitems;k++){ if(k==j) continue; if(strlen(wd->items[k])>strlen(wd->items[j]) && tree_pfx(wd->items[j], wd->items[k])) return 1; } return 0; }
static int tree_hidden(const gmb_pix_widget_t* wd, int j){ for(int k=0;k<wd->nitems;k++){ if(k==j) continue; if(strlen(wd->items[k])<strlen(wd->items[j]) && wd->tcol[k] && tree_pfx(wd->items[k], wd->items[j])) return 1; } return 0; }
static int tree_row_at(gmb_pix_widget_t* wd, int ly, int rh){ if(rh<=0) return -1; int vr=ly/rh, seen=0; for(int j=0;j<wd->nitems;j++){ if(tree_hidden(wd,j)) continue; if(seen==vr) return j; seen++; } return -1; }

static void date_format(const char* fmt, int y, int m, int d, char* out, int cap)
{
    char f[16]; snprintf(f, sizeof(f), "%s", (fmt && *fmt) ? fmt : "dd/mm/yyyy");
    int j = 0;
    for (int i = 0; f[i] && j < cap - 1; ) {
        if (!strncmp(f + i, "yyyy", 4)) { j += snprintf(out + j, cap - j, "%04d", y); i += 4; }
        else if (!strncmp(f + i, "dd", 2)) { j += snprintf(out + j, cap - j, "%02d", d); i += 2; }
        else if (!strncmp(f + i, "mm", 2)) { j += snprintf(out + j, cap - j, "%02d", m); i += 2; }
        else out[j++] = f[i++];
    }
    out[j] = 0;
}
static void date_mask(const char* fmt, char* out, int cap)
{
    char f[16]; snprintf(f, sizeof(f), "%s", (fmt && *fmt) ? fmt : "dd/mm/yyyy");
    int j = 0;
    for (int i = 0; f[i] && j < cap - 1; i++) {
        char ch = f[i];
        if (ch == 'd' || ch == 'm' || ch == 'y') out[j++] = '_';
        else out[j++] = ch;
    }
    out[j] = 0;
}
static void date_mask_fill(gmb_pix_widget_t* wd)
{
    date_mask(wd->dfmt, wd->dmask, sizeof(wd->dmask));
    char fv[32]; date_format(wd->dfmt, wd->sy, wd->sm, wd->sd, fv, sizeof(fv));
    for (int i = 0; wd->dmask[i] && fv[i]; i++) if (fv[i] >= '0' && fv[i] <= '9') wd->dmask[i] = fv[i];
    wd->dcur = 0;
}
static int date_dim(int y, int m);
static int date_mask_valid(const gmb_pix_widget_t* wd)
{
    int dd = 0, mm = 0, yy = 0;
    const char* f = wd->dfmt;
    for (int i = 0; f[i]; ) {
        char ch = f[i];
        if (ch == 'd' || ch == 'm' || ch == 'y') {
            int j = i; while (f[j] == ch) j++;
            if (wd->dmask[i] == '_') return 0;
            int num = atoi(&wd->dmask[i]);
            if (ch == 'd') dd = num; else if (ch == 'm') mm = num; else yy = num;
            i = j;
        } else i++;
    }
    if (mm < 1 || mm > 12 || yy < 1) return 0;
    if (dd < 1 || dd > date_dim(yy, mm)) return 0;
    return 1;
}
static int date_mask_complete(const char* m) { for (const char* p = m; *p; p++) if (*p == '_') return 0; return 1; }
static void date_from_mask(gmb_pix_widget_t* wd)
{
    int dd = 0, mm = 0, yy = 0;
    const char* f = wd->dfmt;
    for (int i = 0; f[i]; ) {
        char ch = f[i];
        if (ch == 'd' || ch == 'm' || ch == 'y') {
            int j = i; while (f[j] == ch) j++;
            int num = atoi(&wd->dmask[i]);   /* dmask[i..] è cifre (stessa posizione) */
            if (ch == 'd') dd = num; else if (ch == 'm') mm = num; else yy = num;
            i = j;
        } else i++;
    }
    if (dd < 1 || dd > 31 || mm < 1 || mm > 12) return;
    wd->sy = yy; wd->sm = mm; wd->sd = dd; wd->dy = yy; wd->dm = mm;
    char b[16]; snprintf(b, sizeof(b), "%04d-%02d-%02d", yy, mm, dd);
    wd_set_str(wd, b);
}
static int date_dim(int y, int m) { static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31}; if(m==2 && ((y%4==0&&y%100!=0)||y%400==0)) return 29; return d[m-1]; }
static int date_first_wday(int y, int m) { struct tm t; memset(&t,0,sizeof t); t.tm_year=y-1900; t.tm_mon=m-1; t.tm_mday=1; t.tm_hour=12; mktime(&t); return (t.tm_wday+6)%7; }

static int form_split_items(const char* items, char out[][40], int cap)
{
    int n = 0; const char* p = items ? items : "";
    while (*p && n < cap) {
        const char* s = p; while (*p && *p != '|') p++;
        int l = (int)(p - s); if (l > 39) l = 39;
        memcpy(out[n], s, (size_t)l); out[n][l] = 0; n++;
        if (*p == '|') p++;
    }
    return n;
}

void gmb_form_reset(void) { g_nbtn = 0; g_nwid = 0; g_closed = 0; g_dirty = 1; g_last_mouse_event = 0; g_last_key_event = 0; g_click_wid = -1; g_click_ms = 0; g_down_wid = -1; g_dragging = 0; }

int gmb_form_add_button(int x, int y, int w, int h, const char* label, gmb_form_cb cb)
{
    /* Dispatch polimorfico: in TEXT il bottone vive nel registry TUI (frame-driven).
     * Vale anche per i chiamanti C# (P/Invoke su questa funzione). */
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_button(int, int, int, const char*);
        extern void gmb_tui_set_event_id(int, int, gmb_tui_cb);
        gmb_control_t* c = gmb_tui_add_button(x, y, w, label);
        if (!c) return -1;
        if (cb) gmb_tui_set_event_id(c->handle, GMB_EV_CLICK, (gmb_tui_cb)cb);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    if (g_nbtn >= GMB_FORM_MAX_BUTTONS) return -1;
    gmb_form_btn_t* b = &g_btns[g_nbtn];
    b->base.x = x; b->base.y = y; b->base.width = w; b->base.height = h;
    b->cb = cb; b->hover = 0; b->pressed = 0; b->base.enabled = 1; b->base.visible = 1; b->base.tab_stop = 1;
    b->name[0] = b->tag[0] = b->tooltip[0] = 0;
    b->style = gmb_widget_style_current();
    snprintf(b->label, sizeof(b->label), "%s", label ? label : "");
    g_dirty = 1;
    g_last_handle = 1000 + g_nbtn;   /* handle bottone (spazio distinto dai widget) */
    g_last_is_tui = 0;               /* l'ultimo controllo creato è un FORM button */
    b->base.handle = 1000 + g_nbtn;  /* base Control: handle univoco */
    return g_nbtn++;
}

/* Factory polimorfica BUTTON: TEXT -> registry TUI (non bloccante, frame-driven),
 * GRAPHICS -> registry form. Stesso verbo, stesso handle unificato. */
int gmb_control_add_button(int x, int y, int w, int h, const char* label, gmb_form_cb cb)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_button(int, int, int, const char*);
        extern void gmb_tui_set_event_id(int, int, gmb_tui_cb);
        gmb_control_t* c = gmb_tui_add_button(x, y, w, label);
        if (!c) return -1;
        if (cb) gmb_tui_set_event_id(c->handle, GMB_EV_CLICK, (gmb_tui_cb)cb);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_button(x, y, w, h, label, cb);
}

/* Factory polimorfica CHECKBOX: TEXT -> registry TUI (+ bind INTO), GRAPHICS -> form. */
int gmb_control_add_checkbox(int x, int y, int w, const char* label, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_checkbox(int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_checkbox(x, y, w, label);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_checkbox(x, y, label, var);
}

/* Factory polimorfica TOGGLE: TEXT -> registry TUI (+ bind INTO), GRAPHICS -> form. */
int gmb_control_add_toggle(int x, int y, int w, const char* label, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_toggle(int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_toggle(x, y, w, label);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_toggle(x, y, label, var);
}


/* ---- Factory polimorfiche (TEXT -> registry TUI + bind INTO; GRAPHICS -> form) ---- */
extern gmb_control_t* gmb_tui_add_radiogroup(int, int, int, int, const char*);
extern gmb_control_t* gmb_tui_add_listbox(int, int, int, int, const char*);
extern gmb_control_t* gmb_tui_add_combobox(int, int, int, const char*);
extern gmb_control_t* gmb_tui_add_numericupdown(int, int, int, int, int, int);
extern gmb_control_t* gmb_tui_add_starrating(int, int, int, int, int);
extern gmb_control_t* gmb_tui_add_scrollbar(int, int, int, int, int, int);
extern void gmb_tui_bind_value(int, double*);

int gmb_control_add_radio(int x, int y, int w, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        gmb_control_t* c = gmb_tui_add_radiogroup(x, y, w, 1, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_radio(x, y, items, var);
}

int gmb_control_add_listbox(int x, int y, int w, int h, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        gmb_control_t* c = gmb_tui_add_listbox(x, y, w, h, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_listbox(x, y, w, h, items, var);
}

int gmb_control_add_combobox(int x, int y, int w, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        gmb_control_t* c = gmb_tui_add_combobox(x, y, w, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_dropdown(x, y, w, items, var);
}

int gmb_control_add_numericupdown(int x, int y, int w, double mn, double mx, double step, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        gmb_control_t* c = gmb_tui_add_numericupdown(x, y, w, (int)mn, (int)mx, (int)(var ? *var : mn));
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_spinner(x, y, w, mn, mx, step, var);
}

int gmb_control_add_starrating(int x, int y, int w, int mx, int val, double* var)
{
    gmb_control_t* c = gmb_tui_add_starrating(x, y, w, mx, val);
    if (!c) return -1;
    if (var) gmb_tui_bind_value(c->handle, var);
    gmb_form_note_handle(c->handle);
    return c->handle;
}

int gmb_control_add_scrollbar(int x, int y, int len, int mn, int mx, int val, double* var)
{
    gmb_control_t* c = gmb_tui_add_scrollbar(x, y, len, mn, mx, val);
    if (!c) return -1;
    if (var) gmb_tui_bind_value(c->handle, var);
    gmb_form_note_handle(c->handle);
    return c->handle;
}


int gmb_control_add_grid(int x, int y, int w, int h, const char* headers, const char* data, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_grid(int, int, int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        int ncols = 1; for (const char* q = headers; q && *q; q++) if (*q == (char)124) ncols++;
        gmb_control_t* c = gmb_tui_add_grid(x, y, w, h, ncols, data);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_grid(x, y, w, h, headers, data, var);
}

int gmb_control_add_tree(int x, int y, int w, int h, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_treeview(int, int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_treeview(x, y, w, h, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_tree(x, y, w, h, items, var);
}

int gmb_control_add_tab(int x, int y, int w, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_tab(int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_tab(x, y, w, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return gmb_form_add_tabs(x, y, items, var);
}

int gmb_form_closed(void)
{
#ifdef GMB_GFX_SDL_PRESENT
    if (GMB_GFX_SHOULD_QUIT()) return 1;
#endif
    return g_closed;
}

void gmb_form_request_close(void)
{
    /* Solo flag interno: chiude il loop (gmb_form_closed()). NON si chiama SDL da
     * qui (il worker non tocca SDL, regola GPM); la finestra si chiude allo shutdown. */
    g_closed = 1;
}

int gmb_form_button_count(void) { return g_nbtn; }

/* Colore per le API che usano resolve_color (line/draw_text): flag RGB, senza segno.
 * (Passare 0xFFRRGGBB diretto = int NEGATIVO -> resolve_color lo tratta come "fg".) */
#define GFX_RGB(c) ((int)(GMB_GFX_RGB_FLAG | ((unsigned)(c) & 0xFFFFFFu)))

/* Luminanza/contiguita' colore per scegliere un riempimento che si stacchi dalla traccia. */
static int pix_lum(unsigned c)
{
    int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
    return (r * 30 + g * 59 + b * 11) / 100;
}
static int pix_close(unsigned a, unsigned b)
{
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF) - (int)(b & 0xFF);
    return dr * dr + dg * dg + db * db < 2500;
}


/* ---- Ricetta "soft UI": pillole, ombre morbide, accento ---- */
static int ui_soft(const gmb_widget_style_t* s) { return s->shape == 1 && s->radius >= 12; }
static unsigned ui_tint(unsigned c, unsigned a) { return (c & 0xFFFFFFu) | ((a & 0xFFu) << 24); }
static unsigned ui_accent(const gmb_widget_style_t* s) { return (s->accent & 0xFFFFFFu) ? s->accent : s->fill_press; }
static unsigned ui_card(const gmb_widget_style_t* s) { return s->skin == 2 ? s->fill : 0xFFFFFFFFu; }
static void ui_fill_accent(int x, int y, int w, int h, int r, const gmb_widget_style_t* s)
{
    if (s->skin != 0 && (s->grad_a & 0xFFFFFFu) && (s->grad_b & 0xFFFFFFu))
        GMB_GFX_FILL_RGRADIENT(x, y, w, h, r, s->grad_a, s->grad_b);
    else
        GMB_GFX_FILL_RRECT(x, y, w, h, r, ui_accent(s));
}
static void ui_fill_card(int x, int y, int w, int h, int r, const gmb_widget_style_t* s)
{
    GMB_GFX_FILL_RRECT(x, y, w, h, r, ui_card(s));
    GMB_GFX_STROKE_RRECT(x, y, w, h, r, s->border);
}

static void ui_soft_shadow(int x, int y, int w, int h, int r)
{
    if (w <= 0 || h <= 0) return;
    /* Ombra PROPORZIONATA all'elemento: sui campi bassi deve restare discreta. */
    /* Elementi bassi (campi): ombra corta e tenue; card grandi: piu' ampia. */
#ifdef GMB_WITH_UI
    { int blur2 = h <= 80 ? 4 : 10;
      gmb_gfx_drop_shadow_rounded(x, y, w, h, 1, ui_tint(0x000000u, 0x14u), r, blur2);
      if (h <= 80) return; }
#else
    (void)ui_tint; (void)r;
    if (h <= 80) return;
#endif
    int rh = r; if (rh > h / 3) rh = h / 3; if (rh < 2) rh = 2;
    int off = rh / 3; if (off < 1) off = 1; if (off > 4) off = 4;
    GMB_GFX_DROP_SHADOW(x, y, w, h, off, ui_tint(0x000000u, 0x16u), rh);
}

/* ---- Parsing colore ---- */
static int hx(char c) { if (c>='0'&&c<='9') return c-'0'; if (c>='a'&&c<='f') return c-'a'+10; if (c>='A'&&c<='F') return c-'A'+10; return -1; }
static unsigned int parse_color(const char* t)
{
    if (!t || !*t) return 0;
    if (t[0] == '#') {
        size_t n = strlen(t + 1);
        unsigned int v = (unsigned int)strtoul(t + 1, NULL, 16);
        if (n >= 8) return v;                       /* #AARRGGBB */
        if (n == 6) return 0xFF000000u | (v & 0xFFFFFFu);
        if (n == 3) { int r=hx(t[1]),g=hx(t[2]),b=hx(t[3]); if(r<0||g<0||b<0)return 0; return 0xFF000000u|(r*17<<16)|(g*17<<8)|(b*17); }
        return 0;
    }
    if (!strcasecmp(t, "white"))   return 0xFFFFFFFFu;
    if (!strcasecmp(t, "black"))   return 0xFF000000u;
    if (!strcasecmp(t, "red"))     return 0xFFFF0000u;
    if (!strcasecmp(t, "green"))   return 0xFF00C000u;
    if (!strcasecmp(t, "blue"))    return 0xFF3060FFu;
    if (!strcasecmp(t, "gray") || !strcasecmp(t, "grey")) return 0xFF808080u;
    if (!strcasecmp(t, "silver"))  return 0xFFC0C0C0u;
    if (!strcasecmp(t, "none") || !strcasecmp(t, "transparent")) return 0;
    { int r=0,g=0,b=0; if (sscanf(t, "%d,%d,%d", &r,&g,&b) >= 3)
        return 0xFF000000u | ((unsigned)(r&255)<<16) | ((unsigned)(g&255)<<8) | (unsigned)(b&255); }
    return 0;
}

/* Applica uno stile CSS-like ai default. Token separati da spazio: key | key=value. */
static void style_apply(gmb_widget_style_t* s, const char* spec)
{
    if (!spec || !*spec) return;
    char buf[512]; snprintf(buf, sizeof(buf), "%s", spec);
    char* save = NULL;
    for (char* tok = strtok_r(buf, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
        char key[64] = ""; const char* val = "";
        char* eq = strchr(tok, '=');
        if (eq) { size_t kl = (size_t)(eq - tok); if (kl > 63) kl = 63; memcpy(key, tok, kl); key[kl] = 0; val = eq + 1; }
        else snprintf(key, sizeof(key), "%s", tok);

        if      (!strcasecmp(key,"primary")) { /* primario = accento del tema */
            unsigned a = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_press;
            s->fill = (a & 0xFFFFFFu) | (0xFFu << 24); s->fill_hover = s->fill; s->fill_press = s->fill;
            s->text = 0xFFFFFFFFu;
        }
        else if (!strcasecmp(key,"flat"))      { s->shape = 0; s->radius = 0; }
        else if (!strcasecmp(key,"rounded"))   { s->shape = 1; if (s->radius <= 0) s->radius = 6; }
        else if (!strcasecmp(key,"radius"))    { s->shape = 1; s->radius = atoi(val); }
        else if (!strcasecmp(key,"shadow"))    { s->shadow_on = 1; if (*val) { int dx=0,dy=0; if (sscanf(val,"%d,%d",&dx,&dy)==2){s->shadow_dx=dx;s->shadow_dy=dy;} } }
        else if (!strcasecmp(key,"softshadow")){ s->shadow_on = 1; s->shadow_soft = 1; }
        else if (!strcasecmp(key,"noshadow"))  { s->shadow_on = 0; }
        else if (!strcasecmp(key,"raised"))    { s->bevel = 1; }
        else if (!strcasecmp(key,"sunken"))    { s->bevel = -1; }
        else if (!strcasecmp(key,"bevel"))     { s->bevel = 1; if (*val) s->bevel_depth = atoi(val); }
        else if (!strcasecmp(key,"emboss") || !strcasecmp(key,"embossed")) { s->emboss = 1; }
        else if (!strcasecmp(key,"noemboss"))  { s->emboss = 0; }
        else if (!strcasecmp(key,"gloss") || !strcasecmp(key,"glossy")) { s->gloss = 1; }
        else if (!strcasecmp(key,"nogloss"))   { s->gloss = 0; }
        else if (!strcasecmp(key,"gradient"))  { s->gradient_on = 1; if (*val) { char a[32]="",b[32]=""; if (sscanf(val,"%31[^,],%31s",a,b)==2){s->grad_top=parse_color(a);s->grad_bottom=parse_color(b);} } }
        else if (!strcasecmp(key,"nogradient")){ s->gradient_on = 0; }
        else if (!strcasecmp(key,"head"))      { s->head_bg = parse_color(val); }
        else if (!strcasecmp(key,"headfg"))    { s->head_fg = parse_color(val); }
        else if (!strcasecmp(key,"rowh"))      { s->row_h = atoi(val); }
        else if (!strcasecmp(key,"pad"))       { s->pad = atoi(val); }
        else if (!strcasecmp(key,"today"))     { s->today_bg = parse_color(val); }
        else if (!strcasecmp(key,"todayfg"))   { s->today_fg = parse_color(val); }
        else if (!strcasecmp(key,"sel"))       { s->sel_bg = parse_color(val); }
        else if (!strcasecmp(key,"selfg"))     { s->sel_fg = parse_color(val); }
        else if (!strcasecmp(key,"glow"))      { s->glow_on = 1; if (*val) s->glow = parse_color(val); }
        else if (!strcasecmp(key,"noglow"))    { s->glow_on = 0; }
        else if (!strcasecmp(key,"bg"))        { s->fill = parse_color(val); }
        else if (!strcasecmp(key,"hover"))     { s->fill_hover = parse_color(val); }
        else if (!strcasecmp(key,"press"))     { s->fill_press = parse_color(val); }
        else if (!strcasecmp(key,"disabled"))  { s->fill_disabled = parse_color(val); }
        else if (!strcasecmp(key,"fg") || !strcasecmp(key,"text")) { s->text = parse_color(val); }
        else if (!strcasecmp(key,"border"))    { int w=1; char c[32]=""; if (sscanf(val,"%d,%31s",&w,c)==2){ s->border_w=w; s->border=parse_color(c); } else if (*val) { s->border_w=1; s->border=parse_color(val); } }
        else if (!strcasecmp(key,"font")) {
            const char* comma = strchr(val, ',');
            if (comma) {
                size_t pl = (size_t)(comma - val);
                if (pl >= sizeof(s->font_path)) pl = sizeof(s->font_path) - 1;
                memcpy(s->font_path, val, pl); s->font_path[pl] = 0;
                s->font_size = atoi(comma + 1);
            } else if (*val && isdigit((unsigned char)*val)) {
                s->font_size = atoi(val);            /* solo dimensione: "font=34" */
            } else snprintf(s->font_path, sizeof(s->font_path), "%s", val);
            if (s->font_path[0] == '"') { memmove(s->font_path, s->font_path + 1, strlen(s->font_path)); size_t l = strlen(s->font_path); if (l && s->font_path[l-1] == '"') s->font_path[l-1] = 0; }
        }
    }
}

static void fill_rect_style(int x,int y,int w,int h,unsigned int rgba) { GMB_GFX_LINE(x,y,x+w-1,y+h-1,GFX_RGB(rgba),"BF"); }
static void border_rect_style(int x,int y,int w,int h,unsigned int rgba) { GMB_GFX_LINE(x,y,x+w-1,y+h-1,GFX_RGB(rgba),"B"); }

/* Disegna UN bottone (tutti gli effetti). sc = render scale (layout logico). */
static void draw_button_body(gmb_form_btn_t* b, float sc)
{
    {   /* centratura sempre corretta: usa la scala CORRENTE, non quella passata (puo' essere stale) */
        sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
        gmb_widget_style_t* s = &b->style;
        int x = b->base.x, y = b->base.y, w = b->base.width, h = b->base.height;
        unsigned int fill = !b->base.enabled ? s->fill_disabled
                          : (b->pressed ? s->fill_press : (b->hover ? s->fill_hover : s->fill));
        unsigned int tcol = b->base.enabled ? s->text : s->text_disabled;
        int rounded = (s->shape == 1);
        /* SOFT UI: i bottoni sono PILLOLE (radius = h/2). */
        int rad = s->radius;
        if (rounded && rad >= 12) { rad = h / 2; if (rad > 26) rad = 26; }

        /* Font per-bottone (default del tema = 0 -> font corrente). */
        if (s->font_size > 0) {
            const char* fp = s->font_path[0] ? s->font_path : GMB_GFX_FONT_PATH();
            GMB_GFX_FONT(fp, (int)(s->font_size * sc));
        }
        int th = (int)(GMB_GFX_TEXT_H() / sc + 0.5f); if (th <= 0) th = 14;

        /* 1) ombra PROPORZIONATA (blur reale). Prima, sui temi non "soft", era una
         * COPIA NERA dura offsettata della stessa dimensione: si vedeva come un
         * rettangolo scuro dietro al bottone ("ombra sbagliata"). */
        if (s->shadow_on) {
#ifdef GMB_WITH_UI
            int blur = (h <= 60) ? 4 : 8;
            gmb_gfx_drop_shadow_rounded(x, y, w, h, 1,
                                        ui_tint(0x000000u, s->shadow_soft ? 0x20u : 0x16u),
                                        rounded ? rad : 0, blur);
#else
            int layers = s->shadow_soft ? 3 : 2;
            for (int L = layers; L >= 1; L--) {
                unsigned int sh = s->shadow;
                unsigned a = ((sh >> 24) & 0xFF) / (unsigned)(L + 1);
                sh = (a << 24) | (sh & 0xFFFFFFu);
                int sx = x + s->shadow_dx + (L - 1), sy = y + s->shadow_dy + (L - 1);
                if (rounded) GMB_GFX_FILL_RRECT(sx, sy, w, h, rad, sh); else fill_rect_style(sx, sy, w, h, sh);
            }
#endif
        }
        /* 2) glow (anello esterno). */
        if (s->glow_on) {
            for (int g = s->bevel_depth + 2; g >= 1; g--) {
                unsigned int col = s->glow;
                int xx = x - g, yy = y - g, ww = w + 2 * g, hh = h + 2 * g;
                if (rounded) GMB_GFX_STROKE_RRECT(xx, yy, ww, hh, s->radius + g, col);
                else         border_rect_style(xx, yy, ww, hh, col);
            }
        }
        /* 3) riempimento (gradient se flat, altrimenti pieno). */
        if (s->gradient_on && !rounded) GMB_GFX_GRADIENT_RECT(x, y, w, h, s->grad_top, s->grad_bottom, 1);
        else if (rounded)               GMB_GFX_FILL_RRECT(x, y, w, h, rad, s->gradient_on ? s->grad_top : fill);
        else                            fill_rect_style(x, y, w, h, fill);

        /* 3b) GLOSS: riflesso BIANCO TRASLUCIDO che sfuma nel pieno (alpha-blend reale),
         * non un blocco di colore più chiaro: la tinta del riflesso non dipende dal fill,
         * quindi funziona su qualunque colore (navy, accent, grigio) e non crea scalini. */
        if (s->gloss) {
            gmb_gfx_fill_rounded_gradient(x, y, w, h, rounded ? rad : 0, 0x50FFFFFFu, 0x00FFFFFFu);
            int rr2 = rounded ? rad : 0;   /* filo di luce sul bordo alto */
            if (w > 2 * rr2 + 6)
                GMB_GFX_LINE(x + rr2, y + 1, x + w - 1 - rr2, y + 1, GFX_RGB(0x38FFFFFFu), "");
        }
        /* 4) bevel 3D (raised/sunken): luci/ombre sui bordi. */
        if (s->bevel != 0) {
            int d = s->bevel_depth > 0 ? s->bevel_depth : 1;
            int up = (s->bevel > 0);
            unsigned int lt = up ? s->bevel_light : s->bevel_dark;
            unsigned int dk = up ? s->bevel_dark : s->bevel_light;
            for (int k = 0; k < d; k++) {
                /* alto+sinistra */
                GMB_GFX_LINE(x + k, y + k, x + w - 1 - k, y + k, GFX_RGB(lt), "");
                GMB_GFX_LINE(x + k, y + k, x + k, y + h - 1 - k, GFX_RGB(lt), "");
                /* basso+destra */
                GMB_GFX_LINE(x + k, y + h - 1 - k, x + w - 1 - k, y + h - 1 - k, GFX_RGB(dk), "");
                GMB_GFX_LINE(x + w - 1 - k, y + k, x + w - 1 - k, y + h - 1 - k, GFX_RGB(dk), "");
            }
        }
        /* 5) bordo. */
        if (s->border_w > 0) {
            if (rounded) GMB_GFX_STROKE_RRECT(x, y, w, h, rad, s->border);
            else         border_rect_style(x, y, w, h, s->border);
        }
        /* 6) testo (emboss = incisione: ombra chiara sotto, testo sopra). */
        int tx = x + w / 2, ty = y + (h - th) / 2;
        if (s->emboss) {
            GMB_GFX_DRAW_TEXT(b->label, tx + 1, ty + 1, 1, GFX_RGB(s->bevel_light & 0xFFFFFFu ? s->bevel_light : 0x40FFFFFFu));
            GMB_GFX_DRAW_TEXT(b->label, tx - 1, ty - 1, 1, GFX_RGB(s->bevel_dark & 0xFFFFFFu ? s->bevel_dark : 0x80000000u));
        }
        GMB_GFX_DRAW_TEXT(b->label, tx, ty, 1, GFX_RGB(tcol));
    }
}

static void grid_sort(gmb_pix_widget_t* wd, int col)
{
    if (wd->gsort_col == col) wd->gsort_dir = !wd->gsort_dir; else { wd->gsort_col = col; wd->gsort_dir = 0; }
    for (int i = 0; i < wd->gnr; i++) for (int j = i + 1; j < wd->gnr; j++) {
        char a[64], b[64]; grid_cell(wd, wd->gorder[i], col, a, sizeof(a)); grid_cell(wd, wd->gorder[j], col, b, sizeof(b));
        int cmp;
        if (a[0] && b[0] && (isdigit((unsigned char)a[0]) || a[0] == '-') && (isdigit((unsigned char)b[0]) || b[0] == '-')) { double da = atof(a), db = atof(b); cmp = da < db ? -1 : (da > db ? 1 : 0); }
        else cmp = strcasecmp(a, b);
        if (wd->gsort_dir ? cmp < 0 : cmp > 0) { int t = wd->gorder[i]; wd->gorder[i] = wd->gorder[j]; wd->gorder[j] = t; }
    }
    g_dirty = 1;
}

/* Larghezza della fascia frecce: BLOCCO SEPARATO ATTACCATO al campo.
 * Regola (2026-11-06): fra il campo di input e il blocco frecce non ci sono spazi;
 * il blocco finisce sul bordo destro del controllo. La larghezza e' una cella di
 * testo + respiro, cosi' la freccia resta centrata e leggibile con ogni font. */
static int px_arrow_band_w(int cwl)
{
    int aw;
    if (cwl <= 0) cwl = 8;
    aw = cwl + 10;
    if (aw < 16) aw = 16;
    if (aw > 30) aw = 30;
    return aw;
}

/* Disegna la fascia frecce a filo del bordo destro.
 * arrows: 1 = singola (select), 2 = due frecce impilate (datetime).
 * Nessun margine fra il campo e la fascia: il bordo sinistro della fascia e' il
 * confine del campo (linea condivisa), e la fascia termina sul bordo del controllo. */
static void px_draw_arrow_band(int x, int y, int w, int h, int tl,
                               gmb_widget_style_t* s, int arrows, int open)
{
    int aw = px_arrow_band_w((int)(GMB_GFX_TEXT_END_X("M", 0, 0)));
    int bx, rr;
    if (aw > w / 2) aw = w / 2;
    bx = x + w - aw;
    rr = ui_soft(s) ? 12 : 6;
    GMB_GFX_FILL_RRECT(bx, y, aw, h, rr, s->fill_hover);
    GMB_GFX_STROKE_RRECT(bx, y, aw, h, rr, s->border);
    GMB_GFX_LINE(bx, y + 1, bx, y + h - 2, GFX_RGB(s->border), "");   /* attacco campo|blocco */
    if (arrows >= 2) {
        int hh = h / 2;
        GMB_GFX_LINE(bx + 1, y + hh, bx + aw - 1, y + hh, GFX_RGB(s->border), "");
        GMB_GFX_DRAW_TEXT("^", bx + aw / 2, y + (hh - tl) / 2, 1, GFX_RGB(s->text));
        GMB_GFX_DRAW_TEXT("v", bx + aw / 2, y + hh + (hh - tl) / 2, 1, GFX_RGB(s->text));
    } else {
        GMB_GFX_DRAW_TEXT(open ? "^" : "v", bx + aw / 2, y + (h - tl) / 2, 1, GFX_RGB(s->text));
    }
}

/* Disegna i widget pixel (checkbox/radio/track/dropdown/progress). */
static void draw_one_widget(gmb_pix_widget_t* wd, float sc)
{
    int tl = (int)(GMB_GFX_TEXT_H() / sc + 0.5f); if (tl <= 0) tl = 14;
    int cwl = GMB_GFX_TEXT_END_X("M", 0, 0); if (cwl <= 0) cwl = 8;
    {
        int i = (int)(wd - g_wid);   /* per i confronti con g_focus_wid */
        wd->style = gmb_widget_style_current();   /* tema corrente (row_h, radius... sempre aggiornati) */
        gmb_widget_style_t* s = &wd->style;
        int x = wd->base.x, y = wd->base.y, w = wd->base.width, h = wd->base.height;
        if (wd->kind == W_PANEL) {
            /* Card di sezione: stessa ricetta delle card del tema. */
            int soft = ui_soft(s);
            int rr = soft ? 16 : 4;
            if (soft) ui_soft_shadow(x, y, w, h, rr);
            gmb_gfx_fill_rounded_rect(x, y, w, h, rr, (s->skin == 2) ? s->fill : 0xFFFFFFFFu);
            gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, s->border);
            if (wd->label[0]) gmb_gfx_draw_text(wd->label, x + 20, y + 14, 0, (int)(0x40000000u | (s->text_disabled & 0xFFFFFFu)));
        } else if (wd->kind == W_CHECK) {
            int soft = ui_soft(s);
            int bs = soft ? 22 : 18, by = y + (h - bs) / 2;
            int on = wd_num(wd) != 0;
            if (soft)
            {
                unsigned acc = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_press;
                ui_soft_shadow(x, by, bs, bs, 7);
                if (on) ui_fill_accent(x, by, bs, bs, 7, s); else GMB_GFX_FILL_RRECT(x, by, bs, bs, 7, ui_card(s));
                if (!on) GMB_GFX_STROKE_RRECT(x, by, bs, bs, 7, s->border);
                if (on)
                {
                    GMB_GFX_LINE(x + 5, by + 11, x + 9, by + 15, 0x40FFFFFFu, "");
                    GMB_GFX_LINE(x + 9, by + 15, x + 17, by + 6, 0x40FFFFFFu, "");
                }
            }
            else
            {
                GMB_GFX_FILL_RRECT(x, by, bs, bs, 4, s->fill);
                GMB_GFX_STROKE_RRECT(x, by, bs, bs, 4, s->border);
                if (on) {
                    GMB_GFX_LINE(x + 4, by + 9, x + 7, by + 13, GFX_RGB(s->text), "");
                    GMB_GFX_LINE(x + 7, by + 13, x + 14, by + 4, GFX_RGB(s->text), "");
                }
            }
            GMB_GFX_DRAW_TEXT(wd->label, x + bs + 10, y + (h - tl) / 2, 0, GFX_RGB(s->text));
        } else if (wd->kind == W_RADIO) {
            int soft = ui_soft(s);
            int sel = (int)(wd_num(wd) + 0.5);
            int dia = soft ? 22 : 16, rowH = soft ? 26 : 22;
            unsigned acc = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_press;
            for (int j = 0; j < wd->nitems; j++) {
                int cy = y + j * rowH + dia / 2;
                if (soft) {
                    if (j == sel) { GMB_GFX_FILL_CIRCLE(x + dia / 2, cy, dia, ui_accent(s)); GMB_GFX_FILL_CIRCLE(x + dia / 2, cy, dia - 10, 0xFFFFFFFFu); }
                    else { GMB_GFX_FILL_CIRCLE(x + dia / 2, cy, dia, 0xFFFFFFFFu); GMB_GFX_FILL_CIRCLE(x + dia / 2, cy, dia - 3, s->border); GMB_GFX_FILL_CIRCLE(x + dia / 2, cy, dia - 4, 0xFFFFFFFFu); }
                } else {
                    GMB_GFX_FILL_CIRCLE(x + 9, cy, 16, s->border);
                    GMB_GFX_FILL_CIRCLE(x + 9, cy, 12, s->fill);
                    if (j == sel) GMB_GFX_FILL_CIRCLE(x + 9, cy, 7, s->text);
                }
                GMB_GFX_DRAW_TEXT(wd->items[j], x + dia + 10, cy - tl / 2, 0, GFX_RGB(s->text));
            }
        } else if (wd->kind == W_TRACK) {
            int soft = ui_soft(s);
            int gy = y + h / 2;
            double v = wd_num(wd);
            double t = (wd->vmax > wd->vmin) ? (v - wd->vmin) / (wd->vmax - wd->vmin) : 0;
            if (t < 0) t = 0; if (t > 1) t = 1;
            int kx = x + (int)(t * (w - 1));
            int tk = soft ? 6 : 6, kr = soft ? 10 : 9;
            unsigned acc = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_hover;
            GMB_GFX_FILL_RRECT(x, gy - tk / 2, w, tk, tk / 2, soft ? s->border : s->border);
            if (soft && kx > x) GMB_GFX_FILL_RRECT(x, gy - tk / 2, kx - x, tk, tk / 2, acc);
            if (soft) ui_soft_shadow(kx - kr, gy - kr, kr * 2, kr * 2, kr);
            GMB_GFX_FILL_CIRCLE(kx, gy, kr * 2, 0xFFFFFFFFu);
            GMB_GFX_FILL_CIRCLE(kx, gy, kr * 2 - 2, soft ? 0xFFFFFFFFu : s->fill_hover);
        } else if (wd->kind == W_DROPDOWN) {
            int soft = ui_soft(s);
            int rr = soft ? 12 : 6;
            if (soft) ui_soft_shadow(x, y, w, h, rr);
            ui_fill_card(x, y, w, h, rr, s);
            GMB_GFX_STROKE_RRECT(x, y, w, h, rr, s->border);
            int sel = (int)(wd_num(wd) + 0.5); if (sel < 0) sel = 0; if (sel >= wd->nitems) sel = wd->nitems ? wd->nitems - 1 : 0;
            if (wd->nitems) GMB_GFX_DRAW_TEXT(wd->items[sel], x + (soft ? 14 : 8), y + (h - tl) / 2, 0, GFX_RGB(s->text));
            /* Fascia frecce attaccata al campo (nessuno spazio fra input e freccia). */
            px_draw_arrow_band(x, y, w, h, tl, s, 1, wd->open);
        } else if (wd->kind == W_PROGRESS) {
            /* Barra OS-style guidata dal tema: pill (shape/radius), traccia fill, riempimento
             * accent (fallback head_bg/title_bg quando coincide con la traccia, es. Turbo),
             * bordo e percentuale con colore a contrasto rispetto a cio' che sta sotto. */
            int prad = (s->shape == 1) ? h / 2 : 0;
            unsigned pfill = s->accent;
            if (!(pfill & 0xFFFFFFu) || pix_close(pfill, s->fill)) pfill = s->head_bg;
            if (!(pfill & 0xFFFFFFu) || pix_close(pfill, s->fill)) pfill = s->title_bg;
            if (!(pfill & 0xFFFFFFu) || pix_close(pfill, s->fill)) pfill = s->sel_bg;
            GMB_GFX_FILL_RRECT(x, y, w, h, prad, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, prad, s->border);
            double v = wd_num(wd); if (v < 0) v = 0; if (v > 100) v = 100;
            int fw = (int)(w * v / 100.0 + 0.5);
            if (fw > 0) { if (s->skin != 0 && (s->grad_a & 0xFFFFFFu)) GMB_GFX_FILL_RGRADIENT(x, y, fw, h, prad, s->grad_a, s->grad_b); else GMB_GFX_FILL_RRECT(x, y, fw, h, prad, pfill); }
            if (wd->show_percent) {
                char pb[16]; snprintf(pb, sizeof(pb), "%.0f%%", v);
                unsigned lbl = s->text;
                if (w / 2 < fw) lbl = pix_lum(pfill) < 128 ? 0xFFFFFFu : 0x000000u;
                GMB_GFX_DRAW_TEXT(pb, x + w / 2, y + (h - tl) / 2, 1, GFX_RGB(lbl));
            }
        } else if (wd->kind == W_TOGGLE) {
            int soft = ui_soft(s);
            int on = wd_num(wd) != 0;
            int th2 = soft ? 28 : 24, tw = soft ? 66 : 46;
            if (soft)
            {
                unsigned acc = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_hover;
                ui_soft_shadow(x, y, tw, th2, th2 / 2);
                if (on) ui_fill_accent(x, y, tw, th2, th2 / 2, s); else GMB_GFX_FILL_RRECT(x, y, tw, th2, th2 / 2, s->border);
                int kd = th2 - 6, kx = on ? x + tw - kd - 3 : x + 3;
                ui_soft_shadow(kx, y + 3, kd, kd, kd / 2);
                GMB_GFX_FILL_CIRCLE(kx + kd / 2, y + th2 / 2, kd, 0xFFFFFFFFu);
                /* NIENTE testo ON/OFF dentro la pillola: si sovrapponeva al cursore.
                 * Lo stato lo dicono posizione del cursore + colore. */
            }
            else
            {
                GMB_GFX_FILL_RRECT(x, y, tw, th2, th2 / 2, on ? s->fill_hover : s->fill);
                GMB_GFX_STROKE_RRECT(x, y, tw, th2, th2 / 2, s->border);
                GMB_GFX_FILL_CIRCLE(on ? x + tw - th2 / 2 : x + th2 / 2, y + th2 / 2, th2 - 6, on ? s->text : s->border);
            }
            GMB_GFX_DRAW_TEXT(wd->label, x + tw + 12, y + (th2 - tl) / 2, 0, GFX_RGB(s->text));
        } else if (wd->kind == W_SPINNER) {
            GMB_GFX_FILL_RRECT(x, y, w, h, 6, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, 6, s->border);
            char vb[32]; double v = wd_num(wd); snprintf(vb, sizeof(vb), "%.0f", v);
            GMB_GFX_DRAW_TEXT(vb, x + 10, y + (h - tl) / 2, 0, GFX_RGB(s->text));
            int bwd = 26;
            GMB_GFX_FILL_RRECT(x + w - 2 * bwd, y, bwd, h, 0, s->fill_hover);
            GMB_GFX_STROKE_RRECT(x + w - 2 * bwd, y, bwd, h, 0, s->border);
            GMB_GFX_FILL_RRECT(x + w - bwd, y, bwd, h, 0, s->fill_hover);
            GMB_GFX_STROKE_RRECT(x + w - bwd, y, bwd, h, 0, s->border);
            GMB_GFX_DRAW_TEXT("-", x + w - 2 * bwd + bwd / 2, y + (h - tl) / 2, 1, GFX_RGB(s->text));
            GMB_GFX_DRAW_TEXT("+", x + w - bwd + bwd / 2, y + (h - tl) / 2, 1, GFX_RGB(s->text));
        } else if (wd->kind == W_STATUSBAR) {
            int soft = ui_soft(s);
            int rr = soft ? 12 : 4;
            if (soft) ui_soft_shadow(x, y, w, h, rr);
            ui_fill_card(x, y, w, h, rr, s);
            GMB_GFX_STROKE_RRECT(x, y, w, h, rr, s->border);
            GMB_GFX_DRAW_TEXT(wd->label, x + (soft ? 16 : 8), y + (h - tl) / 2, 0, GFX_RGB(soft ? s->text_disabled : s->text));
        } else if (wd->kind == W_MENUBAR) {
            int soft = ui_soft(s);
            int sel = (int)(wd_num(wd) + 0.5), mx = x + (soft ? 6 : 4);
            int total = soft ? 12 : 8;
            for (int j = 0; j < wd->nitems; j++) total += (int)(GMB_GFX_TEXT_END_X(wd->items[j], 0, 0) - GMB_GFX_TEXT_END_X("", 0, 0)) + (soft ? 26 : 18);
            if (soft) { ui_soft_shadow(x, y, total, h, 12); ui_fill_card(x, y, total, h, 12, s); }
            else { GMB_GFX_FILL_RRECT(x, y, total, h, 0, s->title_bg); }
            for (int j = 0; j < wd->nitems; j++) {
                int iw = (int)(GMB_GFX_TEXT_END_X(wd->items[j], 0, 0) - GMB_GFX_TEXT_END_X("", 0, 0)) + (soft ? 26 : 18);
                int hot = (j == sel || j == wd->hover);
                if (hot) {
                    if (soft) ui_fill_accent(mx, y + 3, iw - 4, h - 6, (h - 6) / 2, s);
                    else GMB_GFX_LINE(mx, y, mx + iw - 1, y + h - 1, GFX_RGB(s->title_fg), "BF");
                }
                GMB_GFX_DRAW_TEXT(wd->items[j], mx + (soft ? 11 : 9), y + (h - tl) / 2, 0,
                                  GFX_RGB(hot ? (soft ? 0x40FFFFFFu : s->title_bg) : s->text));
                mx += iw;
            }
        } else if (wd->kind == W_TABS) {
            int soft = ui_soft(s);
            int rr = soft ? 14 : 6;
            int sel = (int)(wd_num(wd) + 0.5);
            unsigned acc = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_hover;
            int total = 0;
            for (int j = 0; j < wd->nitems; j++) { int tw0 = (int)(GMB_GFX_TEXT_W(wd->items[j]) / sc) + 24; if (tw0 < 64) tw0 = 64; total += tw0 + (soft ? 2 : 4); }
            if (soft) { ui_soft_shadow(x, y, total, h, rr); ui_fill_card(x, y, total, h, rr, s); }
            int tx = x + (soft ? 4 : 0);
            for (int j = 0; j < wd->nitems; j++) {
                int tw = (int)(GMB_GFX_TEXT_W(wd->items[j]) / sc) + (soft ? 24 : 20); if (tw < (soft ? 64 : 60)) tw = soft ? 64 : 60;
                int act = (j == sel);
                if (soft)
                {
                    if (act) ui_fill_accent(tx, y + 4, tw, h - 8, (h - 8) / 2, s);
                    GMB_GFX_DRAW_TEXT(wd->items[j], tx + tw / 2, y + (h - tl) / 2, 1, GFX_RGB(act ? 0x40FFFFFFu : s->text_disabled));
                }
                else
                {
                    GMB_GFX_FILL_RRECT(tx, y, tw, h, 6, act ? s->fill_hover : s->fill);
                    GMB_GFX_STROKE_RRECT(tx, y, tw, h, 6, s->border);
                    GMB_GFX_DRAW_TEXT(wd->items[j], tx + tw / 2, y + (h - tl) / 2, 1, GFX_RGB(s->text));
                }
                tx += tw + (soft ? 2 : 4);
            }
        } else if (wd->kind == W_LISTBOX) {
            int soft = ui_soft(s);
            int rr = soft ? 12 : 6;
            unsigned acc = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_hover;
            if (soft) ui_soft_shadow(x, y, w, h, rr);
            ui_fill_card(x, y, w, h, rr, s);
            GMB_GFX_STROKE_RRECT(x, y, w, h, rr, s->border);
            int rowH = soft ? 26 : 22, rows = h / rowH, sel = (int)(wd_num(wd) + 0.5);
            for (int r = 0; r < rows && wd->first + r < wd->nitems; r++) {
                int j = wd->first + r, ry = y + r * rowH;
                unsigned int tcol = s->text;
                if (j == sel) {
                    if (soft) GMB_GFX_FILL_RRECT(x + 4, ry + 1, w - 8, rowH - 2, (rowH - 2) / 2, ui_tint(acc, 0x2A));
                    else {
                        /* selezione classica: se sel_bg coincide con lo sfondo della card
                         * (turbovision) si schiarisce lo sfondo: altrimenti è invisibile. */
                        unsigned selbg = s->sel_bg;
                        if ((selbg & 0xFFFFFFu) == (s->fill & 0xFFFFFFu))
                        {
                            unsigned b = s->fill & 0xFFFFFFu;
                            unsigned lr = (((b >> 16) & 0xFF) + 255) / 2;
                            unsigned lg = (((b >> 8) & 0xFF) + 255) / 2;
                            unsigned lb = ((b & 0xFF) + 255) / 2;
                            selbg = (lr << 16) | (lg << 8) | lb;
                        }
                        GMB_GFX_LINE(x + 1, ry, x + w - 2, ry + rowH - 1, GFX_RGB(selbg), "BF");
                        if ((s->sel_fg & 0xFFFFFFu) == (selbg & 0xFFFFFFu)) tcol = s->text;
                        else tcol = s->sel_fg;
                    }
                }
                const char* it = wd->items[j];
                if (it[0] == '@' && (it[1] == 'g' || it[1] == 'G')) { tcol = 0xFF16A34Au; it += 2; if (*it == ' ') it++; }
                else if (it[0] == '@' && (it[1] == 'r' || it[1] == 'R')) { tcol = 0xFFDC2626u; it += 2; if (*it == ' ') it++; }
                GMB_GFX_DRAW_TEXT(it, x + (soft ? 14 : 8), ry + (rowH - tl) / 2, 0, GFX_RGB(tcol));
            }
        } else if (wd->kind == W_COLORPICKER) {
            int sel = (int)(wd_num(wd) + 0.5);
            for (int j = 0; j < wd->ncolors; j++) {
                int cx = x + (j % 8) * 26, cy = y + (j / 8) * 26;
                GMB_GFX_FILL_RRECT(cx, cy, 24, 24, 4, wd->colors[j]);
                if (j == sel) GMB_GFX_STROKE_RRECT(cx - 1, cy - 1, 26, 26, 5, s->text);
                else GMB_GFX_STROKE_RRECT(cx, cy, 24, 24, 4, s->border);
            }
        } else if (wd->kind == W_TREE) {
            int soft = ui_soft(s);
            int rr = soft ? 12 : 6;
            unsigned acc = (s->accent & 0xFFFFFFu) ? s->accent : s->fill_hover;
            if (soft) ui_soft_shadow(x, y, w, h, rr);
            ui_fill_card(x, y, w, h, rr, s);
            GMB_GFX_STROKE_RRECT(x, y, w, h, rr, s->border);
            int rh = s->row_h > 0 ? s->row_h : 22, sel = (int)(wd_num(wd) + 0.5), vr = 0;
            for (int j = 0; j < wd->nitems; j++) {
                if (tree_hidden(wd, j)) continue;
                int depth = tree_depth(wd->items[j]); const char* nm = tree_name(wd->items[j]);
                int ry = y + vr * rh; vr++;
                if (ry + rh > y + h) break;
                int bx = x + (soft ? 12 : 6) + depth * (tl / 2 + 4);
                if (j == sel) {
                    if (soft) GMB_GFX_FILL_RRECT(x + 4, ry, w - 8, rh - 1, (rh - 1) / 2, ui_tint(acc, 0x2A));
                    else      GMB_GFX_LINE(x + 1, ry, x + w - 2, ry + rh - 1, GFX_RGB(s->fill_hover), "BF");
                }
                if (tree_has_children(wd, j)) GMB_GFX_DRAW_TEXT(wd->tcol[j] ? "+" : "-", bx, ry + (rh - tl) / 2, 0, GFX_RGB(s->text_disabled));
                GMB_GFX_DRAW_TEXT(nm, bx + tl + 2, ry + (rh - tl) / 2, 0, GFX_RGB(s->text));
            }
        } else if (wd->kind == W_TEXT) {
            int soft = ui_soft(s);
            int rr = soft ? 12 : 4;
            if (soft) ui_soft_shadow(x, y, w, h, rr);
            ui_fill_card(x, y, w, h, rr, s);
            if (i == g_focus_wid) gmb_gfx_stroke_rounded_rect(x - 2, y - 2, w + 4, h + 4, rr + 2, 0x406C5CE7u);
            const char* tv = wd_str(wd);
            GMB_GFX_DRAW_TEXT(tv, x + 6, y + (h - tl) / 2, 0, GFX_RGB(s->text));
            if (i == g_focus_wid) {   /* caret alla posizione del cursore */
                int tc = wd->tcur; size_t L = strlen(tv); if ((size_t)tc > L) tc = (int)L; if (tc > 255) tc = 255;
                char pre[256]; memcpy(pre, tv, (size_t)tc); pre[tc] = 0;
                int cx = x + 6 + (int)(GMB_GFX_TEXT_W(pre) / sc);
                GMB_GFX_LINE(cx, y + 5, cx, y + h - 5, GFX_RGB(s->text), "");
            }
        } else if (wd->kind == W_FILE) {
            GMB_GFX_FILL_RRECT(x, y, w, h, 6, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, 6, s->border);
            int rh = tl + 4, vis = h / rh;
            for (int vr = 0; vr < vis && wd->bfirst + vr < wd->nbig; vr++) {
                int r = wd->bfirst + vr, ry = y + vr * rh;
                if (r == wd->bsel) GMB_GFX_LINE(x + 1, ry, x + w - 2, ry + rh - 1, GFX_RGB(s->fill_hover), "BF");
                GMB_GFX_DRAW_TEXT(wd->big[r], x + 6, ry + 1, 0, GFX_RGB(s->text));
            }
        } else if (wd->kind == W_GRID) {
            int rh = s->row_h > 0 ? s->row_h : 22, cwd = wd->gnc > 0 ? w / wd->gnc : w, pd = s->pad > 0 ? s->pad : 6;
            GMB_GFX_LINE(x, y, x + w - 1, y + rh - 1, GFX_RGB(s->head_bg), "BF");
            for (int c2 = 0; c2 < wd->gnc; c2++) GMB_GFX_DRAW_TEXT(wd->ghead[c2], x + c2 * cwd + pd, y + 2, 0, GFX_RGB(s->head_fg));
            GMB_GFX_LINE(x, y + rh - 1, x + w - 1, y + rh - 1, GFX_RGB(s->border), "");   /* separatore header (colore tema) */
            int vis = (h - rh) / rh, sel = (int)(wd_num(wd) + 0.5);
            for (int vr = 0; vr < vis && wd->first + vr < wd->gnr; vr++) {
                int r = wd->gorder[wd->first + vr], ry = y + rh + vr * rh;
                if (r == sel) GMB_GFX_LINE(x + 1, ry, x + w - 2, ry + rh - 1, GFX_RGB(s->fill_hover), "BF");
                for (int c2 = 0; c2 < wd->gnc; c2++) { char cell[64]; grid_cell(wd, r, c2, cell, sizeof(cell)); GMB_GFX_DRAW_TEXT(cell, x + c2 * cwd + pd, ry + 2, 0, GFX_RGB(s->text)); }
                GMB_GFX_LINE(x, ry + rh - 1, x + w - 1, ry + rh - 1, GFX_RGB(s->border), "");
            }
            GMB_GFX_LINE(x, y, x + w - 1, y + h - 1, GFX_RGB(s->border), "B");
        } else if (wd->kind == W_DATE) {
            {   /* larghezza: testo + FASCIA FRECCE + padding, nessuno spazio interno */
                int aw = px_arrow_band_w(cwl);
                int padl = (ui_soft(s) ? 14 : 8);
                char mv[32]; if (wd->sd > 0) date_format(wd->dfmt, wd->sy, wd->sm, wd->sd, mv, sizeof(mv)); else date_mask(wd->dfmt, mv, sizeof(mv));
                w = GMB_GFX_TEXT_END_X(mv, 0, 0) + aw + padl + 4; wd->base.width = w;
            }
            GMB_GFX_FILL_RRECT(x, y, w, h, 6, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, 6, s->border);
            char dbuf[32]; if (wd->sd > 0) date_format(wd->dfmt, wd->sy, wd->sm, wd->sd, dbuf, sizeof(dbuf)); else dbuf[0] = 0;
            int fld_focused = (i == g_focus_wid && wd->kind == W_DATE);
            const char* shown = fld_focused ? wd->dmask : dbuf;
            unsigned int tcol = s->text;
            if (fld_focused && !date_mask_valid(wd)) tcol = 0xFFFF3B30u;   /* rosso finche' invalida */
            {   int padl = (ui_soft(s) ? 14 : 8);
                /* testo a sinistra: il campo termina esattamente dove inizia la fascia */
                GMB_GFX_DRAW_TEXT(shown, x + padl, y + (h - tl) / 2, 0, GFX_RGB(tcol));
                if (fld_focused) { char pre[16]; int dc = wd->dcur; if (dc < 0) dc = 0; if (dc > (int)strlen(shown)) dc = (int)strlen(shown); memcpy(pre, shown, (size_t)dc); pre[dc] = 0; int wp = GMB_GFX_TEXT_END_X(pre, 0, 0); int cx = x + padl + wp; GMB_GFX_LINE(cx, y + 3, cx, y + h - 3, GFX_RGB(s->text), ""); }
            }
            /* Fascia frecce attaccata al campo (apre/chiude il calendario). */
            px_draw_arrow_band(x, y, w, h, tl, s, 1, wd->open);
        } else if (wd->kind == W_CANVAS) {
            GMB_GFX_FILL_RRECT(x, y, w, h, 6, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, 6, s->border);
        } else if (wd->kind == W_IMAGE) {
            GMB_GFX_FILL_RRECT(x, y, w, h, 4, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, 4, s->border);
            GMB_GFX_LINE(x + 2, y + 2, x + w - 3, y + h - 3, GFX_RGB(s->border), "");
            GMB_GFX_LINE(x + w - 3, y + 2, x + 2, y + h - 3, GFX_RGB(s->border), "");
            if (wd->label[0]) GMB_GFX_DRAW_TEXT(wd->label, x + 6, y + (h - tl) / 2, 0, GFX_RGB(s->text));
        } else if (wd->kind == W_IMAGEBUTTON) {
            GMB_GFX_FILL_RRECT(x, y, w, h, 6, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, 6, s->border);
            int iw = h - 8; if (iw < 8) iw = 8; if (iw > 40) iw = 40;
            GMB_GFX_STROKE_RRECT(x + 4, y + 4, iw, iw, 3, s->border);
            GMB_GFX_DRAW_TEXT(wd->label, x + 6 + iw + 4, y + (h - tl) / 2, 0, GFX_RGB(s->text));
        } else if (wd->kind == W_QR) {
            GMB_GFX_FILL_RRECT(x, y, w, h, 2, s->fill);
            int side = (w < h ? w : h) - 8; if (side < 8) side = 8;
            int nq = 21, cell = side / nq; if (cell < 1) cell = 1;
            unsigned hsh = 2166136261u; for (const char* q = wd->label; q && *q; q++) hsh = (hsh ^ (unsigned char)*q) * 16777619u;
            for (int qy = 0; qy < nq; qy++)
                for (int qx = 0; qx < nq; qx++) {
                    int finder = (qx < 7 && qy < 7) || (qx >= nq - 7 && qy < 7) || (qx < 7 && qy >= nq - 7);
                    int mod;
                    if (finder) {
                        int lx = qx < 7 ? qx : qx - (nq - 7);
                        int ly2 = qy < 7 ? qy : qy - (nq - 7);
                        mod = (lx == 0 || lx == 6 || ly2 == 0 || ly2 == 6 || (lx >= 2 && lx <= 4 && ly2 >= 2 && ly2 <= 4)) ? 1 : 0;
                    } else mod = (int)((hsh >> ((qx * 7 + qy) & 31)) & 1u);
                    if (mod) GMB_GFX_FILL_RRECT(x + 4 + qx * cell, y + 4 + qy * cell, cell, cell, 0, s->text);
                }
            GMB_GFX_STROKE_RRECT(x, y, w, h, 2, s->border);
        } else if (wd->kind == W_RICHTEXT) {
            GMB_GFX_FILL_RRECT(x, y, w, h, 4, s->fill);
            GMB_GFX_STROKE_RRECT(x, y, w, h, 4, s->border);
            int ly2 = y + 4;
            for (int r = 0; r < wd->nitems; r++) { GMB_GFX_DRAW_TEXT(wd->items[r], x + 6, ly2, 0, GFX_RGB(r == 0 ? s->head_fg : s->text)); ly2 += tl + 2; }
            if (wd->nitems == 0 && wd->label[0]) GMB_GFX_DRAW_TEXT(wd->label, x + 6, y + 4, 0, GFX_RGB(s->text));
        } else if (wd->kind == W_HALFBLOCK) {
            int bw2 = 6, bh2 = 6;
            for (int by2 = y; by2 < y + h; by2 += bh2)
                for (int bx2 = x; bx2 < x + w; bx2 += bw2) {
                    unsigned cap = (((bx2 - x) / bw2) + ((by2 - y) / bh2)) & 1u;
                    GMB_GFX_FILL_RRECT(bx2, by2, bw2 - 1, bh2 - 1, 0, cap ? s->fill_hover : s->fill);
                }
            if (wd->label[0]) GMB_GFX_DRAW_TEXT(wd->label, x + 6, y + (h - tl) / 2, 0, GFX_RGB(s->text));
        } else if (wd->kind == W_TILING) {
            int tw2 = 16, th2 = 16;
            for (int by2 = y; by2 < y + h; by2 += th2)
                for (int bx2 = x; bx2 < x + w; bx2 += tw2) {
                    GMB_GFX_FILL_RRECT(bx2, by2, tw2 - 2, th2 - 2, 3, s->fill);
                    GMB_GFX_STROKE_RRECT(bx2, by2, tw2 - 2, th2 - 2, 3, s->border);
                }
            if (wd->label[0]) GMB_GFX_DRAW_TEXT(wd->label, x + 6, y + (h - tl) / 2, 0, GFX_RGB(s->text));
        }
    }
}

void gmb_px_draw_ex(int wkind, int x, int y, int w, int h, const char* label,
                    const char* const* items, int nitems, double value, double vmin, double vmax,
                    const double* var, int first, const unsigned* colors, int ncolors)
{
    gmb_pix_widget_t wd; memset(&wd, 0, sizeof(wd));
    wd.kind = wkind;
    wd.base.x = x; wd.base.y = y; wd.base.width = w; wd.base.height = h;
    if (label) snprintf(wd.label, sizeof(wd.label), "%s", label);
    wd.nitems = nitems > 8 ? 8 : nitems;
    for (int i = 0; i < wd.nitems; i++) snprintf(wd.items[i], sizeof(wd.items[i]), "%s", items && items[i] ? items[i] : "");
    wd.ival = value; wd.vmin = vmin; wd.vmax = vmax; wd.var = (double*)var; wd.first = first;
    wd.ncolors = ncolors > 16 ? 16 : ncolors; for (int i = 0; i < wd.ncolors; i++) wd.colors[i] = colors ? colors[i] : 0;
    wd.style = gmb_widget_style_current();
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
    draw_one_widget(&wd, sc);
}

static void draw_widgets(float sc) { for (int i = 0; i < g_nwid; i++) { if (!g_wid[i].base.visible) continue; draw_one_widget(&g_wid[i], sc); } }
/* Popup dei dropdown aperti: disegnato per ULTIMO (sopra ogni altro controllo). */
static void draw_open_dropdowns(float sc)
{
    int tl = (int)(GMB_GFX_TEXT_H() / sc + 0.5f); if (tl <= 0) tl = 14;
    for (int i = 0; i < g_nwid; i++) {
        gmb_pix_widget_t* wd = &g_wid[i];
        if (wd->kind == W_DATE && wd->open) {
            gmb_widget_style_t* s = &wd->style;
            int x = wd->base.x, y = wd->base.y, h = wd->base.height, cell = tl + 6;
            int prx = x, pry = y + h, pw = 7 * cell, ph = 8 * cell;
            GMB_GFX_LINE(prx, pry, prx + pw - 1, pry + ph - 1, GFX_RGB(s->fill), "BF");
            char mt[32]; snprintf(mt, sizeof(mt), "%02d/%04d", wd->dm, wd->dy);
            GMB_GFX_LINE(prx, pry, prx + pw - 1, pry + cell - 1, GFX_RGB(s->text), "BF");   /* header invertito */
            GMB_GFX_DRAW_TEXT("<<", prx + 2, pry + 2, 0, GFX_RGB(s->fill));
            GMB_GFX_DRAW_TEXT("<", prx + cell + 2, pry + 2, 0, GFX_RGB(s->fill));
            GMB_GFX_DRAW_TEXT(">", prx + pw - 2 * cell + 2, pry + 2, 0, GFX_RGB(s->fill));
            GMB_GFX_DRAW_TEXT(">>", prx + pw - cell, pry + 2, 0, GFX_RGB(s->fill));
            GMB_GFX_DRAW_TEXT(mt, prx + pw / 2, pry + 2, 1, GFX_RGB(s->fill));
            { const char* wdl[7] = { "L", "M", "M", "G", "V", "S", "D" };
              for (int wi = 0; wi < 7; wi++) GMB_GFX_DRAW_TEXT(wdl[wi], prx + wi * cell + cell / 2, pry + cell + 1, 1, GFX_RGB(s->text)); }
            int fw = date_first_wday(wd->dy, wd->dm), dim = date_dim(wd->dy, wd->dm);
            time_t tn = time(NULL); struct tm* ltn = localtime(&tn);
            int ty = ltn->tm_year + 1900, tmn = ltn->tm_mon + 1, tdn = ltn->tm_mday;
            for (int d = 1; d <= dim; d++) {
                int idx = fw + d - 1, r = idx / 7, c2 = idx % 7;
                int dx = prx + c2 * cell, dy2 = pry + (2 + r) * cell;
                char db[8]; snprintf(db, sizeof(db), "%d", d);
                int is_today = (wd->dy == ty && wd->dm == tmn && d == tdn);
                int is_sel = (wd->dy == wd->sy && wd->dm == wd->sm && d == wd->sd);
                if (is_today) GMB_GFX_LINE(dx + 1, dy2, dx + cell - 2, dy2 + cell - 2, GFX_RGB(s->today_bg), "BF");   /* oggi: sfondo */
                if (is_sel) GMB_GFX_LINE(dx + 1, dy2, dx + cell - 2, dy2 + cell - 2, GFX_RGB(s->sel_bg), "BF");       /* selezionato: sfondo (sopra) */
                if (is_today && is_sel) GMB_GFX_LINE(dx + 1, dy2, dx + cell - 2, dy2 + cell - 2, GFX_RGB(s->today_bg), "B");   /* oggi==sel: bordo oggi */
                GMB_GFX_DRAW_TEXT(db, dx + cell / 2, dy2 + 1, 1, GFX_RGB(is_sel ? s->sel_fg : (is_today ? s->today_fg : s->text)));
            }
            GMB_GFX_LINE(prx, pry, prx + pw - 1, pry + ph - 1, GFX_RGB(s->border), "B");
            continue;
        }
        if (wd->kind != W_DROPDOWN || !wd->open || !wd->nitems) continue;
        gmb_widget_style_t* s = &wd->style;
        int x = wd->base.x, y = wd->base.y, w = wd->base.width, h = wd->base.height;
        GMB_GFX_LINE(x, y + h, x + w - 1, y + h + h * wd->nitems - 1, GFX_RGB(s->fill), "BF");  /* sfondo opaco */
        for (int j = 0; j < wd->nitems; j++) {
            int iy = y + h + j * h;
            unsigned int bg = (j == wd->hover) ? s->fill_hover : s->fill;
            GMB_GFX_LINE(x, iy, x + w - 1, iy + h - 1, GFX_RGB(bg), "BF");
            GMB_GFX_DRAW_TEXT(wd->items[j], x + 8, iy + (h - tl) / 2, 0, GFX_RGB(s->text));
            GMB_GFX_LINE(x, iy + h - 1, x + w - 1, iy + h - 1, GFX_RGB(s->border), "");
        }
        GMB_GFX_LINE(x, y + h, x, y + h + h * wd->nitems - 1, GFX_RGB(s->border), "");
        GMB_GFX_LINE(x + w - 1, y + h, x + w - 1, y + h + h * wd->nitems - 1, GFX_RGB(s->border), "");
    }
}

/* Disegna tutti i bottoni del form (no-op in TUI/headless). Salva/ripristina il font
 * se qualche bottone usa un font proprio. */
/* Rect PIXEL effettivo di un controllo del FORM secondo lo spazio coordinate DICHIARATO:
 * 0 = nessuna conversione (default: pixel in grafica), 1 = celle → pixel (USE CELLS).
 * La dimensione della cella viene dal buffer TUI e dalla pagina grafica (font/renderScale),
 * quindi la conversione si ricalcola a ogni frame (nessuna cache da invalidare). */
static void form_control_px_rect(gmb_control_t* c, int* x, int* y, int* w, int* h)
{
    /* Unità LOGICHE (le stesse del disegno gfx: gmb_gfx_* applica la render-scale al suo
     * interno). La cella si ricava dalla griglia del buffer TUI sulla pagina LOGICA: usare
     * il drawable fisico la raddoppiava su Retina (pillola schiacciata, non ovale). */
    int cw = GMB_CELL_PX_W, ch = GMB_CELL_PX_H;
    { extern gmb_clipper_state_t* gmb_clipper_peek_state(void);
      gmb_clipper_state_t* st = gmb_clipper_peek_state();
      if (st && st->buffer) {
          int bw = gmb_screen_width(st->buffer), bh = gmb_screen_height(st->buffer);
          int pw = GMB_GFX_SCREEN_W(), ph = GMB_GFX_SCREEN_H();
          if (bw > 0 && pw > 0) cw = pw / bw;
          if (bh > 0 && ph > 0) ch = ph / bh;
      } }
    if (cw < 1) cw = GMB_CELL_PX_W;
    if (ch < 1) ch = GMB_CELL_PX_H;
    int tr = gmb_control_coord_transform(c);
    if (x) *x = (tr > 0) ? c->x * cw : c->x;
    if (y) *y = (tr > 0) ? c->y * ch : c->y;
    if (w) *w = (tr > 0 && !c->coord_auto_w) ? c->width * cw : c->width;
    if (h) *h = (tr > 0 && !c->coord_auto_h) ? c->height * ch : c->height;
}

static void form_draw(void)
{
    if (!GMB_GFX_SCREEN_ON()) return;
    /* Sfondo del form = colore di background corrente (`COLOR fg,bg` → `g_gfx.bg`).
     * Prima `GMB_GFX_CLS(0)` forzava il NERO, ignorando COLOR/CLS. */
    GMB_GFX_CLS(-1);
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
    const char* op = GMB_GFX_FONT_PATH(); char ops[160]; snprintf(ops, sizeof(ops), "%s", op ? op : "");
    int os = GMB_GFX_FONT_SIZE(); int fc = 0;
    /* Spazio coordinate DICHIARATO (SCREEN ... USE CELLS / metric=1): il FORM disegna in
     * pixel, quindi le coordinate in CELLE si convertono QUI, per il frame, e si ripristinano.
     * Default (metric 0 + SCREEN auto): nessuna conversione. */
    int sw[GMB_FORM_MAX_WIDGETS], sh[GMB_FORM_MAX_WIDGETS], sx[GMB_FORM_MAX_WIDGETS], sy[GMB_FORM_MAX_WIDGETS];
    int sbw[GMB_FORM_MAX_BUTTONS], sbh[GMB_FORM_MAX_BUTTONS], sbx[GMB_FORM_MAX_BUTTONS], sby[GMB_FORM_MAX_BUTTONS];
    for (int i = 0; i < g_nwid; i++) {
        gmb_control_t* c = &g_wid[i].base;
        int nx, ny, nw, nh; form_control_px_rect(c, &nx, &ny, &nw, &nh);
        int conv = gmb_control_coord_transform(c) > 0;
        sx[i] = c->x; sy[i] = c->y; sw[i] = c->width; sh[i] = c->height;
        if (conv) { c->x = nx; c->y = ny; c->width = nw; c->height = nh; }
    }
    for (int i = 0; i < g_nbtn; i++) {
        gmb_control_t* c = &g_btns[i].base;
        int nx, ny, nw, nh; form_control_px_rect(c, &nx, &ny, &nw, &nh);
        int conv = gmb_control_coord_transform(c) > 0;
        sbx[i] = c->x; sby[i] = c->y; sbw[i] = c->width; sbh[i] = c->height;
        if (conv) { c->x = nx; c->y = ny; c->width = nw; c->height = nh; }
    }
    draw_widgets(sc);
    /* Bottoni SOPRA i widget: il pannello-card (draw_widgets) copriva i bottoni
     * aggiunti prima di lui. Le dropdown restano l'overlay più alto. */
    for (int i = 0; i < g_nbtn; i++) { if (!g_btns[i].base.visible) continue; if (g_btns[i].style.font_size > 0) fc = 1; draw_button_body(&g_btns[i], sc); }
    draw_open_dropdowns(sc);
    /* Ripristino le coordinate DICHIARATE (il controllo resta nella sua metrica). */
    for (int i = 0; i < g_nwid; i++) { g_wid[i].base.x = sx[i]; g_wid[i].base.y = sy[i]; g_wid[i].base.width = sw[i]; g_wid[i].base.height = sh[i]; }
    for (int i = 0; i < g_nbtn; i++) { g_btns[i].base.x = sbx[i]; g_btns[i].base.y = sby[i]; g_btns[i].base.width = sbw[i]; g_btns[i].base.height = sbh[i]; }
    if (fc) GMB_GFX_FONT(ops, os);
}

int gmb_form_poll_mouse(int* type, int* x, int* y, int* button, int* wheel)
{
    if (!GMB_UI_MOUSE_POLL(type, x, y, button, wheel)) return 0;
    /* Le coordinate arrivano GIA' LOGICHE di finestra (map_mouse non le rimappa:
     * SDL con ALLOW_HIGHDPI riporta punti-finestra). Nessuna scala qui: dividere
     * per renderScale dimezzava i click su Retina (150,70 -> 75,35). */
    if (*type == GMB_GFX_MOUSE_DOWN) g_last_mouse_event = GMB_EV_MOUSEDOWN;
    else if (*type == GMB_GFX_MOUSE_UP) g_last_mouse_event = GMB_EV_MOUSEUP;
    else if (*type == GMB_GFX_MOUSE_MOVE) g_last_mouse_event = GMB_EV_MOUSEMOVE;
    return 1;
}
void gmb_form_set_event(int ev, gmb_form_cb cb)
{
    if (g_last_added >= 0 && g_last_added < g_nwid && ev > 0 && ev < GMB_EV_COUNT)
        g_wid[g_last_added].base.ev_cb[ev] = cb;
}

/* Nome evento -> indice GMB_EV_* (per ON ... CONTROL emesso per nome). 0 se sconosciuto. */
int gmb_form_ev_index(const char* name)
{
    if (!name || !*name) return 0;
    static const struct { const char* n; int ev; } m[] = {
        {"CLICK",GMB_EV_CLICK},{"DBLCLICK",GMB_EV_DBLCLICK},{"CHANGE",GMB_EV_CHANGE},
        {"GOTFOCUS",GMB_EV_GOTFOCUS},{"LOSTFOCUS",GMB_EV_LOSTFOCUS},
        {"MOUSEDOWN",GMB_EV_MOUSEDOWN},{"MOUSEUP",GMB_EV_MOUSEUP},{"MOUSEMOVE",GMB_EV_MOUSEMOVE},
        {"KEYDOWN",GMB_EV_KEYDOWN},{"KEYPRESS",GMB_EV_KEYPRESS},{"KEYUP",GMB_EV_KEYUP},
        {"DRAGDROP",GMB_EV_DRAGDROP},{"DRAGOVER",GMB_EV_DRAGOVER},{"VALIDATE",GMB_EV_VALIDATE},
        {"SCROLL",GMB_EV_SCROLL},{"DROPDOWN",GMB_EV_DROPDOWN},{"UPCLICK",GMB_EV_UPCLICK},
        {"DOWNCLICK",GMB_EV_DOWNCLICK},{"NODECLICK",GMB_EV_NODECLICK},{"EXPAND",GMB_EV_EXPAND},
        {"COLLAPSE",GMB_EV_COLLAPSE},{"SELCHANGE",GMB_EV_SELCHANGE},{"ROWCOLCHANGE",GMB_EV_ROWCOLCHANGE},{"WHEN",GMB_EV_WHEN}
    };
    for (size_t i = 0; i < sizeof(m)/sizeof(m[0]); i++) if (strcasecmp(name, m[i].n) == 0) return m[i].ev;
    return 0;
}
/* Registra un gestore su un controllo per HANDLE (widget=indice, bottone=1000+indice).
 * cb NULL = rimuove (STOP). Sovrascrive l'inline. */
void gmb_form_set_event_id(int handle, int ev, gmb_form_cb cb)
{
    if (ev <= 0 || ev >= GMB_EV_COUNT) return;
    if (handle >= 1000) { int i = handle - 1000; if (i >= 0 && i < g_nbtn) { g_btns[i].base.ev_cb[ev] = cb; return; } }
    else if (handle >= 0 && handle < g_nwid) { g_wid[handle].base.ev_cb[ev] = cb; return; }
    /* Fallback registro TUI: i controlli gmb_control_t usano lo stesso spazio-handle. */
    { extern void gmb_tui_set_event_id(int, int, gmb_form_cb); gmb_tui_set_event_id(handle, ev, cb); }
}
static void fire_ev(int widx, int ev);

static void set_focus_wid(int i)
{
    if (i == g_focus_wid) return;
    if (g_focus_wid >= 0) { fire_ev(g_focus_wid, GMB_EV_LOSTFOCUS); fire_ev(g_focus_wid, GMB_EV_VALIDATE); }
    g_focus_wid = i;
    if (i >= 0) fire_ev(i, GMB_EV_GOTFOCUS);
}
static int ev_is_mouse(int ev) { return ev==GMB_EV_CLICK||ev==GMB_EV_DBLCLICK||ev==GMB_EV_MOUSEDOWN||ev==GMB_EV_MOUSEUP||ev==GMB_EV_MOUSEMOVE||ev==GMB_EV_DRAGOVER||ev==GMB_EV_DRAGDROP; }
static int ev_is_key(int ev) { return ev==GMB_EV_KEYDOWN||ev==GMB_EV_KEYPRESS||ev==GMB_EV_KEYUP; }
static int handle_last_event(int handle)
{
    if (handle >= 1000) { int i = handle - 1000; return (i >= 0 && i < g_nbtn) ? g_btns[i].base.last_event : 0; }
    return (handle >= 0 && handle < g_nwid) ? g_wid[handle].base.last_event : 0;
}
static void fire_ev(int widx, int ev)
{
    if (g_evq_head != (g_evq_tail + 1) % GMB_FORM_EVQ) { g_evq_ev[g_evq_tail] = ev; g_evq_wid[g_evq_tail] = widx; g_evq_tail = (g_evq_tail + 1) % GMB_FORM_EVQ; }
    if (ev_is_mouse(ev)) g_last_mouse_event = ev; else if (ev == GMB_EV_KEYPRESS) g_last_key_event = ev;   /* KEYPRESSED: sticky sul KEYPRESS (UP non lo azzera) */
    if (widx >= 0 && widx < g_nwid && ev > 0) g_wid[widx].base.last_event = ev;
    if (widx >= 0 && widx < g_nwid && ev > 0 && ev < GMB_EV_COUNT && g_wid[widx].base.ev_cb[ev]) {
        g_firing_control = widx; g_firing_event = ev;
        g_wid[widx].base.ev_cb[ev]();
        g_firing_control = -1; g_firing_event = 0;
    }
}
/* ---- Polling di stato (NON consuma): CLICKED/CHANGED/... + globali ---- */
extern double gmb_tui_clicked(int);
extern double gmb_tui_changed(int);
extern double gmb_tui_gotfocused(int);
extern double gmb_tui_lostfocused(int);
double gmb_form_clicked(int handle)     { return (handle_last_event(handle) == GMB_EV_CLICK || gmb_tui_clicked(handle) != 0.0) ? 1.0 : 0.0; }
double gmb_form_dblclicked(int handle)  { return handle_last_event(handle) == GMB_EV_DBLCLICK ? 1.0 : 0.0; }
double gmb_form_changed(int handle)     { return (handle_last_event(handle) == GMB_EV_CHANGE || gmb_tui_changed(handle) != 0.0) ? 1.0 : 0.0; }
double gmb_form_gotfocused(int handle)  { return (handle_last_event(handle) == GMB_EV_GOTFOCUS || gmb_tui_gotfocused(handle) != 0.0) ? 1.0 : 0.0; }
double gmb_form_lostfocused(int handle) { return (handle_last_event(handle) == GMB_EV_LOSTFOCUS || gmb_tui_lostfocused(handle) != 0.0) ? 1.0 : 0.0; }
double gmb_mouse_clicked(void)          { return g_last_mouse_event == GMB_EV_CLICK ? 1.0 : 0.0; }double gmb_mouse_dblclicked(void)       { return g_last_mouse_event == GMB_EV_DBLCLICK ? 1.0 : 0.0; }
double gmb_mouse_pressed(void)          { return g_last_mouse_event == GMB_EV_MOUSEDOWN ? 1.0 : 0.0; }
double gmb_key_pressed(void)            { return g_last_key_event == GMB_EV_KEYPRESS ? 1.0 : 0.0; }
/* Contesto "evento in corso" (letto dal thunk managed durante la callback). */
int gmb_form_firing_control(void) { return g_firing_control; }
int gmb_form_firing_event(void)   { return g_firing_event; }
/* Innalza programmaticamente un evento su un controllo per handle (test/bridge managed).
 * Aggiorna last_event/globale e invoca la callback registrata (se presente). */
void gmb_form_set_last_event(int handle, int ev)
{
    if (ev <= 0 || ev >= GMB_EV_COUNT) return;
    if (handle >= 1000) { int i = handle - 1000; if (i >= 0 && i < g_nbtn) g_btns[i].base.last_event = ev; }
    else if (handle >= 0 && handle < g_nwid) g_wid[handle].base.last_event = ev;
    else return;
    if (ev_is_mouse(ev)) g_last_mouse_event = ev; else if (ev == GMB_EV_KEYPRESS) g_last_key_event = ev;
}
void gmb_form_fire_event(int handle, int ev)
{
    if (ev <= 0 || ev >= GMB_EV_COUNT) return;
    gmb_form_cb cb = NULL;
    if (handle >= 1000) { int i = handle - 1000; if (i >= 0 && i < g_nbtn) { g_btns[i].base.last_event = ev; cb = g_btns[i].base.ev_cb[ev] ? g_btns[i].base.ev_cb[ev] : (ev == GMB_EV_CLICK ? g_btns[i].cb : NULL); } }
    else if (handle >= 0 && handle < g_nwid) { g_wid[handle].base.last_event = ev; cb = g_wid[handle].base.ev_cb[ev]; }
    if (ev_is_mouse(ev)) g_last_mouse_event = ev; else if (ev == GMB_EV_KEYPRESS) g_last_key_event = ev;
    if (cb) { g_firing_control = handle; g_firing_event = ev; cb(); g_firing_control = -1; g_firing_event = 0; }
    else { extern void gmb_tui_fire(int, int); gmb_tui_fire(handle, ev); }   /* fallback registro TUI */
}
/* Proprieta' base per handle quando il controllo vive nel registry FORM (pixel):
 * usata come fallback da gmb_control_apply_props_handle (che cerca nel registry TUI). */
void gmb_form_apply_props_handle(int handle, unsigned flags, const char* name, const char* tag, const char* tooltip,
                                 int tabindex, int visible, int enabled, int tabstop)
{    for (int i = 0; i < g_nwid; i++) {
        gmb_control_t* c = &g_wid[i].base;
        if (c->handle != handle) continue;
        if (flags & 1u)  { snprintf(c->name, sizeof(c->name), "%s", name ? name : ""); snprintf(g_wid[i].name, sizeof(g_wid[i].name), "%s", name ? name : ""); }
        if (flags & 2u)  { snprintf(c->tag, sizeof(c->tag), "%s", tag ? tag : ""); snprintf(g_wid[i].tag, sizeof(g_wid[i].tag), "%s", tag ? tag : ""); }
        if (flags & 4u)  { snprintf(c->tooltip, sizeof(c->tooltip), "%s", tooltip ? tooltip : ""); snprintf(g_wid[i].tooltip, sizeof(g_wid[i].tooltip), "%s", tooltip ? tooltip : ""); }
        if (flags & 8u)  c->tab_index = tabindex;
        if (flags & 16u) c->visible = visible ? true : false;
        if (flags & 32u) c->enabled = enabled ? true : false;
        if (flags & 64u) c->tab_stop = tabstop ? true : false;
        if (flags & 128u) c->focused = true;
        g_dirty = 1;
        return;
    }
    for (int i = 0; i < g_nbtn; i++) {
        if (g_btns[i].base.handle != handle) continue;
        if (flags & 1u)  { snprintf(g_btns[i].base.name, sizeof(g_btns[i].base.name), "%s", name ? name : ""); snprintf(g_btns[i].name, sizeof(g_btns[i].name), "%s", name ? name : ""); }
        if (flags & 2u)  { snprintf(g_btns[i].base.tag, sizeof(g_btns[i].base.tag), "%s", tag ? tag : ""); snprintf(g_btns[i].tag, sizeof(g_btns[i].tag), "%s", tag ? tag : ""); }
        if (flags & 4u)  { snprintf(g_btns[i].base.tooltip, sizeof(g_btns[i].base.tooltip), "%s", tooltip ? tooltip : ""); snprintf(g_btns[i].tooltip, sizeof(g_btns[i].tooltip), "%s", tooltip ? tooltip : ""); }
        if (flags & 8u)  g_btns[i].base.tab_index = tabindex;
        if (flags & 16u) g_btns[i].base.visible = visible ? true : false;
        if (flags & 32u) g_btns[i].base.enabled = enabled ? true : false;
        if (flags & 64u) g_btns[i].base.tab_stop = tabstop ? true : false;
        if (flags & 128u) g_btns[i].base.focused = true;
        g_dirty = 1;
        return;
    }
}

static void apply_base_props_btn(gmb_form_btn_t* b, const char* spec);

/* Nota l'ultimo controllo creato (usato dagli emitter per applicare le clausole per handle).
 * Chiamato anche dal registry TUI: una sola nozione di "ultimo controllo" per entrambi. */
void gmb_form_note_handle(int handle) { g_last_handle = handle; g_last_added = -1; g_last_is_tui = 1; }

/* Applica le clausole dichiarate (NAME/STYLE/metric/…) all'ULTIMO controllo creato,
 * nel registry giusto: form (widget/button) o TUI. Gli handle dei due registry si
 * sovrappongono (0..31 / 0..63), quindi il registry NON si può dedurre dall'handle. */
void gmb_form_apply_last_style(const char* spec)
{
    if (!spec) return;
    if (g_last_is_tui) {
        extern void gmb_control_apply_style_handle(int, const char*);
        gmb_control_apply_style_handle(g_last_handle, spec);
        return;
    }
    for (int i = 0; i < g_nwid; i++) if (g_wid[i].base.handle == g_last_handle) {
        style_apply(&g_wid[i].style, spec); apply_base_props(&g_wid[i], spec, i); g_dirty = 1; return;
    }
    for (int i = 0; i < g_nbtn; i++) if (g_btns[i].base.handle == g_last_handle) {
        style_apply(&g_btns[i].style, spec); apply_base_props_btn(&g_btns[i], spec); g_dirty = 1; return;
    }
    { extern void gmb_control_apply_style_handle(int, const char*);
      gmb_control_apply_style_handle(g_last_handle, spec); }
}

void gmb_form_apply_style_handle(int handle, const char* spec)
{
    /* Form widget/button per handle: stile + proprietà base (NAME/TAG/…/metric). Poi TUI. */
    for (int i = 0; i < g_nwid; i++) if (g_wid[i].base.handle == handle) {
        style_apply(&g_wid[i].style, spec); apply_base_props(&g_wid[i], spec, i); g_dirty = 1; return;
    }
    for (int i = 0; i < g_nbtn; i++) if (g_btns[i].base.handle == handle) {
        style_apply(&g_btns[i].style, spec); apply_base_props_btn(&g_btns[i], spec); g_dirty = 1; return;
    }
    { extern void gmb_control_apply_style_handle(int, const char*);
      gmb_control_apply_style_handle(handle, spec); }
}

void gmb_form_set_last_style(const char* spec)
{
    /* TUI (schermo grafico OFF): il controllo vive nel registry TUI, non nei form:
     * l'"ultimo" è identificato dall'HANDLE (che parte da 0). Senza questo ramo la
     * spec (colori, proprietà base, `metric`) andava persa in modalità testo. */
    if (!GMB_GFX_SCREEN_ON()) {
        if (g_last_handle >= 0) {
            extern void gmb_control_apply_style_handle(int handle, const char* spec);
            gmb_control_apply_style_handle(g_last_handle, spec);
        }
        return;
    }
    /* Ultimo controllo puo essere un WIDGET (0..g_nwid-1) o un BUTTON (handle 1000+i):
     * g_last_added indicizza solo i widget, quindi per i bottoni si usa g_last_handle. */
    if (g_last_handle >= 1000) {
        int bi = g_last_handle - 1000;
        if (bi >= 0 && bi < g_nbtn) { style_apply(&g_btns[bi].style, spec); apply_base_props_btn(&g_btns[bi], spec); g_dirty = 1; }
        return;
    }
    if (g_last_added >= 0 && g_last_added < g_nwid) { style_apply(&g_wid[g_last_added].style, spec); apply_base_props(&g_wid[g_last_added], spec, g_last_added); g_dirty = 1; }
}

/* Proprieta' base VB (Name/Tag/ToolTip/Visible/Enabled/TabIndex/TabStop/Focus) passate
 * nella STESSA spec CSS-like dello stile: "name=OkButton visible=1 tabindex=2 focus". */
static void apply_base_props(gmb_pix_widget_t* wd, const char* spec, int idx)
{
    if (!wd || !spec) return;
    static const char* keys[] = { "name", "tag", "tooltip", "visible", "enabled", "tabindex", "tabstop", "metric", "font", NULL };
    for (int k = 0; keys[k]; k++) {
        char pat[16]; snprintf(pat, sizeof(pat), "%s=", keys[k]); size_t pl = strlen(pat);
        const char* p = spec;
        while ((p = form_stristr(p, pat))) {
            if (p != spec && p[-1] != ' ') { p += pl; continue; }
            const char* v = p + pl; int q = (*v == '"'); if (q) v++;
            char tmp[80]; size_t n;
            if (q) { const char* e = strchr(v, '"'); n = e ? (size_t)(e - v) : strlen(v); }
            else { n = 0; while (v[n] && v[n] != ' ') n++; }
            if (n > 79) n = 79; memcpy(tmp, v, n); tmp[n] = 0;
            if      (!strcasecmp(keys[k], "name"))    snprintf(wd->name, sizeof(wd->name), "%s", tmp);
            else if (!strcasecmp(keys[k], "tag"))     snprintf(wd->tag, sizeof(wd->tag), "%s", tmp);
            else if (!strcasecmp(keys[k], "tooltip")) snprintf(wd->tooltip, sizeof(wd->tooltip), "%s", tmp);
            else if (!strcasecmp(keys[k], "visible")) wd->base.visible = atoi(tmp);
            else if (!strcasecmp(keys[k], "enabled")) wd->base.enabled = atoi(tmp);
            else if (!strcasecmp(keys[k], "tabindex")) wd->base.tab_index = atoi(tmp);
            else if (!strcasecmp(keys[k], "tabstop")) wd->base.tab_stop = atoi(tmp);
            else if (!strcasecmp(keys[k], "metric"))  wd->base.coord_mode = atoi(tmp);
            else if (!strcasecmp(keys[k], "font"))    wd->base.font_size = atoi(tmp);
            break;
        }
    }
    for (const char* f = spec; (f = strstr(f, "focus")); f += 5)
        if ((f == spec || f[-1] == ' ') && (f[5] == 0 || f[5] == ' ')) { set_focus_wid(idx); break; }
}

static void apply_base_props_btn(gmb_form_btn_t* b, const char* spec)
{
    if (!b || !spec) return;
    static const char* keys[] = { "name", "tag", "tooltip", "visible", "enabled", "tabindex", "tabstop", "metric", "font", NULL };
    for (int k = 0; keys[k]; k++) {
        char pat[16]; snprintf(pat, sizeof(pat), "%s=", keys[k]); size_t pl = strlen(pat);
        const char* p = spec;
        while ((p = form_stristr(p, pat))) {
            if (p != spec && p[-1] != ' ') { p += pl; continue; }
            const char* v = p + pl; int q = (*v == '"'); if (q) v++;
            char tmp[80]; size_t n;
            if (q) { const char* e = strchr(v, '"'); n = e ? (size_t)(e - v) : strlen(v); }
            else { n = 0; while (v[n] && v[n] != ' ') n++; }
            if (n > 79) n = 79; memcpy(tmp, v, n); tmp[n] = 0;
            if      (!strcasecmp(keys[k], "name"))    snprintf(b->name, sizeof(b->name), "%s", tmp);
            else if (!strcasecmp(keys[k], "tag"))     snprintf(b->tag, sizeof(b->tag), "%s", tmp);
            else if (!strcasecmp(keys[k], "tooltip")) snprintf(b->tooltip, sizeof(b->tooltip), "%s", tmp);
            else if (!strcasecmp(keys[k], "visible")) b->base.visible = atoi(tmp);
            else if (!strcasecmp(keys[k], "enabled")) b->base.enabled = atoi(tmp);
            else if (!strcasecmp(keys[k], "tabindex")) b->base.tab_index = atoi(tmp);
            else if (!strcasecmp(keys[k], "tabstop")) b->base.tab_stop = atoi(tmp);
            else if (!strcasecmp(keys[k], "metric"))  b->base.coord_mode = atoi(tmp);
            else if (!strcasecmp(keys[k], "font"))    b->base.font_size = atoi(tmp);
            break;
        }
    }
}
const char* gmb_form_button_name(int i)    { return (i >= 0 && i < g_nbtn) ? g_btns[i].name : ""; }
const char* gmb_form_button_tooltip(int i) { return (i >= 0 && i < g_nbtn) ? g_btns[i].tooltip : ""; }
int gmb_form_button_visible(int i)         { return (i >= 0 && i < g_nbtn) ? g_btns[i].base.visible : 0; }
int gmb_form_button_enabled(int i)         { return (i >= 0 && i < g_nbtn) ? g_btns[i].base.enabled : 0; }
int gmb_form_button_tabindex(int i)        { return (i >= 0 && i < g_nbtn) ? g_btns[i].base.tab_index : 0; }

/* ---- Metodi a RUNTIME per nome (VB): SET/GET/MOVE/FOCUS/ZORDER ---- */
static int ctrl_find(const char* name, int* isbtn)
{
    if (!name || !*name) return -1;
    for (int i = 0; i < g_nwid; i++) if (strcmp(g_wid[i].name, name) == 0) { *isbtn = 0; return i; }
    for (int i = 0; i < g_nbtn; i++) if (strcmp(g_btns[i].name, name) == 0) { *isbtn = 1; return i; }
    return -1;
}
int gmb_form_control_find(const char* name) { int b; int i = ctrl_find(name, &b); return i < 0 ? -1 : (b ? 1000 + i : i); }

/* ---- Accessor generici sui controlli TUI (CONTROL SET/GET per nome) ---- */
extern gmb_control_t* gmb_tui_control_find_by_name(const char* name);
static double tui_ctrl_value(gmb_control_t* c)
{
    void* u = c->user;
    switch (c->ctrl_kind) {
        case GMB_CK_CHECKBOX:    return ((gmb_checkbox_t*)u)->checked ? 1.0 : 0.0;
        case GMB_CK_TOGGLE:      return ((gmb_toggle_button_t*)u)->pressed ? 1.0 : 0.0;
        case GMB_CK_RADIO:       return (double)((gmb_radio_group_t*)u)->selected;
        case GMB_CK_SLIDER:      return (double)((gmb_slider_t*)u)->value;
        case GMB_CK_COMBOBOX:    return (double)((gmb_combobox_t*)u)->selected;
        case GMB_CK_LISTBOX:     return (double)((gmb_listbox_t*)u)->selected;
        case GMB_CK_SPINNER:     return (double)((gmb_spinner_t*)u)->index;
        case GMB_CK_TAB:         return (double)((gmb_tab_t*)u)->selected;
        case GMB_CK_GRID:        return (double)((gmb_grid_t*)u)->sel_row;
        case GMB_CK_TREE:        return (double)((gmb_tree_view_t*)u)->selected_index;
        case GMB_CK_COLORPICKER: return (double)((gmb_color_picker_t*)u)->selected_index;
        case GMB_CK_NUMERICUPDOWN: return (double)((gmb_numeric_updown_t*)u)->value;
        case GMB_CK_STARRATING:    return (double)((gmb_star_rating_t*)u)->value;
        case GMB_CK_SCROLLBAR:     return (double)((gmb_scrollbar_t*)u)->value;
        case GMB_CK_SCROLLVIEW:    return (double)((gmb_scroll_view_t*)u)->offset;
        case GMB_CK_DATALIST:      return (double)((gmb_data_list_t*)u)->selected_index;
        case GMB_CK_REORDERLISTBOX:return (double)((gmb_reorder_listbox_t*)u)->selected;
        case GMB_CK_HEXVIEW:       return (double)((gmb_hex_view_t*)u)->offset;
        case GMB_CK_LOGVIEW:       return (double)((gmb_log_view_t*)u)->offset;
        case GMB_CK_DASHBOARD:     return (double)((gmb_dashboard_t*)u)->selected_index;
        case GMB_CK_PROPERTYGRID:  return (double)((gmb_property_grid_t*)u)->selected_index;
        default: return 0.0;
    }
}
static void tui_ctrl_set_value(gmb_control_t* c, double v)
{
    void* u = c->user;
    switch (c->ctrl_kind) {
        case GMB_CK_CHECKBOX:    ((gmb_checkbox_t*)u)->checked = (v != 0.0); break;
        case GMB_CK_TOGGLE:      ((gmb_toggle_button_t*)u)->pressed = (v != 0.0); break;
        case GMB_CK_RADIO:       ((gmb_radio_group_t*)u)->selected = (int)v; break;
        case GMB_CK_SLIDER:      ((gmb_slider_t*)u)->value = (int)v; break;
        case GMB_CK_COMBOBOX:    ((gmb_combobox_t*)u)->selected = (int)v; break;
        case GMB_CK_LISTBOX:     ((gmb_listbox_t*)u)->selected = (int)v; break;
        case GMB_CK_SPINNER:     ((gmb_spinner_t*)u)->index = (int)v; break;
        case GMB_CK_TAB:         ((gmb_tab_t*)u)->selected = (int)v; break;
        case GMB_CK_GRID:        ((gmb_grid_t*)u)->sel_row = (int)v; break;
        case GMB_CK_TREE:        ((gmb_tree_view_t*)u)->selected_index = (int)v; break;
        case GMB_CK_COLORPICKER: ((gmb_color_picker_t*)u)->selected_index = (int)v; break;
        case GMB_CK_NUMERICUPDOWN: ((gmb_numeric_updown_t*)u)->value = (int)v; break;
        case GMB_CK_STARRATING:    ((gmb_star_rating_t*)u)->value = (int)v; break;
        case GMB_CK_SCROLLBAR:     ((gmb_scrollbar_t*)u)->value = (int)v; break;
        case GMB_CK_SCROLLVIEW:    ((gmb_scroll_view_t*)u)->offset = (int)v; break;
        case GMB_CK_DATALIST:      ((gmb_data_list_t*)u)->selected_index = (int)v; break;
        case GMB_CK_REORDERLISTBOX:((gmb_reorder_listbox_t*)u)->selected = (int)v; break;
        case GMB_CK_HEXVIEW:       ((gmb_hex_view_t*)u)->offset = (int)v; break;
        case GMB_CK_LOGVIEW:       ((gmb_log_view_t*)u)->offset = (int)v; break;
        case GMB_CK_DASHBOARD:     ((gmb_dashboard_t*)u)->selected_index = (int)v; break;
        case GMB_CK_PROPERTYGRID:  ((gmb_property_grid_t*)u)->selected_index = (int)v; break;
        default: break;
    }
}
static const char* tui_ctrl_text(gmb_control_t* c)
{
    static char _buf[256]; void* u = c->user; _buf[0] = 0;
    switch (c->ctrl_kind) {
        case GMB_CK_CHECKBOX: snprintf(_buf, sizeof(_buf), "%s", ((gmb_checkbox_t*)u)->label); break;
        case GMB_CK_TOGGLE:   snprintf(_buf, sizeof(_buf), "%s", ((gmb_toggle_button_t*)u)->caption); break;
        case GMB_CK_STATUSBAR:snprintf(_buf, sizeof(_buf), "%s", ((gmb_status_bar_t*)u)->text); break;
        case GMB_CK_LABEL:    snprintf(_buf, sizeof(_buf), "%s", ((gmb_label_t*)u)->text); break;
        case GMB_CK_BIGTEXT:  snprintf(_buf, sizeof(_buf), "%s", ((gmb_big_text_t*)u)->text); break;
        default: return "";
    }
    return _buf;
}
static void tui_ctrl_set_text(gmb_control_t* c, const char* str)
{
    void* u = c->user;
    switch (c->ctrl_kind) {
        case GMB_CK_CHECKBOX: snprintf(((gmb_checkbox_t*)u)->label, sizeof(((gmb_checkbox_t*)u)->label), "%s", str); break;
        case GMB_CK_TOGGLE:   snprintf(((gmb_toggle_button_t*)u)->caption, sizeof(((gmb_toggle_button_t*)u)->caption), "%s", str); break;
        case GMB_CK_STATUSBAR:snprintf(((gmb_status_bar_t*)u)->text, sizeof(((gmb_status_bar_t*)u)->text), "%s", str); break;
        case GMB_CK_LABEL:    snprintf(((gmb_label_t*)u)->text, sizeof(((gmb_label_t*)u)->text), "%s", str); break;
        case GMB_CK_BIGTEXT:  snprintf(((gmb_big_text_t*)u)->text, sizeof(((gmb_big_text_t*)u)->text), "%s", str); break;
        case GMB_CK_SPINNER:  if (str) gmb_spinner_set_preset((gmb_spinner_t*)u, str); break;
        default: break;
    }
}
static void control_extra_set(gmb_control_t* c, const char* key, double num, const char* str)
{
    if (!c || !key || !*key) return;
    for (int i = 0; i < c->extra_count; i++) {
        if (strcasecmp(c->extra_keys[i], key) == 0) {
            if (str && *str) snprintf(c->extra_values[i], sizeof(c->extra_values[i]), "%s", str);
            else snprintf(c->extra_values[i], sizeof(c->extra_values[i]), "%g", num);
            return;
        }
    }
    if (c->extra_count >= (int)(sizeof(c->extra_keys) / sizeof(c->extra_keys[0]))) return;
    snprintf(c->extra_keys[c->extra_count], sizeof(c->extra_keys[c->extra_count]), "%s", key);
    if (str && *str) snprintf(c->extra_values[c->extra_count], sizeof(c->extra_values[c->extra_count]), "%s", str);
    else snprintf(c->extra_values[c->extra_count], sizeof(c->extra_values[c->extra_count]), "%g", num);
    c->extra_count++;
}

static const char* control_extra_get(gmb_control_t* c, const char* key)
{
    if (!c || !key) return NULL;
    for (int i = 0; i < c->extra_count; i++)
        if (strcasecmp(c->extra_keys[i], key) == 0) return c->extra_values[i];
    return NULL;
}

static void tui_ctrl_set(const char* name, const char* prop, double num, const char* str)
{
    gmb_control_t* c = gmb_tui_control_find_by_name(name); if (!c || !prop) return;    if (!strcasecmp(prop,"left")||!strcasecmp(prop,"x")) c->x=(int)num;
    else if (!strcasecmp(prop,"top")||!strcasecmp(prop,"y")) c->y=(int)num;
    else if (!strcasecmp(prop,"width")||!strcasecmp(prop,"w")) c->width=(int)num;
    else if (!strcasecmp(prop,"height")||!strcasecmp(prop,"h")) c->height=(int)num;
    else if (!strcasecmp(prop,"visible")) c->visible=(num!=0);
    else if (!strcasecmp(prop,"enabled")) c->enabled=(num!=0);
    else if (!strcasecmp(prop,"tabindex")) c->tab_index=(int)num;
    else if (!strcasecmp(prop,"tabstop")) c->tab_stop=(num!=0);
    /* Spazio coordinate DICHIARATO (0=default/segue SCREEN, 1=CellsToPixel, 2=PixelToCells). */
    else if (!strcasecmp(prop,"metric")||!strcasecmp(prop,"coordmode")) c->coord_mode=(int)num;
    else if (!strcasecmp(prop,"value")||!strcasecmp(prop,"selected")||!strcasecmp(prop,"index")||!strcasecmp(prop,"listindex")) tui_ctrl_set_value(c,num);
    else if (!strcasecmp(prop,"items")||!strcasecmp(prop,"list")) {
        if (str && c->ctrl_kind==GMB_CK_LISTBOX) {
            char buf[2048], *save=NULL; char* arr[128]; int n=0;
            snprintf(buf,sizeof(buf),"%s",str);
            for (char* tk=strtok_r(buf,"|",&save); tk && n<128; tk=strtok_r(NULL,"|",&save)) arr[n++]=tk;
#ifdef GMB_WITH_UI
            gmb_listbox_set_items((gmb_listbox_t*)c->user,(const char* const*)arr,n);
#else
            gmb_ui_missing_warn();
#endif
        } else if (str && c->ctrl_kind==GMB_CK_COMBOBOX) {
            char buf[2048], *save=NULL; char* arr[128]; int n=0;
            snprintf(buf,sizeof(buf),"%s",str);
            for (char* tk=strtok_r(buf,"|",&save); tk && n<128; tk=strtok_r(NULL,"|",&save)) arr[n++]=tk;
#ifdef GMB_WITH_UI
            gmb_combobox_set_items((gmb_combobox_t*)c->user,(const char* const*)arr,n);
#else
            gmb_ui_missing_warn();
#endif
        }
    }
    else if (!strcasecmp(prop,"caption")||!strcasecmp(prop,"text")||!strcasecmp(prop,"label")) { if (str) tui_ctrl_set_text(c,str); }
    else if (!strcasecmp(prop,"tooltip")) { if (str) snprintf(c->tooltip,sizeof(c->tooltip),"%s",str); }
    else if (!strcasecmp(prop,"tag")) { if (str) snprintf(c->tag,sizeof(c->tag),"%s",str); }
    else if (!strcasecmp(prop,"name")) { if (str) snprintf(c->name,sizeof(c->name),"%s",str); }
    else if (!strcasecmp(prop,"uppercase")) c->uppercase=(num!=0);
    else if (!strcasecmp(prop,"haserror")||!strcasecmp(prop,"error")) c->has_error=(num!=0);
    else if (!strcasecmp(prop,"color")||!strcasecmp(prop,"colors")) { if (str) { char b[160]; snprintf(b,sizeof(b),"color=%s",str); gmb_control_apply_style(c,b); } }
    else if (!strcasecmp(prop,"focuscolor")) { if (str) { char b[160]; snprintf(b,sizeof(b),"focuscolor=%s",str); gmb_control_apply_style(c,b); } }
    else if (!strcasecmp(prop,"selcolor")||!strcasecmp(prop,"selectcolor")) { if (str) { char b[160]; snprintf(b,sizeof(b),"selcolor=%s",str); gmb_control_apply_style(c,b); } }
    else if (!strcasecmp(prop,"errorcolor")) { if (str) { char b[160]; snprintf(b,sizeof(b),"errorcolor=%s",str); gmb_control_apply_style(c,b); } }
    else if (!strcasecmp(prop,"forecolor")||!strcasecmp(prop,"fg")||!strcasecmp(prop,"foreground")) { if (str) { char b[160]; snprintf(b,sizeof(b),"fg=%s",str); gmb_control_apply_style(c,b); } }
    else if (!strcasecmp(prop,"backcolor")||!strcasecmp(prop,"bg")||!strcasecmp(prop,"background")) { if (str) { char b[160]; snprintf(b,sizeof(b),"bg=%s",str); gmb_control_apply_style(c,b); } }
    else if (!strcasecmp(prop,"min")||!strcasecmp(prop,"minvalue")) { if (c->ctrl_kind==GMB_CK_SLIDER) ((gmb_slider_t*)c->user)->min=(int)num; else if (c->ctrl_kind==GMB_CK_NUMERICUPDOWN) ((gmb_numeric_updown_t*)c->user)->min=(int)num; else if (c->ctrl_kind==GMB_CK_SCROLLBAR) ((gmb_scrollbar_t*)c->user)->min=(int)num; }
    else if (!strcasecmp(prop,"max")||!strcasecmp(prop,"maxvalue")) { if (c->ctrl_kind==GMB_CK_SLIDER) ((gmb_slider_t*)c->user)->max=(int)num; else if (c->ctrl_kind==GMB_CK_NUMERICUPDOWN) ((gmb_numeric_updown_t*)c->user)->max=(int)num; else if (c->ctrl_kind==GMB_CK_STARRATING) ((gmb_star_rating_t*)c->user)->max=(int)num; else if (c->ctrl_kind==GMB_CK_SCROLLBAR) ((gmb_scrollbar_t*)c->user)->max=(int)num; }
    else if (!strcasecmp(prop,"step")) { if (c->ctrl_kind==GMB_CK_SLIDER) ((gmb_slider_t*)c->user)->step=(int)num; else if (c->ctrl_kind==GMB_CK_NUMERICUPDOWN) ((gmb_numeric_updown_t*)c->user)->step=(int)num; }
    else if (!strcasecmp(prop,"toprow")) { if (c->ctrl_kind==GMB_CK_LISTBOX) ((gmb_listbox_t*)c->user)->scroll_offset=(int)num; else if (c->ctrl_kind==GMB_CK_REORDERLISTBOX) ((gmb_reorder_listbox_t*)c->user)->top=(int)num; else if (c->ctrl_kind==GMB_CK_TBROWSE) ((gmb_browse_t*)c->user)->top_row=(int)num; }
    else if (!strcasecmp(prop,"multi")||!strcasecmp(prop,"multiselect")) { if (c->ctrl_kind==GMB_CK_DATALIST) ((gmb_data_list_t*)c->user)->multi_select=(num!=0); else if (c->ctrl_kind==GMB_CK_REORDERLISTBOX) ((gmb_reorder_listbox_t*)c->user)->multi_select=(num!=0); }
    else if (!strcasecmp(prop,"searchable")) { if (c->ctrl_kind==GMB_CK_DATALIST) ((gmb_data_list_t*)c->user)->searchable=(num!=0); }
    else if (!strcasecmp(prop,"scrolloffset")) { if (c->ctrl_kind==GMB_CK_DATALIST) ((gmb_data_list_t*)c->user)->scroll_offset=(int)num; else if (c->ctrl_kind==GMB_CK_PROPERTYGRID) ((gmb_property_grid_t*)c->user)->scroll_offset=(int)num; else if (c->ctrl_kind==GMB_CK_TREE) ((gmb_tree_view_t*)c->user)->scroll_offset=(int)num; }
    else if (!strcasecmp(prop,"open")) { if (c->ctrl_kind==GMB_CK_MENU) ((gmb_menu_t*)c->user)->is_open=(num!=0); else if (c->ctrl_kind==GMB_CK_COMBOBOX) ((gmb_combobox_t*)c->user)->open=(num!=0); else if (c->ctrl_kind==GMB_CK_DATEPICKER) ((gmb_date_picker_t*)c->user)->open=(num!=0); }
    else if (!strcasecmp(prop,"subselected")||!strcasecmp(prop,"subselectedindex")) { if (c->ctrl_kind==GMB_CK_MENU) ((gmb_menu_t*)c->user)->sub_selected_index=(int)num; }
    else if (!strcasecmp(prop,"columns")) { if (c->ctrl_kind==GMB_CK_COLORPICKER) ((gmb_color_picker_t*)c->user)->columns=(int)num; }
    else if (!strcasecmp(prop,"highlight")) { if (c->ctrl_kind==GMB_CK_COMBOBOX) ((gmb_combobox_t*)c->user)->highlight=(int)num; }
    else if (!strcasecmp(prop,"dropdownheight")) { if (c->ctrl_kind==GMB_CK_COMBOBOX) ((gmb_combobox_t*)c->user)->drop_down_height=(int)num; }
    else if (!strcasecmp(prop,"day")) { if (c->ctrl_kind==GMB_CK_DATEPICKER) ((gmb_date_picker_t*)c->user)->day=(int)num; }
    else if (!strcasecmp(prop,"month")) { if (c->ctrl_kind==GMB_CK_DATEPICKER) ((gmb_date_picker_t*)c->user)->month=(int)num; }
    else if (!strcasecmp(prop,"year")) { if (c->ctrl_kind==GMB_CK_DATEPICKER) ((gmb_date_picker_t*)c->user)->year=(int)num; }
    else if (!strcasecmp(prop,"bytesperrow")) { if (c->ctrl_kind==GMB_CK_HEXVIEW) ((gmb_hex_view_t*)c->user)->bytes_per_row=(int)num; }
    else if (!strcasecmp(prop,"showaddress")) { if (c->ctrl_kind==GMB_CK_HEXVIEW) ((gmb_hex_view_t*)c->user)->show_address=(num!=0); }
    else if (!strcasecmp(prop,"showascii")) { if (c->ctrl_kind==GMB_CK_HEXVIEW) ((gmb_hex_view_t*)c->user)->show_ascii=(num!=0); }
    else if (!strcasecmp(prop,"minlevel")) { if (c->ctrl_kind==GMB_CK_LOGVIEW) ((gmb_log_view_t*)c->user)->min_level=(int)num; }
    else if (!strcasecmp(prop,"capacity")) { if (c->ctrl_kind==GMB_CK_LOGVIEW) ((gmb_log_view_t*)c->user)->capacity=(int)num; }
    else if (!strcasecmp(prop,"barwidth")) { if (c->ctrl_kind==GMB_CK_DASHBOARD) ((gmb_dashboard_t*)c->user)->bar_width=(int)num; }
    else if (!strcasecmp(prop,"ctrlheld")) { if (c->ctrl_kind==GMB_CK_REORDERLISTBOX) ((gmb_reorder_listbox_t*)c->user)->ctrl_held=(num!=0); }
    else if (!strcasecmp(prop,"leftcol")) { if (c->ctrl_kind==GMB_CK_TBROWSE) ((gmb_browse_t*)c->user)->left_col=(int)num; }
    else if (!strcasecmp(prop,"showheader")) { if (c->ctrl_kind==GMB_CK_TBROWSE) ((gmb_browse_t*)c->user)->show_header=(num!=0); }
    else if (!strcasecmp(prop,"autolite")) { if (c->ctrl_kind==GMB_CK_TBROWSE) ((gmb_browse_t*)c->user)->auto_lite=(num!=0); else if (c->ctrl_kind==GMB_CK_GRID) ((gmb_grid_t*)c->user)->auto_lite=(num!=0); }
    /* TBROWSE sort/filter: espone il motore già presente (gmb_browse_sort/set_filter).
       `sort` = colonna (col), `sortdir` = 0/1 asc/desc, `filter` = testo. */
    else if (!strcasecmp(prop,"sort")||!strcasecmp(prop,"sortcol")) { if (c->ctrl_kind==GMB_CK_TBROWSE) { gmb_browse_t* br=(gmb_browse_t*)c->user; gmb_browse_sort(br,(int)num,gmb_browse_sort_dir(br)); } }
    else if (!strcasecmp(prop,"sortdir")||!strcasecmp(prop,"sortdesc")) { if (c->ctrl_kind==GMB_CK_TBROWSE) { gmb_browse_t* br=(gmb_browse_t*)c->user; gmb_browse_sort(br,gmb_browse_sort_col(br),(int)num); } }
    else if (!strcasecmp(prop,"filter")) { if (c->ctrl_kind==GMB_CK_TBROWSE) gmb_browse_set_filter((gmb_browse_t*)c->user, str); }
    else if (!strcasecmp(prop,"sortkeys")||!strcasecmp(prop,"sortmulti")) { if (c->ctrl_kind==GMB_CK_TBROWSE && str) gmb_browse_set_sortkeys((gmb_browse_t*)c->user, str); }
    else if (!strcasecmp(prop,"align")||!strcasecmp(prop,"alignment")) { if (c->ctrl_kind==GMB_CK_LABEL) ((gmb_label_t*)c->user)->align=(int)num; else if (c->ctrl_kind==GMB_CK_GRID) ((gmb_grid_t*)c->user)->align=(int)num; }
    else if (!strcasecmp(prop,"preset")) { if (str && c->ctrl_kind==GMB_CK_SPINNER) gmb_spinner_set_preset((gmb_spinner_t*)c->user, str); }
    else if (!strcasecmp(prop,"format")||!strcasecmp(prop,"picture")) { if (str && c->ctrl_kind==GMB_CK_TEXTBOX) snprintf(((gmb_text_input_t*)c->user)->format, sizeof(((gmb_text_input_t*)c->user)->format), "%s", str); }
    else if (!strcasecmp(prop,"valid")||!strcasecmp(prop,"isvalid")) { if (c->ctrl_kind==GMB_CK_TEXTBOX) ((gmb_text_input_t*)c->user)->is_valid=(num!=0); }
    else if (!strcasecmp(prop,"readonly")||!strcasecmp(prop,"read_only")) { if (c->ctrl_kind==GMB_CK_TEXTBOX) ((gmb_text_input_t*)c->user)->read_only=(num!=0); }
    else if (!strcasecmp(prop,"password")||!strcasecmp(prop,"passwordmode")) { if (c->ctrl_kind==GMB_CK_TEXTBOX) ((gmb_text_input_t*)c->user)->password_mode=(num!=0); }
    else if (!strcasecmp(prop,"maxlength")||!strcasecmp(prop,"max_length")) { if (c->ctrl_kind==GMB_CK_TEXTBOX) ((gmb_text_input_t*)c->user)->max_length=(int)num; }
    else if (!strcasecmp(prop,"cursorrow")) { if (c->ctrl_kind==GMB_CK_GRID) ((gmb_grid_t*)c->user)->cursor_row=(int)num; }
    else if (!strcasecmp(prop,"cursorcol")) { if (c->ctrl_kind==GMB_CK_GRID) ((gmb_grid_t*)c->user)->cursor_col=(int)num; }
    else if (!strcasecmp(prop,"selrow")) { if (c->ctrl_kind==GMB_CK_GRID) { ((gmb_grid_t*)c->user)->sel_row=(int)num; ((gmb_grid_t*)c->user)->has_selected=true; } }
    else if (!strcasecmp(prop,"selcol")) { if (c->ctrl_kind==GMB_CK_GRID) ((gmb_grid_t*)c->user)->sel_col=(int)num; }
    else control_extra_set(c, prop, num, str);
    /* Ogni modifica a un controllo TUI deve riflettersi al frame successivo: il livello
     * pixel è ridisegnato solo su dirty (senza, `visible=0` lasciava i pixel vecchi).
     * g_dirty fa anche ripulire la pagina dal form (altrimenti restano i pixel del controllo). */
    g_dirty = 1;
    { extern void gmb_tui_mark_dirty(void); gmb_tui_mark_dirty(); }
}
static double tui_ctrl_get(const char* name, const char* prop)
{
    gmb_control_t* c = gmb_tui_control_find_by_name(name); if (!c || !prop) return 0;
    if (!strcasecmp(prop,"left")||!strcasecmp(prop,"x")) return c->x;
    if (!strcasecmp(prop,"top")||!strcasecmp(prop,"y")) return c->y;
    if (!strcasecmp(prop,"width")||!strcasecmp(prop,"w")) return c->width;
    if (!strcasecmp(prop,"height")||!strcasecmp(prop,"h")) return c->height;
    if (!strcasecmp(prop,"visible")) return c->visible;
    if (!strcasecmp(prop,"enabled")) return c->enabled;
    if (!strcasecmp(prop,"focused")) return c->focused;
    if (!strcasecmp(prop,"handle")) return (double)c->handle;
    if (!strcasecmp(prop,"tabindex")) return c->tab_index;
    if (!strcasecmp(prop,"tabstop")) return c->tab_stop;
    if (!strcasecmp(prop,"metric")||!strcasecmp(prop,"coordmode")) return c->coord_mode;
    if (!strcasecmp(prop,"value")||!strcasecmp(prop,"selected")||!strcasecmp(prop,"index")||!strcasecmp(prop,"listindex")) return tui_ctrl_value(c);
    if (!strcasecmp(prop,"listcount")) {
        if (c->ctrl_kind==GMB_CK_LISTBOX) return (double)((gmb_listbox_t*)c->user)->items_count;
        if (c->ctrl_kind==GMB_CK_COMBOBOX) return (double)((gmb_combobox_t*)c->user)->items_count;
        return 0;
    }
    if (!strcasecmp(prop,"uppercase")) return c->uppercase;
    if (!strcasecmp(prop,"haserror")||!strcasecmp(prop,"error")) return c->has_error;
    if (!strcasecmp(prop,"min")||!strcasecmp(prop,"minvalue")) { if (c->ctrl_kind==GMB_CK_SLIDER) return ((gmb_slider_t*)c->user)->min; if (c->ctrl_kind==GMB_CK_NUMERICUPDOWN) return ((gmb_numeric_updown_t*)c->user)->min; if (c->ctrl_kind==GMB_CK_SCROLLBAR) return ((gmb_scrollbar_t*)c->user)->min; return 0; }
    if (!strcasecmp(prop,"max")||!strcasecmp(prop,"maxvalue")) { if (c->ctrl_kind==GMB_CK_SLIDER) return ((gmb_slider_t*)c->user)->max; if (c->ctrl_kind==GMB_CK_NUMERICUPDOWN) return ((gmb_numeric_updown_t*)c->user)->max; if (c->ctrl_kind==GMB_CK_STARRATING) return ((gmb_star_rating_t*)c->user)->max; if (c->ctrl_kind==GMB_CK_SCROLLBAR) return ((gmb_scrollbar_t*)c->user)->max; return 0; }
    if (!strcasecmp(prop,"step")) { if (c->ctrl_kind==GMB_CK_SLIDER) return ((gmb_slider_t*)c->user)->step; if (c->ctrl_kind==GMB_CK_NUMERICUPDOWN) return ((gmb_numeric_updown_t*)c->user)->step; return 0; }
    if (!strcasecmp(prop,"toprow")) { if (c->ctrl_kind==GMB_CK_LISTBOX) return ((gmb_listbox_t*)c->user)->scroll_offset; if (c->ctrl_kind==GMB_CK_REORDERLISTBOX) return ((gmb_reorder_listbox_t*)c->user)->top; if (c->ctrl_kind==GMB_CK_TBROWSE) return ((gmb_browse_t*)c->user)->top_row; return 0; }
    if (!strcasecmp(prop,"leftcol")) return c->ctrl_kind==GMB_CK_TBROWSE ? ((gmb_browse_t*)c->user)->left_col : 0;
    if (!strcasecmp(prop,"showheader")) return c->ctrl_kind==GMB_CK_TBROWSE ? ((gmb_browse_t*)c->user)->show_header : 0;
    if (!strcasecmp(prop,"autolite")) { if (c->ctrl_kind==GMB_CK_TBROWSE) return ((gmb_browse_t*)c->user)->auto_lite; if (c->ctrl_kind==GMB_CK_GRID) return ((gmb_grid_t*)c->user)->auto_lite; return 0; }
    if (!strcasecmp(prop,"sort")||!strcasecmp(prop,"sortcol")) return c->ctrl_kind==GMB_CK_TBROWSE ? gmb_browse_sort_col((gmb_browse_t*)c->user) : -1;
    if (!strcasecmp(prop,"sortdir")||!strcasecmp(prop,"sortdesc")) return c->ctrl_kind==GMB_CK_TBROWSE ? gmb_browse_sort_dir((gmb_browse_t*)c->user) : 0;
    if (!strcasecmp(prop,"align")||!strcasecmp(prop,"alignment")) { if (c->ctrl_kind==GMB_CK_LABEL) return ((gmb_label_t*)c->user)->align; if (c->ctrl_kind==GMB_CK_GRID) return ((gmb_grid_t*)c->user)->align; return 0; }
    if (!strcasecmp(prop,"valid")||!strcasecmp(prop,"isvalid")) return c->ctrl_kind==GMB_CK_TEXTBOX ? ((gmb_text_input_t*)c->user)->is_valid : 0;
    if (!strcasecmp(prop,"readonly")||!strcasecmp(prop,"read_only")) return c->ctrl_kind==GMB_CK_TEXTBOX ? ((gmb_text_input_t*)c->user)->read_only : 0;
    if (!strcasecmp(prop,"password")||!strcasecmp(prop,"passwordmode")) return c->ctrl_kind==GMB_CK_TEXTBOX ? ((gmb_text_input_t*)c->user)->password_mode : 0;
    if (!strcasecmp(prop,"maxlength")||!strcasecmp(prop,"max_length")) return c->ctrl_kind==GMB_CK_TEXTBOX ? ((gmb_text_input_t*)c->user)->max_length : 0;
    if (!strcasecmp(prop,"cursorrow")) return c->ctrl_kind==GMB_CK_GRID ? (double)((gmb_grid_t*)c->user)->cursor_row : 0;
    if (!strcasecmp(prop,"cursorcol")) return c->ctrl_kind==GMB_CK_GRID ? (double)((gmb_grid_t*)c->user)->cursor_col : 0;
    if (!strcasecmp(prop,"bytesperrow")) return c->ctrl_kind==GMB_CK_HEXVIEW ? ((gmb_hex_view_t*)c->user)->bytes_per_row : 0;
    if (!strcasecmp(prop,"showaddress")) return c->ctrl_kind==GMB_CK_HEXVIEW ? ((gmb_hex_view_t*)c->user)->show_address : 0;
    if (!strcasecmp(prop,"showascii")) return c->ctrl_kind==GMB_CK_HEXVIEW ? ((gmb_hex_view_t*)c->user)->show_ascii : 0;
    if (!strcasecmp(prop,"minlevel")) return c->ctrl_kind==GMB_CK_LOGVIEW ? ((gmb_log_view_t*)c->user)->min_level : 0;
    if (!strcasecmp(prop,"barwidth")) return c->ctrl_kind==GMB_CK_DASHBOARD ? ((gmb_dashboard_t*)c->user)->bar_width : 0;
    if (!strcasecmp(prop,"columns")) return c->ctrl_kind==GMB_CK_COLORPICKER ? ((gmb_color_picker_t*)c->user)->columns : 0;
    if (!strcasecmp(prop,"highlight")) return c->ctrl_kind==GMB_CK_COMBOBOX ? ((gmb_combobox_t*)c->user)->highlight : 0;
    if (!strcasecmp(prop,"day")) return c->ctrl_kind==GMB_CK_DATEPICKER ? ((gmb_date_picker_t*)c->user)->day : 0;
    if (!strcasecmp(prop,"month")) return c->ctrl_kind==GMB_CK_DATEPICKER ? ((gmb_date_picker_t*)c->user)->month : 0;
    if (!strcasecmp(prop,"year")) return c->ctrl_kind==GMB_CK_DATEPICKER ? ((gmb_date_picker_t*)c->user)->year : 0;
    { const char* extra = control_extra_get(c, prop); if (extra) return atof(extra); }
    return 0;
}
static const char* tui_ctrl_get_str(const char* name, const char* prop)
{
    gmb_control_t* c = gmb_tui_control_find_by_name(name); if (!c || !prop) return "";
    if (!strcasecmp(prop,"caption")||!strcasecmp(prop,"text")||!strcasecmp(prop,"label")) return tui_ctrl_text(c);
    if (!strcasecmp(prop,"tooltip")) return c->tooltip;
    if (!strcasecmp(prop,"tag")) return c->tag;
    if (!strcasecmp(prop,"name")) return c->name;
    if (!strcasecmp(prop,"items")||!strcasecmp(prop,"list")) {
        static char _ib[4096]; _ib[0] = 0; void* u = c->user; int off = 0;
        if (c->ctrl_kind == GMB_CK_LISTBOX) { gmb_listbox_t* l = (gmb_listbox_t*)u;
            for (int i = 0; i < l->items_count; i++) { if (i) off += snprintf(_ib + off, sizeof(_ib) - off, "|"); off += snprintf(_ib + off, sizeof(_ib) - off, "%s", l->items[i]); } return _ib; }
        if (c->ctrl_kind == GMB_CK_COMBOBOX) { gmb_combobox_t* l = (gmb_combobox_t*)u;
            for (int i = 0; i < l->items_count; i++) { if (i) off += snprintf(_ib + off, sizeof(_ib) - off, "|"); off += snprintf(_ib + off, sizeof(_ib) - off, "%s", l->items[i]); } return _ib; }
        if (c->ctrl_kind == GMB_CK_TAB) { gmb_tab_t* l = (gmb_tab_t*)u;
            for (int i = 0; i < l->tabs_count; i++) { if (i) off += snprintf(_ib + off, sizeof(_ib) - off, "|"); off += snprintf(_ib + off, sizeof(_ib) - off, "%s", l->tabs[i]); } return _ib; }
        if (c->ctrl_kind == GMB_CK_GRID) { gmb_grid_t* l = (gmb_grid_t*)u;
            for (int i = 0; i < l->cells_count; i++) { if (i) off += snprintf(_ib + off, sizeof(_ib) - off, "|"); off += snprintf(_ib + off, sizeof(_ib) - off, "%s", l->cells[i]); } return _ib; }
        return "";
    }
    if (c->ctrl_kind == GMB_CK_SPINNER && !strcasecmp(prop,"preset")) { gmb_spinner_t* s = (gmb_spinner_t*)c->user; static char _sp[256]; int off=0; _sp[0]=0; for (int i=0;i<s->frames_count && off<(int)sizeof _sp-1;i++){ off += snprintf(_sp+off, sizeof _sp - off, "%s%s", i?"|":"", s->frames[i]); } return _sp; }
    if ((!strcasecmp(prop,"format")||!strcasecmp(prop,"picture")) && c->ctrl_kind==GMB_CK_TEXTBOX) return ((gmb_text_input_t*)c->user)->format;
    if (!strcasecmp(prop,"filter") && c->ctrl_kind==GMB_CK_TBROWSE) return gmb_browse_filter((gmb_browse_t*)c->user);
    if ((!strcasecmp(prop,"sortkeys")||!strcasecmp(prop,"sortmulti")) && c->ctrl_kind==GMB_CK_TBROWSE) { static char _sk[128]; _sk[0]=0; gmb_browse_sortkeys((gmb_browse_t*)c->user, _sk, sizeof _sk); return _sk; }
    if (!strcasecmp(prop,"csv") && c->ctrl_kind==GMB_CK_TBROWSE) { static char _cv[16384]; _cv[0]=0; gmb_browse_csv((gmb_browse_t*)c->user, _cv, sizeof _cv); return _cv; }
    if (!strcasecmp(prop,"forecolor")||!strcasecmp(prop,"fg")||!strcasecmp(prop,"foreground")) { static char _fc[16]; if (!c->style.has_fg) return ""; snprintf(_fc,sizeof _fc,"#%02X%02X%02X", c->style.fg.r, c->style.fg.g, c->style.fg.b); return _fc; }
    if (!strcasecmp(prop,"backcolor")||!strcasecmp(prop,"bg")||!strcasecmp(prop,"background")) { static char _bc[16]; if (!c->style.has_bg) return ""; snprintf(_bc,sizeof _bc,"#%02X%02X%02X", c->style.bg.r, c->style.bg.g, c->style.bg.b); return _bc; }
    { const char* extra = control_extra_get(c, prop); if (extra) return extra; }
    return "";
}


void gmb_form_control_set(const char* name, const char* prop, double num, const char* str)
{
    int b; int i = ctrl_find(name, &b); if (i < 0) { tui_ctrl_set(name, prop, num, str); return; } if (!prop) return;
    gmb_pix_widget_t* w = b ? NULL : &g_wid[i];
    gmb_form_btn_t* t = b ? &g_btns[i] : NULL;
    if      (!strcasecmp(prop, "left") || !strcasecmp(prop, "x"))      { if (w) w->base.x = (int)num; else t->base.x = (int)num; }
    else if (!strcasecmp(prop, "top") || !strcasecmp(prop, "y"))       { if (w) w->base.y = (int)num; else t->base.y = (int)num; }
    else if (!strcasecmp(prop, "width") || !strcasecmp(prop, "w"))     { if (w) w->base.width = (int)num; else t->base.width = (int)num; }
    else if (!strcasecmp(prop, "height") || !strcasecmp(prop, "h"))    { if (w) w->base.height = (int)num; else t->base.height = (int)num; }
    else if (!strcasecmp(prop, "visible"))                            { if (w) w->base.visible = (int)num; else t->base.visible = (int)num; }
    else if (!strcasecmp(prop, "enabled"))                            { if (w) w->base.enabled = (int)num; else t->base.enabled = (int)num; }
    else if (!strcasecmp(prop, "tabindex"))                           { if (w) w->base.tab_index = (int)num; else t->base.tab_index = (int)num; }
    else if (!strcasecmp(prop, "tabstop"))                            { if (w) w->base.tab_stop = (int)num; else t->base.tab_stop = (int)num; }
    else if (!strcasecmp(prop, "metric") || !strcasecmp(prop, "coordmode")) { if (w) w->base.coord_mode = (int)num; else t->base.coord_mode = (int)num; }
    else if (!strcasecmp(prop, "value") || !strcasecmp(prop, "listindex")) { if (w) wd_set_num(w, num); }
    else if (!strcasecmp(prop, "selected") || !strcasecmp(prop, "index")) { if (w) wd_set_num(w, num); }
    else if ((!strcasecmp(prop, "items") || !strcasecmp(prop, "list")) && str && w) {
        char buf[1536], *save = NULL; int n = 0;
        snprintf(buf, sizeof(buf), "%s", str);
        for (char* tk = strtok_r(buf, "|", &save); tk && n < 8; tk = strtok_r(NULL, "|", &save))
            snprintf(w->items[n++], sizeof(w->items[0]), "%s", tk);
        w->nitems = n;
    }
    else if (!strcasecmp(prop, "caption") || !strcasecmp(prop, "text") || !strcasecmp(prop, "label")) {
        if (str) { if (w) { snprintf(w->label, sizeof(w->label), "%s", str); if (w->strptr || w->kind == W_TEXT) wd_set_str(w, str); } else snprintf(t->label, sizeof(t->label), "%s", str); } }
    else if (!strcasecmp(prop, "tooltip")) { if (str) { if (w) snprintf(w->tooltip, sizeof(w->tooltip), "%s", str); else snprintf(t->tooltip, sizeof(t->tooltip), "%s", str); } }
    else if (!strcasecmp(prop, "tag"))     { if (str) { if (w) snprintf(w->tag, sizeof(w->tag), "%s", str); else snprintf(t->tag, sizeof(t->tag), "%s", str); } }
    else if (!strcasecmp(prop, "name"))    { if (str) { if (w) snprintf(w->name, sizeof(w->name), "%s", str); else snprintf(t->name, sizeof(t->name), "%s", str); } }
    else if (!strcasecmp(prop, "min") || !strcasecmp(prop, "minvalue")) { if (w) w->vmin = num; }
    else if (!strcasecmp(prop, "max") || !strcasecmp(prop, "maxvalue")) { if (w) w->vmax = num; }
    else if (!strcasecmp(prop, "step"))    { if (w) w->vstep = num; }
    else if (!strcasecmp(prop, "format"))  { if (str && w) snprintf(w->dfmt, sizeof(w->dfmt), "%s", str); }
    else if (!strcasecmp(prop, "mask"))    { if (str && w) snprintf(w->dmask, sizeof(w->dmask), "%s", str); }
    else if (!strcasecmp(prop, "showpercent")) { if (w) w->show_percent = (num != 0); }
    else if (!strcasecmp(prop, "barstyle")) { if (str && w) snprintf(w->bar_style, sizeof(w->bar_style), "%s", str); }
    else if (!strcasecmp(prop, "align") || !strcasecmp(prop, "alignment")) { if (w) w->align = (int)num; }
    else if (!strcasecmp(prop, "valid") || !strcasecmp(prop, "isvalid")) { if (w) w->is_valid = (num != 0); }
    else control_extra_set(w ? &w->base : &t->base, prop, num, str);
    g_dirty = 1;
}

double gmb_form_control_get(const char* name, const char* prop)
{
    int b; int i = ctrl_find(name, &b); if (i < 0) return tui_ctrl_get(name, prop); if (!prop) return 0;
    gmb_pix_widget_t* w = b ? NULL : &g_wid[i];
    gmb_form_btn_t* t = b ? &g_btns[i] : NULL;
    if      (!strcasecmp(prop, "left") || !strcasecmp(prop, "x"))   return w ? w->base.x : t->base.x;
    else if (!strcasecmp(prop, "top") || !strcasecmp(prop, "y"))    return w ? w->base.y : t->base.y;
    else if (!strcasecmp(prop, "width") || !strcasecmp(prop, "w"))  return w ? w->base.width : t->base.width;
    else if (!strcasecmp(prop, "height") || !strcasecmp(prop, "h")) return w ? w->base.height : t->base.height;
    else if (!strcasecmp(prop, "visible"))                          return w ? w->base.visible : t->base.visible;
    else if (!strcasecmp(prop, "enabled"))                          return w ? w->base.enabled : t->base.enabled;
    else if (!strcasecmp(prop, "tabindex"))                         return w ? w->base.tab_index : t->base.tab_index;
    else if (!strcasecmp(prop, "tabstop"))                          return w ? w->base.tab_stop : t->base.tab_stop;
    else if (!strcasecmp(prop, "metric") || !strcasecmp(prop, "coordmode")) return w ? w->base.coord_mode : t->base.coord_mode;
    else if (!strcasecmp(prop, "value") || !strcasecmp(prop, "selected") || !strcasecmp(prop, "index") || !strcasecmp(prop, "listindex")) return w ? wd_num(w) : 0;
    else if (!strcasecmp(prop, "listcount")) return w ? w->nitems : 0;
    else if (!strcasecmp(prop, "min") || !strcasecmp(prop, "minvalue")) return w ? w->vmin : 0;
    else if (!strcasecmp(prop, "max") || !strcasecmp(prop, "maxvalue")) return w ? w->vmax : 0;
    else if (!strcasecmp(prop, "step")) return w ? w->vstep : 0;
    else if (!strcasecmp(prop, "showpercent")) return w ? w->show_percent : 0;
    else if (!strcasecmp(prop, "align") || !strcasecmp(prop, "alignment")) return w ? w->align : 0;
    else if (!strcasecmp(prop, "valid") || !strcasecmp(prop, "isvalid")) return w ? w->is_valid : 0;
    { const char* extra = control_extra_get(w ? &w->base : &t->base, prop); if (extra) return atof(extra); }
    return 0;
}
const char* gmb_form_control_get_str(const char* name, const char* prop)
{
    int b; int i = ctrl_find(name, &b); if (i < 0) return tui_ctrl_get_str(name, prop); if (!prop) return "";
    if (!strcasecmp(prop, "caption") || !strcasecmp(prop, "text") || !strcasecmp(prop, "label")) { if (b) return g_btns[i].label; if (g_wid[i].strptr || g_wid[i].kind == W_TEXT) return wd_str(&g_wid[i]); return g_wid[i].label; }
    if (!strcasecmp(prop, "tooltip")) return b ? g_btns[i].tooltip : g_wid[i].tooltip;
    if (!strcasecmp(prop, "tag"))     return b ? g_btns[i].tag : g_wid[i].tag;
    if (!strcasecmp(prop, "name"))    return b ? g_btns[i].name : g_wid[i].name;
    if (!strcasecmp(prop, "format"))  return b ? "" : g_wid[i].dfmt;
    if (!strcasecmp(prop, "mask"))    return b ? "" : g_wid[i].dmask;
    if (!strcasecmp(prop, "barstyle")) return b ? "" : g_wid[i].bar_style;
    if ((!strcasecmp(prop, "items") || !strcasecmp(prop, "list")) && !b) {
        static char _it[1024]; int off = 0; _it[0] = 0;
        for (int k = 0; k < g_wid[i].nitems && off < (int)sizeof(_it) - 1; k++)
            off += snprintf(_it + off, sizeof(_it) - off, "%s%s", k ? "|" : "", g_wid[i].items[k]);
        return _it;
    }
    { const char* extra = control_extra_get(b ? &g_btns[i].base : &g_wid[i].base, prop); if (extra) return extra; }
    return "";
}
void gmb_form_control_move(const char* name, int x, int y, int ww, int hh)
{
    int b; int i = ctrl_find(name, &b); if (i < 0) { gmb_control_t* tc = gmb_tui_control_find_by_name(name); if (tc) { tc->x=x; tc->y=y; if (ww>0) tc->width=ww; if (hh>0) tc->height=hh; } return; }
    int* px; int* py; int* pw; int* ph;
    if (b) { px=&g_btns[i].base.x; py=&g_btns[i].base.y; pw=&g_btns[i].base.width; ph=&g_btns[i].base.height; }
    else   { px=&g_wid[i].base.x;  py=&g_wid[i].base.y;  pw=&g_wid[i].base.width;  ph=&g_wid[i].base.height;  }
    *px = x; *py = y; if (ww > 0) *pw = ww; if (hh > 0) *ph = hh;
    g_dirty = 1;
}
void gmb_form_control_focus(const char* name)
{
    int b; int i = ctrl_find(name, &b); if (i < 0) { gmb_control_t* tc = gmb_tui_control_find_by_name(name); if (tc) tc->focused = true; return; }
    if (b) { g_focus_btn = i; }
    else   { set_focus_wid(i); }
    g_dirty = 1;
}
void gmb_form_control_zorder(const char* name, int front)
{
    int b; int i = ctrl_find(name, &b); if (i < 0) return;
    if (!b && g_nwid > 1) {
        gmb_pix_widget_t tmp = g_wid[i];
        if (front) { for (int k = i; k < g_nwid - 1; k++) g_wid[k] = g_wid[k + 1]; g_wid[g_nwid - 1] = tmp; }
        else       { for (int k = i; k > 0; k--) g_wid[k] = g_wid[k - 1]; g_wid[0] = tmp; }
    } else if (b && g_nbtn > 1) {
        gmb_form_btn_t tmp = g_btns[i];
        if (front) { for (int k = i; k < g_nbtn - 1; k++) g_btns[k] = g_btns[k + 1]; g_btns[g_nbtn - 1] = tmp; }
        else       { for (int k = i; k > 0; k--) g_btns[k] = g_btns[k - 1]; g_btns[0] = tmp; }
    }
    g_dirty = 1;
}

/* §11.7 Control: Invalidate/Refresh. Nel modello "draw-all" (ogni frame ridisegna i controlli)
 * entrambi marcano la form "sporca" → il prossimo paint ridisegna. (Refresh non forza un loop
 * qui: il frame successivo del loop applicativo è il repaint.) */
void gmb_form_control_invalidate(const char* name) { (void)name; g_dirty = 1; }
void gmb_form_control_refresh(const char* name)    { (void)name; g_dirty = 1; }

/* Chip/bottone pixel "virtuale" (barra tasti, toolbar): riusa draw_button_body con lo
 * stile del TEMA. state: 0=normale, 1=hover, 2=premuto. Serve ai draw_px_fn dei
 * controlli TUI-first per avere lo stesso rendering (centratura testo inclusa). */
void gmb_form_draw_chip(int x, int y, int w, int h, const char* label, int state)
{
    gmb_form_btn_t b; memset(&b, 0, sizeof(b));
    b.base.x = x; b.base.y = y; b.base.width = w; b.base.height = h;
    b.base.enabled = 1; b.base.visible = 1;
    b.style = gmb_widget_style_current();
    b.hover = (state == 1); b.pressed = (state == 2);
    snprintf(b.label, sizeof(b.label), "%s", label ? label : "");
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
    draw_button_body(&b, sc);
}

const char* gmb_form_widget_name(int i)    { return (i >= 0 && i < g_nwid) ? g_wid[i].name : ""; }
const char* gmb_form_widget_tooltip(int i) { return (i >= 0 && i < g_nwid) ? g_wid[i].tooltip : ""; }
const char* gmb_form_widget_tag(int i)     { return (i >= 0 && i < g_nwid) ? g_wid[i].tag : ""; }
int gmb_form_widget_visible(int i)         { return (i >= 0 && i < g_nwid) ? g_wid[i].base.visible : 0; }
int gmb_form_widget_enabled(int i)         { return (i >= 0 && i < g_nwid) ? g_wid[i].base.enabled : 0; }
int gmb_form_widget_tabindex(int i)        { return (i >= 0 && i < g_nwid) ? g_wid[i].base.tab_index : 0; }
int gmb_form_widget_tabstop(int i)         { return (i >= 0 && i < g_nwid) ? g_wid[i].base.tab_stop : 0; }
int gmb_form_add_button_css(int x, int y, int w, int h, const char* label, gmb_form_cb cb, const char* spec)
{
    int idx = gmb_form_add_button(x, y, w, h, label, cb);
    if (idx >= 0) {
        /* In TEXT il bottone vive nel registry TUI (vedi gmb_form_add_button): `idx` è
         * l'HANDLE TUI, non un indice in g_btns → la spec va applicata per handle. */
        if (!GMB_GFX_SCREEN_ON()) {
            extern void gmb_control_apply_style_handle(int handle, const char* spec);
            gmb_control_apply_style_handle(idx, spec);
            return idx;
        }
        style_apply(&g_btns[idx].style, spec); apply_base_props_btn(&g_btns[idx], spec);
        if (spec && strstr(spec, "primary")) {   /* primario = accento del tema (esplicito) */
            gmb_widget_style_t st = gmb_widget_style_current();
            unsigned a = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
            g_btns[idx].style.fill = (a & 0xFFFFFFu) | (0xFFu << 24);
            g_btns[idx].style.fill_hover = g_btns[idx].style.fill;
            g_btns[idx].style.fill_press = g_btns[idx].style.fill;
            g_btns[idx].style.text = 0xFFFFFFFFu;
        }
        g_dirty = 1; }
    return idx;
}

static int form_add_widget(int kind, int x, int y, int w, int h, const char* items, double* var)
{
    if (g_nwid >= GMB_FORM_MAX_WIDGETS) return -1;
    gmb_pix_widget_t* wd = &g_wid[g_nwid];
    memset(wd, 0, sizeof(*wd));
    wd->kind = kind; wd->base.x = x; wd->base.y = y; wd->base.width = w; wd->base.height = h; wd->var = var; wd->ival = var ? *var : 0.0;
    wd->base.visible = 1; wd->base.enabled = 1; wd->base.tab_stop = 1;
    g_last_is_tui = 0;   /* l'ultimo controllo creato è un FORM widget */
    /* Controlli a CONTENUTO: la dimensione la calcola il runtime, il programma ha dichiarato
     * solo la posizione → non va convertita (vedi gmb_control_t.coord_auto_w/h). */
    switch (kind) {
        case W_CHECK: case W_TOGGLE: case W_RADIO: case W_TABS: case W_MENUBAR:
        case W_DATE: case W_COLORPICKER:
            wd->base.coord_auto_w = 1; wd->base.coord_auto_h = 1; break;
        case W_TRACK: case W_DROPDOWN: case W_PROGRESS: case W_STATUSBAR: case W_SPINNER:
            wd->base.coord_auto_h = 1; break;
        default: break;
    }
    wd->style = gmb_widget_style_current();
    if (items) wd->nitems = form_split_items(items, wd->items, 8);
    g_dirty = 1;
    g_last_added = g_nwid;
    g_last_handle = g_nwid;   /* handle widget */
    wd->base.handle = g_nwid;  /* base Control: handle univoco */
    return g_nwid++;
}

int gmb_form_add_checkbox(int x, int y, const char* label, double* var)
{
    int i = form_add_widget(W_CHECK, x, y, 200, 22, NULL, var);
    if (i >= 0) snprintf(g_wid[i].label, sizeof(g_wid[i].label), "%s", label ? label : "");
    return i;
}
int gmb_form_add_radio(int x, int y, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_radiogroup(int, int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_radiogroup(x, y, 200, 22, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    int i = form_add_widget(W_RADIO, x, y, 200, 22, items, var);
    if (i >= 0) g_wid[i].base.height = g_wid[i].nitems * 22;
    return i;
}
void gmb_form_set_last_value(double v)
{
    int i = g_last_added;
    if (i >= 0 && i < g_nwid) { g_wid[i].ival = v; if (g_wid[i].var) *(g_wid[i].var) = v; g_dirty = 1; }
}
int gmb_form_add_track(int x, int y, int w, double vmin, double vmax, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_slider(int, int, int, int, int, int);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_slider(x, y, w, (int)vmin, (int)vmax, (int)(var ? *var : vmin));
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    int i = form_add_widget(W_TRACK, x, y, w, 24, NULL, var);
    if (i >= 0) { g_wid[i].vmin = vmin; g_wid[i].vmax = vmax; }
    return i;
}
int gmb_form_add_dropdown(int x, int y, int w, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_combobox(int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_combobox(x, y, w, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return form_add_widget(W_DROPDOWN, x, y, w, 28, items, var);
}

/* Factory PIXEL esplicita (senza dispatch polimorfico TUI/GRAPHICS): serve ai test
 * headless e a chi vuole il widget pixel anche senza screen attivo. kind = GMB_PX_*. */
int gmb_form_add_widget_px(int kind, int x, int y, int w, int h, const char* items, double* var)
{
    return form_add_widget(kind, x, y, w, h, items, var);
}

/* Factory PIXEL del checkbox: imposta il caption (test: il testo accanto al quadratino). */
int gmb_form_add_checkbox_px(int x, int y, const char* label, double* var)
{
    int i = form_add_widget(W_CHECK, x, y, 200, 22, NULL, var);
    if (i >= 0) snprintf(g_wid[i].label, sizeof(g_wid[i].label), "%s", label ? label : "");
    return i;
}

/* Caption corrente di un widget pixel (per i test: il testo deve essere quello
 * dichiarato, non la larghezza o una stringa vuota). */
const char* gmb_form_widget_caption(int i)
{
    return (i >= 0 && i < g_nwid) ? g_wid[i].label : "";
}

/* PANEL: card di sezione (sfondo) con titolo opzionale. Va creato PRIMA dei
 * controlli che ospita, cosi' resta dietro (ordine di registrazione). */
int gmb_form_add_panel(int x, int y, int w, int h, const char* title)
{
    int i = form_add_widget(W_PANEL, x, y, w, h, NULL, NULL);
    if (i >= 0) snprintf(g_wid[i].label, sizeof(g_wid[i].label), "%s", title ? title : "");
    return i;
}

int gmb_form_add_progress(int x, int y, int w, const double* var)
{
    int h = form_add_widget(W_PROGRESS, x, y, w, 16, NULL, (double*)var);
    if (h >= 0 && h < g_nwid) g_wid[h].show_percent = 1;   /* default: mostra la % (come Clipper) */
    return h;
}
int gmb_form_add_progress_v(int x, int y, int w, double value)
{
    int h = gmb_form_add_progress(x, y, w, NULL);
    if (h >= 0 && h < g_nwid) { g_wid[h].ival = value; if (g_wid[h].var) *(g_wid[h].var) = value; g_dirty = 1; }
    return h;
}
static int gfx_add_widget(int kind, int x, int y, int w, int h, const char* label, const char* items)
{
    int idx = form_add_widget(kind, x, y, w, h, items, NULL);
    if (idx >= 0) snprintf(g_wid[idx].label, sizeof(g_wid[idx].label), "%s", label ? label : "");
    return idx;
}
int gmb_form_add_canvas(int x, int y, int w, int h) { return gfx_add_widget(W_CANVAS, x, y, w, h, NULL, NULL); }
int gmb_form_add_image(int x, int y, int w, int h, const char* label) { return gfx_add_widget(W_IMAGE, x, y, w, h, label, NULL); }
int gmb_form_add_imagebutton(int x, int y, int w, int h, const char* label) { return gfx_add_widget(W_IMAGEBUTTON, x, y, w, h, label, NULL); }
int gmb_form_add_qr(int x, int y, int w, int h, const char* text) { return gfx_add_widget(W_QR, x, y, w, h, text, NULL); }
int gmb_form_add_richtext(int x, int y, int w, int h, const char* text) { return gfx_add_widget(W_RICHTEXT, x, y, w, h, text, text); }
int gmb_form_add_halfblock(int x, int y, int w, int h, const char* label) { return gfx_add_widget(W_HALFBLOCK, x, y, w, h, label, NULL); }
int gmb_form_add_tiling(int x, int y, int w, int h, const char* label) { return gfx_add_widget(W_TILING, x, y, w, h, label, NULL); }
static int grid_cell(const gmb_pix_widget_t* wd, int r, int c, char* out, int cap)
{
    const char* p = wd->gdata; int row = 0;
    while (*p && row < r) { if (*p == ';') row++; p++; }
    if (row != r) { out[0] = 0; return 0; }
    int col = 0;
    while (*p && *p != ';' && col < c) { if (*p == '|') col++; p++; }
    if (col != c) { out[0] = 0; return 0; }
    int i = 0; while (*p && *p != ';' && *p != '|' && i < cap - 1) out[i++] = *p++;
    out[i] = 0; return 1;
}
int gmb_form_add_grid(int x, int y, int w, int h, const char* headers, const char* data, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_grid(int, int, int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        int cols = 0; { const char* p = headers; if (p && *p) { cols = 1; for (; *p; p++) if (*p == '|') cols++; } }
        gmb_control_t* c = gmb_tui_add_grid(x, y, w, h, cols, data);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    int i = form_add_widget(W_GRID, x, y, w, h, NULL, var);
    if (i >= 0) {
        gmb_pix_widget_t* wd = &g_wid[i];
        wd->gnc = form_split_items(headers, wd->ghead, 8);
        snprintf(wd->gdata, sizeof(wd->gdata), "%s", data ? data : "");
        int r = 1; for (const char* p = wd->gdata; *p; p++) if (*p == ';') r++;
        wd->gnr = r; if (wd->gnr > 64) wd->gnr = 64;
        for (int k = 0; k < wd->gnr; k++) wd->gorder[k] = k;
        wd->gsort_col = -1;
    }
    return i;
}
int gmb_form_add_filedialog(int x, int y, int w, int h, const char* dir, void* strptr)
{
    int i = form_add_widget(W_FILE, x, y, w, h, NULL, NULL);
    if (i >= 0) {
        gmb_pix_widget_t* wd = &g_wid[i];
        wd->strptr = strptr;
        snprintf(wd->label, sizeof(wd->label), "%s", (dir && *dir) ? dir : ".");
        DIR* d = opendir(wd->label);
        if (d) { struct dirent* e; while ((e = readdir(d)) && wd->nbig < 64) { if (e->d_name[0] == '.') continue; snprintf(wd->big[wd->nbig++], 56, "%s", e->d_name); } closedir(d); }
    }
    return i;
}
int gmb_form_add_tree(int x, int y, int w, int h, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_treeview(int, int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_treeview(x, y, w, h, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    int i = form_add_widget(W_TREE, x, y, w, h, items, var);
    return i;
}

/* Campo di testo pixel con caret/editing (cursore/editing comune della base Control).
 * `strptr` e' un gmb_string_t* del programma (INTO var$): il testo vive li'. */
int gmb_form_add_textbox(int x, int y, int w, int h, void* strptr)
{
    int i = form_add_widget(W_TEXT, x, y, w, h, NULL, NULL);
    if (i >= 0) { g_wid[i].strptr = strptr; g_wid[i].tcur = strptr ? (int)strlen(((gmb_string_t*)strptr)->data ? ((gmb_string_t*)strptr)->data : "") : 0; }
    return i;
}
int gmb_form_add_datepicker(int x, int y, void* strptr, const char* fmt)
{
    int i = form_add_widget(W_DATE, x, y, 150, 24, NULL, NULL);
    if (i >= 0) {
        gmb_pix_widget_t* wd = &g_wid[i];
        wd->strptr = strptr;
        const char* v = strptr ? ((gmb_string_t*)strptr)->data : "";
        snprintf(wd->dfmt, sizeof(wd->dfmt), "%s", (fmt && *fmt) ? fmt : "dd/mm/yyyy");
        int yy = 0, mm = 0, dd = 0;
        time_t t = time(NULL); struct tm* lt = localtime(&t);
        if (v && sscanf(v, "%d-%d-%d", &yy, &mm, &dd) == 3) {
            wd->dy = yy; wd->dm = mm; wd->sy = yy; wd->sm = mm; wd->sd = dd; wd->dday = dd;
        } else {   /* nessun valore specificato: si usa la data odierna */
            wd->dy = lt->tm_year + 1900; wd->dm = lt->tm_mon + 1; wd->dday = lt->tm_mday;
            wd->sy = wd->dy; wd->sm = wd->dm; wd->sd = wd->dday;
        }
        if (!wd->dm) { wd->dy = 2026; wd->dm = 1; }
        date_mask(wd->dfmt, wd->dmask, sizeof(wd->dmask)); wd->dcur = 0;
    }
    return i;
}
int gmb_form_add_toggle(int x, int y, const char* label, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_toggle(int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_toggle(x, y, 220, label);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    int i = form_add_widget(W_TOGGLE, x, y, 220, 24, NULL, var);
    if (i >= 0) snprintf(g_wid[i].label, sizeof(g_wid[i].label), "%s", label ? label : "");
    return i;
}
int gmb_form_add_spinner(int x, int y, int w, double vmin, double vmax, double step, double* var)
{
    int i = form_add_widget(W_SPINNER, x, y, w, 28, NULL, var);
    if (i >= 0) { g_wid[i].vmin = vmin; g_wid[i].vmax = vmax; g_wid[i].vstep = step > 0 ? step : 1; }
    return i;
}
int gmb_form_add_statusbar(int x, int y, int w, const char* text)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_statusbar(int, int, int, const char*);
        gmb_control_t* c = gmb_tui_add_statusbar(x, y, w, text);
        if (!c) return -1;
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    int i = form_add_widget(W_STATUSBAR, x, y, w, 24, NULL, NULL);
    if (i >= 0) snprintf(g_wid[i].label, sizeof(g_wid[i].label), "%s", text ? text : "");
    return i;
}
int gmb_form_add_tabs(int x, int y, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_tab(int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_tab(x, y, 60, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return form_add_widget(W_TABS, x, y, 60, 30, items, var);
}
int gmb_form_add_menubar(int x, int y, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_menu(int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_menu(x, y, 400, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return form_add_widget(W_MENUBAR, x, y, 400, 24, items, var);
}
int gmb_form_add_listbox(int x, int y, int w, int h, const char* items, double* var)
{
    if (!GMB_GFX_SCREEN_ON())
    {
        extern gmb_control_t* gmb_tui_add_listbox(int, int, int, int, const char*);
        extern void gmb_tui_bind_value(int, double*);
        gmb_control_t* c = gmb_tui_add_listbox(x, y, w, h, items);
        if (!c) return -1;
        if (var) gmb_tui_bind_value(c->handle, var);
        gmb_form_note_handle(c->handle);
        return c->handle;
    }
    return form_add_widget(W_LISTBOX, x, y, w, h, items, var);
}
int gmb_form_add_colorpicker(int x, int y, const char* palette, double* var)
{
    int i = form_add_widget(W_COLORPICKER, x, y, 0, 0, NULL, var);
    if (i >= 0) {
        gmb_pix_widget_t* wd = &g_wid[i];
        char buf[256]; snprintf(buf, sizeof(buf), "%s", palette ? palette : "");
        char* sv = NULL;
        for (char* tk = strtok_r(buf, ",", &sv); tk && wd->ncolors < 16; tk = strtok_r(NULL, ",", &sv)) {
            unsigned int c = (unsigned int)strtoul(tk, NULL, 16); wd->colors[wd->ncolors++] = 0xFF000000u | (c & 0xFFFFFFu);
        }
        if (!wd->ncolors) { unsigned int def[8] = { 0xFF000000u,0xFFFFFFFFu,0xFFFF0000u,0xFF00C000u,0xFF3060FFu,0xFFFFD400u,0xFFFF00FFu,0xFF00E5FFu }; for (int k = 0; k < 8; k++) wd->colors[wd->ncolors++] = def[k]; }
        wd->base.width = 8 * 26; wd->base.height = ((wd->ncolors + 7) / 8) * 26;
    }
    return i;
}

/* Word-wrap grezzo su pixel-width (device). Ritorna il numero di righe. */
static int form_wrap(const char* text, int maxw, char out[][160], int cap)
{
    int n = 0; char cur[160]; cur[0] = 0;
    const char* p = text ? text : "";
    while (*p && n < cap) {
        while (*p == ' ') p++;
        const char* s = p; while (*p && *p != ' ') p++;
        int wl = (int)(p - s); if (wl > 150) wl = 150;
        char word[160]; memcpy(word, s, (size_t)wl); word[wl] = 0;
        char cand[160];
        if (cur[0]) snprintf(cand, sizeof(cand), "%s %s", cur, word);
        else        snprintf(cand, sizeof(cand), "%s", word);
        if (cur[0] && GMB_GFX_TEXT_W(cand) > maxw) { snprintf(out[n++], 160, "%s", cur); snprintf(cur, sizeof(cur), "%s", word); }
        else snprintf(cur, sizeof(cur), "%s", cand);
    }
    if (cur[0] && n < cap) snprintf(out[n++], 160, "%s", cur);
    return n;
}

/* Dialog grafico modale (ALERT/CONFIRM in grafica): FINESTRA (titolo+X+corpo) con
 * messaggio (word-wrap) e 1-3 bottoni stilizzati. Ritorna l'indice premuto (0..2) o -1.
 * GPM: gira come task (poll + park); modal, blocca solo il task chiamante. */
/* WindowBase-based: il dialog e' una finestra (Form) derivata dalla base comune.
 * Il loop (Load/Activate/Paint/input/Unload + park) e' in gmb_form_win_run. */
typedef struct {
    const char* title;
    const char* icon;
    const char* labels[3];
    int nbtn;
    int px, py, pw, ph, tb, bw, bh, gap, th, icon_w;
    char lines[8][160];
    int nlines;
    gmb_widget_style_t base;
    gmb_form_btn_t dlg[3];
    int hover_i, press_i;
} dlg_ctx_t;

static void dlg_draw(void* vctx)
{
    dlg_ctx_t* c = vctx;
    int px = c->px, py = c->py, pw = c->pw, ph = c->ph, tb = c->tb, th = c->th;
    gmb_widget_style_t* bp = &c->base;
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
    g_dirty = 0;   /* il form sotto non si ridisegna sopra la finestra modale */
    GMB_GFX_FILL_RRECT(px + 5, py + 5, pw, ph, 10, bp->shadow);
    GMB_GFX_FILL_RRECT(px, py, pw, ph, 10, bp->body);
    GMB_GFX_FILL_RRECT(px, py, pw, tb + 8, 10, bp->title_bg);
    GMB_GFX_LINE(px, py + tb - 2, px + pw - 1, py + tb - 2, GFX_RGB(bp->title_bg), "BF");
    GMB_GFX_DRAW_TEXT(c->title ? c->title : "", px + 14, py + (tb - 16) / 2, 0, GFX_RGB(bp->title_fg));
    GMB_GFX_FILL_RRECT(px + pw - 26, py + 8, 18, 18, 4, bp->shadow);
    GMB_GFX_DRAW_TEXT("x", px + pw - 21, py + 7, 0, GFX_RGB(bp->title_fg));
    GMB_GFX_STROKE_RRECT(px, py, pw, ph, 10, bp->border);
    int mx = px + 18;
    if (c->icon_w > 0) {
        unsigned int ic = 0xFF3B82F6u; const char* gl = "i";
        if (!strcasecmp(c->icon, "question")) { ic = 0xFF3B82F6u; gl = "?"; }
        else if (!strcasecmp(c->icon, "warn") || !strcasecmp(c->icon, "warning")) { ic = 0xFFF5A524u; gl = "!"; }
        else if (!strcasecmp(c->icon, "error")) { ic = 0xFFE5484Du; gl = "x"; }
        int icy = py + tb + 14 + (c->nlines * (th + 6)) / 2;
        GMB_GFX_FILL_CIRCLE(px + 40, icy, 40, ic);
        GMB_GFX_DRAW_TEXT(gl, px + 40, icy - th / 2, 1, GFX_RGB(0xFFFFFFu));
        mx = px + 18 + c->icon_w;
    }
    for (int li = 0; li < c->nlines; li++)
        GMB_GFX_DRAW_TEXT(c->lines[li], mx, py + tb + 14 + li * (th + 6), 0, GFX_RGB(bp->text));
    for (int i = 0; i < c->nbtn; i++) { c->dlg[i].hover = (c->hover_i == i); c->dlg[i].pressed = (c->press_i == i); draw_button_body(&c->dlg[i], sc); }
}

static int dlg_evcb(gmb_form_win_t* w, int ev, int x, int y, void* vctx)
{
    dlg_ctx_t* c = vctx;
    if (ev == GMB_EV_KEYPRESS) {
        unsigned ch = (unsigned)x;
        if (ch == '\r' || ch == '\n') { w->result = 0; return 1; }
        if (ch == 0x1b) { w->result = -1; return 1; }
        return 0;
    }
    if (ev == GMB_EV_MOUSEDOWN || ev == GMB_EV_MOUSEUP || ev == GMB_EV_MOUSEMOVE) {
        int hi = -1;
        for (int i = 0; i < c->nbtn; i++) {
            gmb_form_btn_t* b = &c->dlg[i];
            if (x >= b->base.x && x < b->base.x + b->base.width && y >= b->base.y && y < b->base.y + b->base.height) hi = i;
        }
        if (ev == GMB_EV_MOUSEDOWN && hi >= 0) c->press_i = hi;
        else if (ev == GMB_EV_MOUSEUP) { int was = c->press_i; c->press_i = -1; if (was >= 0 && was == hi) { w->result = was; return 1; } }
        c->hover_i = hi;
    }
    return 0;
}

int gmb_form_dialog(const char* title, const char* msg, const char* b0, const char* b1, const char* b2, const char* spec, const char* icon)
{
    if (!GMB_GFX_SCREEN_ON()) return 0;   /* headless/TUI: default */
    dlg_ctx_t c; memset(&c, 0, sizeof(c));
    c.title = title; c.icon = icon;
    c.labels[0] = b0; c.labels[1] = b1; c.labels[2] = b2;
    c.nbtn = (b2 && *b2) ? 3 : ((b1 && *b1) ? 2 : 1);
    c.tb = 34; c.bw = 120; c.bh = 36; c.gap = 12; c.hover_i = -1; c.press_i = -1;

    int sw = GMB_GFX_SCREEN_W(), sh = GMB_GFX_SCREEN_H();
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
    c.th = (int)(GMB_GFX_TEXT_H() / sc + 0.5f); if (c.th <= 0) c.th = 14;

    int pw = 460; if (pw > sw - 20) pw = sw - 20;
    c.icon_w = (icon && *icon && strcasecmp(icon, "none") != 0) ? 56 : 0;
    c.nlines = form_wrap(msg, (int)((pw - 36 - c.icon_w) * sc), c.lines, 8);
    if (c.nlines < 1) { snprintf(c.lines[0], 160, "%s", msg ? msg : ""); c.nlines = 1; }

    int ph = c.tb + 16 + c.nlines * (c.th + 6) + 16 + c.bh + 18;
    if (ph < 150) ph = 150;
    if (ph > sh - 20) ph = sh - 20;
    c.pw = pw; c.ph = ph; c.px = (sw - pw) / 2; c.py = (sh - ph) / 2;

    gmb_widget_style_t base = gmb_widget_style_current();
    base.body = 0xFFEAF3FFu; base.text = 0xFF10233Bu;
    base.title_bg = 0xFF1E3A8Au; base.title_fg = 0xFFFFFFFFu;
    base.border = 0xFF94A3B8u;
    c.base = base;
    int totalw = c.nbtn * c.bw + (c.nbtn - 1) * c.gap;
    int bx = c.px + c.pw - 18 - totalw, by = c.py + c.ph - 18 - c.bh;
    for (int i = 0; i < c.nbtn; i++) {
        c.dlg[i].base.x = bx + i * (c.bw + c.gap); c.dlg[i].base.y = by; c.dlg[i].base.width = c.bw; c.dlg[i].base.height = c.bh;
        c.dlg[i].base.enabled = 1; c.dlg[i].style = base;
        if (i == 0) c.dlg[i].style.fill = base.fill_hover;   /* default = enfatizzato */
        snprintf(c.dlg[i].label, sizeof(c.dlg[i].label), "%s", c.labels[i] ? c.labels[i] : "");
        if (spec && *spec) { style_apply(&c.dlg[i].style, spec); apply_base_props_btn(&c.dlg[i], spec); }
    }

    gmb_form_win_t w;
    gmb_form_win_init(&w);
    snprintf(w.caption, sizeof(w.caption), "%s", title ? title : "");
    w.result = -1;                               /* ESC/chiudi -> -1 */
    gmb_form_win_run(&w, dlg_draw, dlg_evcb, &c, 16);

    GMB_GFX_CLS(0); g_dirty = 1;   /* il form si ridisegna al prossimo present */
    return w.result;
}



/* Dialogo stampa (PRINTERDIALOG): stampante, copie (+/-), fronte/retro, Stampa/Annulla.
 * Ritorna 0 = Stampa, 1 = Annulla. Scrive i target se non NULL. */
typedef struct {
    int px, py, pw, ph, tb, th;
    int ncopies, dpx;
    const char* pshot; int pshot_at, pframe;
    gmb_widget_style_t st;
    gmb_form_btn_t bt[2];
    int hover_i, press_i;
} pdlg_ctx_t;

static void pdlg_draw(void* vctx)
{
    pdlg_ctx_t* c = vctx;
    int px = c->px, py = c->py, pw = c->pw, ph = c->ph, tb = c->tb, th = c->th;
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0) sc = 1;
    GMB_GFX_CLS(0); g_dirty = 0;
    GMB_GFX_FILL_RRECT(px + 5, py + 5, pw, ph, 10, c->st.shadow);
    GMB_GFX_FILL_RRECT(px, py, pw, ph, 10, c->st.body);
    GMB_GFX_FILL_RRECT(px, py, pw, tb + 8, 10, c->st.title_bg);
    GMB_GFX_DRAW_TEXT("Stampa", px + 14, py + (tb - 16) / 2, 0, GFX_RGB(c->st.title_fg));
    GMB_GFX_STROKE_RRECT(px, py, pw, ph, 10, c->st.border);
    GMB_GFX_DRAW_TEXT("Stampante:  Default Printer", px + 18, py + tb + 16, 0, GFX_RGB(c->st.text));
    char cb[64]; snprintf(cb, sizeof(cb), "Copie:  %d     [+ / -]", c->ncopies);
    GMB_GFX_DRAW_TEXT(cb, px + 18, py + tb + 44, 0, GFX_RGB(c->st.text));
    GMB_GFX_DRAW_TEXT(c->dpx ? "[x] Fronte/retro" : "[ ] Fronte/retro", px + 18, py + tb + 72, 0, GFX_RGB(c->st.text));
    for (int i = 0; i < 2; i++) { c->bt[i].hover = (c->hover_i == i); c->bt[i].pressed = (c->press_i == i); draw_button_body(&c->bt[i], sc); }
    if (c->pshot && *c->pshot && c->pshot_at && ++c->pframe == c->pshot_at) GMB_GFX_SAVE_PNG(c->pshot);
}

static int pdlg_evcb(gmb_form_win_t* w, int ev, int x, int y, void* vctx)
{
    pdlg_ctx_t* c = vctx;
    int px = c->px, py = c->py, tb = c->tb, th = c->th;
    if (ev == GMB_EV_KEYPRESS) {
        unsigned ch = (unsigned)x;
        if (ch == '\r' || ch == '\n') { w->result = 0; return 1; }
        if (ch == 0x1b) { w->result = 1; return 1; }
        return 0;
    }
    if (ev == GMB_EV_MOUSEDOWN || ev == GMB_EV_MOUSEUP || ev == GMB_EV_MOUSEMOVE) {
        if (ev == GMB_EV_MOUSEDOWN && x >= px + 18 && x < px + 200 && y >= py + tb + 44 && y < py + tb + 44 + th) { if (x >= px + 110) c->ncopies++; else if (c->ncopies > 1) c->ncopies--; return 0; }
        if (ev == GMB_EV_MOUSEDOWN && x >= px + 18 && x < px + 180 && y >= py + tb + 72 && y < py + tb + 72 + th) { c->dpx = !c->dpx; return 0; }
        int hi = -1;
        for (int i = 0; i < 2; i++) if (x >= c->bt[i].base.x && x < c->bt[i].base.x + c->bt[i].base.width && y >= c->bt[i].base.y && y < c->bt[i].base.y + c->bt[i].base.height) hi = i;
        if (ev == GMB_EV_MOUSEDOWN) { c->press_i = hi; if (hi >= 0) c->hover_i = hi; }
        else if (ev == GMB_EV_MOUSEUP) { int was = c->press_i; c->press_i = -1; if (was >= 0 && was == hi) { w->result = was; return 1; } }
        else c->hover_i = hi;
    }
    return 0;
}

int gmb_form_printerdialog(void* pstr, double* copies, double* duplex)
{
    if (!GMB_GFX_SCREEN_ON()) return 1;
    pdlg_ctx_t c; memset(&c, 0, sizeof(c));
    int sw = GMB_GFX_SCREEN_W(), sh = GMB_GFX_SCREEN_H();
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0) sc = 1;
    c.th = (int)(GMB_GFX_TEXT_H() / sc + 0.5f); if (c.th <= 0) c.th = 14;
    c.tb = 34; c.pw = 380; c.ph = 200; c.px = (sw - c.pw) / 2; c.py = (sh - c.ph) / 2;
    c.st = gmb_widget_style_current();
    c.ncopies = copies ? (int)(*copies + 0.5) : 1; if (c.ncopies < 1) c.ncopies = 1;
    c.dpx = duplex ? (*duplex != 0) : 0;
    const char* lab[2] = { "Stampa", "Annulla" };
    int bw = 120, bh = 34, bx = c.px + c.pw - 18 - (2 * bw + 12), by = c.py + c.ph - 18 - bh;
    for (int i = 0; i < 2; i++) { c.bt[i].base.x = bx + i * (bw + 12); c.bt[i].base.y = by; c.bt[i].base.width = bw; c.bt[i].base.height = bh; c.bt[i].base.enabled = 1; c.bt[i].style = c.st; snprintf(c.bt[i].label, sizeof(c.bt[i].label), "%s", lab[i]); }
    c.pshot = gmb_opt_env("GMB_FORM_SHOT"); { const char* psf = gmb_opt_env("GMB_FORM_SHOT_FRAME"); c.pshot_at = (psf && *psf) ? atoi(psf) : 0; }
    c.hover_i = -1; c.press_i = -1;

    gmb_form_set_exclusive(1);
    gmb_form_win_t w; gmb_form_win_init(&w);
    snprintf(w.caption, sizeof(w.caption), "Stampa");
    w.result = 1;                                 /* default: annulla/chiuso */
    gmb_form_win_run(&w, pdlg_draw, pdlg_evcb, &c, 16);
    GMB_GFX_CLS(0); gmb_form_set_exclusive(0);
    int result = w.result;
    if (result == 0) { if (pstr) *(gmb_string_t*)pstr = gmb_string_alloc("Default Printer"); if (copies) *copies = c.ncopies; if (duplex) *duplex = c.dpx; }
    return result;
}

int gmb_form_msgbox(const char* title, const char* msg, const char* buttons, const char* spec)
{
    char b[3][64]; int n = 0;
    const char* p = (buttons && *buttons) ? buttons : "OK";
    while (*p && n < 3) {
        const char* s = p; while (*p && *p != '|') p++;
        int l = (int)(p - s); if (l > 63) l = 63;
        memcpy(b[n], s, (size_t)l); b[n][l] = 0; n++;
        if (*p == '|') p++;
    }
    if (n == 0) { snprintf(b[0], sizeof(b[0]), "OK"); n = 1; }
    return gmb_form_dialog(title, msg, b[0], n > 1 ? b[1] : NULL, n > 2 ? b[2] : NULL, spec, "info");
}

typedef struct {
    const char* title; const char* prompt;
    int px, py, pw, ph, tb, bh, bw, gap, th, fx, fy, fw, fh;
    char buf[256];
    gmb_widget_style_t base;
    gmb_form_btn_t bt[2];
    int hover_i, press_i;
} inbox_ctx_t;

static void inbox_draw(void* vctx)
{
    inbox_ctx_t* c = vctx;
    int px = c->px, py = c->py, pw = c->pw, ph = c->ph, tb = c->tb, th = c->th;
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
    GMB_GFX_CLS(0); g_dirty = 0;
    GMB_GFX_FILL_RRECT(px + 5, py + 5, pw, ph, 10, c->base.shadow);
    GMB_GFX_FILL_RRECT(px, py, pw, ph, 10, c->base.body);
    GMB_GFX_FILL_RRECT(px, py, pw, tb + 8, 10, c->base.title_bg);
    GMB_GFX_LINE(px, py + tb - 2, px + pw - 1, py + tb - 2, GFX_RGB(c->base.title_bg), "BF");
    GMB_GFX_DRAW_TEXT(c->title ? c->title : "", px + 14, py + (tb - 16) / 2, 0, GFX_RGB(c->base.title_fg));
    GMB_GFX_FILL_RRECT(px + pw - 26, py + 8, 18, 18, 4, c->base.shadow);
    GMB_GFX_DRAW_TEXT("x", px + pw - 21, py + 7, 0, GFX_RGB(c->base.title_fg));
    GMB_GFX_STROKE_RRECT(px, py, pw, ph, 10, c->base.border);
    GMB_GFX_DRAW_TEXT(c->prompt ? c->prompt : "", px + 18, py + tb + 14, 0, GFX_RGB(c->base.text));
    GMB_GFX_FILL_RRECT(c->fx, c->fy, c->fw, c->fh, 6, c->base.fill);
    GMB_GFX_STROKE_RRECT(c->fx, c->fy, c->fw, c->fh, 6, c->base.border);
    GMB_GFX_DRAW_TEXT(c->buf, c->fx + 8, c->fy + (c->fh - th) / 2, 0, GFX_RGB(c->base.text));
    int cx = c->fx + 8 + (int)(GMB_GFX_TEXT_W(c->buf) / sc);
    GMB_GFX_LINE(cx, c->fy + 7, cx, c->fy + c->fh - 7, GFX_RGB(c->base.text), "");
    for (int i = 0; i < 2; i++) { c->bt[i].hover = (c->hover_i == i); c->bt[i].pressed = (c->press_i == i); draw_button_body(&c->bt[i], sc); }
}

static int inbox_evcb(gmb_form_win_t* w, int ev, int x, int y, void* vctx)
{
    inbox_ctx_t* c = vctx;
    if (ev == GMB_EV_KEYPRESS) {
        unsigned ch = (unsigned)x;
        if (ch == '\r' || ch == '\n') { w->result = 0; return 1; }
        if (ch == 0x1b) { w->result = -1; return 1; }
        if (ch == '\b' || ch == 127) { size_t L = strlen(c->buf); if (L) c->buf[L - 1] = 0; return 0; }
        if (ch >= 32) { char kk[2] = { (char)ch, 0 }; size_t L = strlen(c->buf); if (L + 1 < sizeof(c->buf)) strcat(c->buf, kk); return 0; }
        return 0;
    }
    if (ev == GMB_EV_MOUSEDOWN || ev == GMB_EV_MOUSEUP || ev == GMB_EV_MOUSEMOVE) {
        int hi = -1;
        for (int i = 0; i < 2; i++) if (x >= c->bt[i].base.x && x < c->bt[i].base.x + c->bt[i].base.width && y >= c->bt[i].base.y && y < c->bt[i].base.y + c->bt[i].base.height) hi = i;
        if (ev == GMB_EV_MOUSEDOWN && hi >= 0) c->press_i = hi;
        else if (ev == GMB_EV_MOUSEUP) { int was = c->press_i; c->press_i = -1; if (was >= 0 && was == hi) { w->result = was; return 1; } }
        c->hover_i = hi;
    }
    return 0;
}

gmb_string_t gmb_form_inputbox(const char* title, const char* prompt, const char* def, const char* spec)
{
    if (!GMB_GFX_SCREEN_ON()) return gmb_string_alloc(def ? def : "");
    inbox_ctx_t c; memset(&c, 0, sizeof(c));
    c.title = title; c.prompt = prompt;
    int sw = GMB_GFX_SCREEN_W(), sh = GMB_GFX_SCREEN_H();
    float sc = GMB_GFX_RENDER_SCALE(); if (sc <= 0.0f) sc = 1.0f;
    c.th = (int)(GMB_GFX_TEXT_H() / sc + 0.5f); if (c.th <= 0) c.th = 14;
    c.tb = 34; c.bh = 36; c.bw = 110; c.gap = 12;
    int pw = 480; if (pw > sw - 20) pw = sw - 20;
    int fh = 34, ph = c.tb + 14 + c.th + 14 + fh + 16 + c.bh + 18;
    if (ph < 170) ph = 170;
    if (ph > sh - 20) ph = sh - 20;
    c.pw = pw; c.ph = ph; c.px = (sw - pw) / 2; c.py = (sh - ph) / 2;
    c.fx = c.px + 18; c.fy = c.py + c.tb + 14 + c.th + 14; c.fw = pw - 36; c.fh = fh;
    snprintf(c.buf, sizeof(c.buf), "%s", def ? def : "");
    c.base = gmb_widget_style_current();
    const char* blab[2] = { "OK", "Annulla" };
    int totalw = 2 * c.bw + c.gap, bx = c.px + c.pw - 18 - totalw, by = c.py + c.ph - 18 - c.bh;
    for (int i = 0; i < 2; i++) {
        c.bt[i].base.x = bx + i * (c.bw + c.gap); c.bt[i].base.y = by; c.bt[i].base.width = c.bw; c.bt[i].base.height = c.bh;
        c.bt[i].base.enabled = 1; c.bt[i].style = c.base;
        if (i == 0) c.bt[i].style.fill = c.base.fill_hover;
        snprintf(c.bt[i].label, sizeof(c.bt[i].label), "%s", blab[i]);
        if (spec && *spec) { style_apply(&c.bt[i].style, spec); apply_base_props_btn(&c.bt[i], spec); }
    }
    c.hover_i = -1; c.press_i = -1;

    gmb_form_win_t w; gmb_form_win_init(&w);
    snprintf(w.caption, sizeof(w.caption), "%s", title ? title : "");
    w.result = -1;
    gmb_form_win_run(&w, inbox_draw, inbox_evcb, &c, 16);

    GMB_GFX_CLS(0); g_dirty = 1;
    return gmb_string_alloc(w.result == 0 ? c.buf : (def ? def : ""));
}

/* Interazione mouse coi widget (chiamata per evento in gmb_form_poll). */
static void widget_mouse(int type, int x, int y, float sc)
{
    for (int i = 0; i < g_nwid; i++) {
        gmb_pix_widget_t* wd = &g_wid[i];
        int X = wd->base.x, Y = wd->base.y, W = wd->base.width, H = wd->base.height;
        {   /* eventi comuni VB6 (MouseMove/Down/Up/Click/DblClick) */
            int inW = (x >= X && x < X + W && y >= Y && y < Y + H);
            if (inW && type == GMB_GFX_MOUSE_MOVE) {
                fire_ev(i, GMB_EV_MOUSEMOVE);
                if (g_down_wid >= 0) { g_dragging = 1; fire_ev(i, GMB_EV_DRAGOVER); }   /* trascinamento in corso */
            }
            if (inW && type == GMB_GFX_MOUSE_DOWN) { fire_ev(i, GMB_EV_MOUSEDOWN); g_down_wid = i; g_dragging = 0; }
            if (type == GMB_GFX_MOUSE_UP) {
                if (inW) {
                    fire_ev(i, GMB_EV_MOUSEUP);
                    if (g_dragging) fire_ev(i, GMB_EV_DRAGDROP);                        /* rilascio sul target */
                    if (g_down_wid == i && !g_dragging) {
                        long long now = form_now_ms();
                        if (g_click_wid == i && now - g_click_ms < 400) fire_ev(i, GMB_EV_DBLCLICK);
                        else fire_ev(i, GMB_EV_CLICK);
                        g_click_ms = (long)now; g_click_wid = i;
                    }
                }
            }
        }
        if (wd->kind == W_CHECK) {
            if (type == GMB_GFX_MOUSE_DOWN && x >= X && x < X + W && y >= Y && y < Y + H) wd->press = 1;
            else if (type == GMB_GFX_MOUSE_UP) {
                int was = wd->press; wd->press = 0;
                if (was && x >= X && x < X + W && y >= Y && y < Y + H) { wd_set_num(wd, wd_num(wd) != 0 ? 0 : 1); fire_ev(i, GMB_EV_CHANGE); g_dirty = 1; }
            }
        } else if (wd->kind == W_RADIO) {
            int rowH = 22;
            if (type == GMB_GFX_MOUSE_UP && rowH > 0 && x >= X && x < X + W && y >= Y && y < Y + H) {
                int row = (y - Y) / rowH;
                if (row >= 0 && row < wd->nitems) { wd_set_num(wd, row); fire_ev(i, GMB_EV_CHANGE); g_dirty = 1; }
            }
        } else if (wd->kind == W_TRACK) {
            if (type == GMB_GFX_MOUSE_DOWN && x >= X - 9 && x < X + W + 9 && y >= Y && y < Y + H) wd->press = 1;
            if ((type == GMB_GFX_MOUSE_MOVE || type == GMB_GFX_MOUSE_DOWN) && wd->press) {
                double t = (W > 1) ? (double)(x - X) / (W - 1) : 0; if (t < 0) t = 0; if (t > 1) t = 1;
                wd_set_num(wd, wd->vmin + t * (wd->vmax - wd->vmin)); fire_ev(i, GMB_EV_SCROLL); fire_ev(i, GMB_EV_CHANGE); g_dirty = 1;
            }
            if (type == GMB_GFX_MOUSE_UP) wd->press = 0;
        } else if (wd->kind == W_TOGGLE) {
            if (type == GMB_GFX_MOUSE_UP && x >= X && x < X + W && y >= Y && y < Y + H) { wd_set_num(wd, wd_num(wd) != 0 ? 0 : 1); fire_ev(i, GMB_EV_CHANGE); g_dirty = 1; }
        } else if (wd->kind == W_SPINNER) {
            if (type == GMB_GFX_MOUSE_UP && x >= X && x < X + W && y >= Y && y < Y + H) {
                int bwd = 26;
                if (x >= X + W - bwd) { wd_add_num(wd, wd->vstep); fire_ev(i, GMB_EV_UPCLICK); fire_ev(i, GMB_EV_CHANGE); }
                else if (x >= X + W - 2 * bwd) { wd_add_num(wd, -wd->vstep); fire_ev(i, GMB_EV_DOWNCLICK); fire_ev(i, GMB_EV_CHANGE); }
                { double vv = wd_num(wd); if (vv < wd->vmin) vv = wd->vmin; if (vv > wd->vmax) vv = wd->vmax; wd_set_num(wd, vv); }
                g_dirty = 1;
            }
        } else if (wd->kind == W_MENUBAR) {
            if (x >= X && x < X + W && y >= Y && y < Y + H) {
                int mx = X, hi = -1;
                for (int j = 0; j < wd->nitems; j++) { int iw = (int)(GMB_GFX_TEXT_END_X(wd->items[j], 0, 0) - GMB_GFX_TEXT_END_X("", 0, 0)) + 18; if (x >= mx && x < mx + iw) { hi = j; break; } mx += iw; }
                if (hi != wd->hover) { wd->hover = hi; g_dirty = 1; }
                if (type == GMB_GFX_MOUSE_UP && hi >= 0) { wd_set_num(wd, hi); fire_ev(i, GMB_EV_CHANGE); g_dirty = 1; }
            }
        } else if (wd->kind == W_TABS) {
            if (type == GMB_GFX_MOUSE_UP && y >= Y && y < Y + H) {
                int tx = X;
                for (int j = 0; j < wd->nitems; j++) {
                    int lw = (int)(GMB_GFX_TEXT_W(wd->items[j]) / sc) + 20; if (lw < 60) lw = 60;
                    int tw = lw;
                    if (x >= tx && x < tx + tw) { wd_set_num(wd, j); fire_ev(i, GMB_EV_CHANGE); g_dirty = 1; break; }
                    tx += tw + 4;
                }
            }
        } else if (wd->kind == W_LISTBOX) {
            int rowH = 22;
            if (type == GMB_GFX_MOUSE_DOWN && rowH > 0 && x >= X && x < X + W && y >= Y && y < Y + H) {
                int r = (y - Y) / rowH, j = wd->first + r;
                if (j >= 0 && j < wd->nitems) { wd_set_num(wd, j); fire_ev(i, GMB_EV_CHANGE); fire_ev(i, GMB_EV_SELCHANGE); g_dirty = 1; }
            }
        } else if (wd->kind == W_COLORPICKER) {
            int cell = 26;
            if (type == GMB_GFX_MOUSE_UP && cell > 0 && x >= X && y >= Y) {
                int col = (x - X) / cell, row = (y - Y) / cell, j = row * 8 + col;
                if (col >= 0 && col < 8 && j >= 0 && j < wd->ncolors) { wd_set_num(wd, j); fire_ev(i, GMB_EV_CHANGE); g_dirty = 1; }
            }
        } else if (wd->kind == W_TREE) {
            float rsc = GMB_GFX_RENDER_SCALE(); if (rsc <= 0) rsc = 1;
            (void)rsc; int rh = wd->style.row_h > 0 ? wd->style.row_h : 22; int tl2 = 14;
            if (type == GMB_GFX_MOUSE_DOWN && rh > 0 && x >= X && x < X + W && y >= Y && y < Y + H) {
                int j = tree_row_at(wd, y - Y, rh);
                if (j >= 0) {
                    int depth = tree_depth(wd->items[j]);
                    int mx = X + 6 + depth * (tl2 / 2 + 4);
                    if (tree_has_children(wd, j) && x >= mx - 2 && x < mx + tl2) {
                        wd->tcol[j] = !wd->tcol[j];
                        fire_ev(i, wd->tcol[j] ? GMB_EV_COLLAPSE : GMB_EV_EXPAND);
                        g_dirty = 1;
                    } else { wd_set_num(wd, j); fire_ev(i, GMB_EV_CHANGE); fire_ev(i, GMB_EV_NODECLICK); g_dirty = 1; }
                }
            }
        } else if (wd->kind == W_TEXT) {
            if (type == GMB_GFX_MOUSE_DOWN && x >= X && x < X + W && y >= Y && y < Y + H) {
                set_focus_wid(i);
                const char* tv = wd_str(wd);
                int rel = x - (X + 6), pos = 0, L = (int)strlen(tv);
                while (pos < L) { char ch[2] = { tv[pos], 0 }; int cw = (int)(GMB_GFX_TEXT_W(ch) / (sc > 0 ? sc : 1)); if (rel <= cw) break; rel -= cw; pos++; }
                wd->tcur = pos; g_dirty = 1;
            }
        } else if (wd->kind == W_FILE) {
            float rsc = GMB_GFX_RENDER_SCALE(); if (rsc <= 0) rsc = 1;
            int rh = (int)(GMB_GFX_TEXT_H() / rsc) + 4;
            if (type == GMB_GFX_MOUSE_DOWN && rh > 0 && x >= X && x < X + W && y >= Y && y < Y + H) {
                int vr = (y - Y) / rh, r = wd->bfirst + vr;
                if (r >= 0 && r < wd->nbig) {
                    wd->bsel = r;
                    { char path[300]; snprintf(path, sizeof(path), "%s/%s", wd->label, wd->big[r]); wd_set_str(wd, path); }
                    fire_ev(i, GMB_EV_CHANGE); g_dirty = 1;
                }
            }
        } else if (wd->kind == W_GRID) {
            float rsc = GMB_GFX_RENDER_SCALE(); if (rsc <= 0) rsc = 1;
            int rh = wd->style.row_h > 0 ? wd->style.row_h : 22;
            (void)rsc;
            if (type == GMB_GFX_MOUSE_DOWN && rh > 0 && x >= X && x < X + W && y >= Y && y < Y + H) {
                int rel = y - Y;
                if (rel < rh) { int c2 = wd->gnc > 0 ? (x - X) / (W / wd->gnc) : 0; if (c2 >= 0 && c2 < wd->gnc) grid_sort(wd, c2); }
                else { int vr = (rel - rh) / rh, di = wd->first + vr; if (di >= 0 && di < wd->gnr) { wd_set_num(wd, wd->gorder[di]); fire_ev(i, GMB_EV_CHANGE); fire_ev(i, GMB_EV_ROWCOLCHANGE); fire_ev(i, GMB_EV_SELCHANGE); g_dirty = 1; } }
            }
        } else if (wd->kind == W_DATE) {
            int listY = Y + H;
            int cs2 = (int)(GMB_GFX_RENDER_SCALE() > 0 ? GMB_GFX_RENDER_SCALE() : 1);
            int cc = (int)((GMB_GFX_TEXT_H() / (cs2 > 0 ? cs2 : 1)) + 6);
            if (type == GMB_GFX_MOUSE_DOWN) {
                if (wd->open) {
                    int prx = X, pry = listY, pw = 7 * cc, ph = 8 * cc;
                    if (x >= prx && x < prx + pw && y >= pry && y < pry + ph) {
                        int rel = y - pry;
                        if (rel < cc) {   /* header: << <  Mese/Anno  > >> (NON chiude) */
                            if (x < prx + cc) { wd->dy--; }
                            else if (x < prx + 2 * cc) { if (--wd->dm < 1) { wd->dm = 12; wd->dy--; } }
                            else if (x >= prx + pw - cc) { wd->dy++; }
                            else if (x >= prx + pw - 2 * cc) { if (++wd->dm > 12) { wd->dm = 1; wd->dy++; } }
                            g_dirty = 1;
                        } else if (rel >= 2 * cc) {   /* giorno: seleziona + chiude */
                            int r = (rel - 2 * cc) / cc, c2 = (x - prx) / cc;
                            int fw = date_first_wday(wd->dy, wd->dm), dim = date_dim(wd->dy, wd->dm);
                            int d = r * 7 + c2 - fw + 1;
                            if (d >= 1 && d <= dim) { wd->dday = d; wd->sy = wd->dy; wd->sm = wd->dm; wd->sd = d; char b[16]; snprintf(b, sizeof(b), "%04d-%02d-%02d", wd->dy, wd->dm, d); wd_set_str(wd, b); }
                            wd->open = 0; g_dirty = 1;
                        }
                    } else { wd->open = 0; g_dirty = 1; }
                } else if (x >= X && x < X + W && y >= Y && y < Y + H) { if (x >= X + W - px_arrow_band_w((int)GMB_GFX_TEXT_END_X("M",0,0))) { wd->open = 1; wd->hover = 0; } else { set_focus_wid(i); date_mask_fill(wd); } g_dirty = 1; }
            }
        } else if (wd->kind == W_DROPDOWN) {
            int listY = Y + H, listH = H * wd->nitems;
            if (type == GMB_GFX_MOUSE_DOWN) {
                if (wd->open) {   /* qualunque click chiude; se su una voce, la seleziona */
                    if (wd->nitems && x >= X && x < X + W && y >= listY && y < listY + listH) {
                        int row = (y - listY) / H; if (row >= 0 && row < wd->nitems) { wd_set_num(wd, row); fire_ev(i, GMB_EV_CHANGE); }
                    }
                    wd->open = 0; g_dirty = 1;
                } else if (x >= X && x < X + W && y >= Y && y < Y + H) {
                    wd->open = 1; wd->hover = -1; fire_ev(i, GMB_EV_DROPDOWN); g_dirty = 1;
                }
            } else if (type == GMB_GFX_MOUSE_MOVE && wd->open) {
                if (x >= X && x < X + W && y >= listY && y < listY + listH) {
                    int row = (y - listY) / H; if (row != wd->hover) { wd->hover = row; g_dirty = 1; }
                }
            }
        }
    }
    if (type == GMB_GFX_MOUSE_UP) { g_down_wid = -1; g_dragging = 0; }
}

int gmb_form_active(void) { return (g_nwid > 0 || g_nbtn > 0) ? 1 : 0; }
int gmb_form_last_handle(void) { return g_last_handle; }
int gmb_form_last_is_tui(void) { return g_last_is_tui; }
void gmb_form_set_last_handle(int h) { g_last_handle = h; }

/* Dispatch di UN evento mouse (stessa logica del loop di gmb_form_poll).
 * Serve quando l'host (C# IL/PCode) drena lui la coda input: deve poter
 * consegnare l'evento al layer controlli C evento per evento. */
void gmb_form_dispatch_mouse_px(int type, int x, int y, int button, int wheel)
{
    float sc = GMB_GFX_RENDER_SCALE();
    if (sc <= 0.0f) sc = 1.0f;
    if (type == GMB_GFX_MOUSE_DOWN) {   /* focus generico (base Control) */
        for (int fi = g_nwid - 1; fi >= 0; fi--) {
            gmb_pix_widget_t* fw = &g_wid[fi];
            if (!fw->base.visible || !fw->base.enabled) continue;
            if (fw->kind == W_PROGRESS || fw->kind == W_STATUSBAR) continue;
            int fx, fy, fwd, fht; form_control_px_rect(&fw->base, &fx, &fy, &fwd, &fht);
            if (x >= fx && x < fx + fwd && y >= fy && y < fy + fht) { set_focus_wid(fi); break; }
        }
    }
    if (type == GMB_GFX_MOUSE_DOWN && g_focus_wid >= 0 && g_focus_wid < g_nwid && g_wid[g_focus_wid].kind == W_DATE) {
        gmb_pix_widget_t* fd = &g_wid[g_focus_wid];
        int inside = (x >= fd->base.x && x < fd->base.x + fd->base.width && y >= fd->base.y && y < fd->base.y + fd->base.height);
        if (!inside) { if (!date_mask_valid(fd)) { date_mask_fill(fd); } else { date_from_mask(fd); } set_focus_wid(-1); g_dirty = 1; }
    }
    for (int i = 0; i < g_nbtn; i++) {
        gmb_form_btn_t* b = &g_btns[i];
        if (!b->base.visible) continue;
        int bx, by, bw, bh; form_control_px_rect(&b->base, &bx, &by, &bw, &bh);
        int hit = b->base.enabled && (x >= bx && x < bx + bw && y >= by && y < by + bh);
        if (type == GMB_GFX_MOUSE_MOVE) {
            if (b->hover != hit) { b->hover = hit; g_dirty = 1; }
        } else if (type == GMB_GFX_MOUSE_DOWN && hit) {
            if (!b->pressed) { b->pressed = 1; g_dirty = 1; }
        } else if (type == GMB_GFX_MOUSE_UP) {
            int was = b->pressed;
            b->pressed = 0;
            if (was) g_dirty = 1;
            /* Fire INLINE sul task GPM corrente (mai su thread esterni). */
            if (was && hit) { b->base.last_event = GMB_EV_CLICK; g_last_mouse_event = GMB_EV_CLICK; gmb_form_cb _h = b->base.ev_cb[GMB_EV_CLICK] ? b->base.ev_cb[GMB_EV_CLICK] : b->cb; if (_h) _h(); }
        }
    }
    widget_mouse(type, x, y, sc);
    { extern void gmb_tui_dispatch_mouse_px(int, int, int, int, int);
      gmb_tui_dispatch_mouse_px(type, x, y, button, wheel); }
}

/* Dispatch di UN tasto al layer controlli (stessa logica del loop di gmb_form_poll).
 * L'host C# drena la coda input: consegna i tasti qui evento per evento, così il
 * controllo focalizzato (textbox/datepicker) e i registri TUI ricevono i tasti. */
void gmb_form_focus_widget(int i) { set_focus_wid(i); g_dirty = 1; }
int gmb_form_focused_widget(void) { return g_focus_wid; }

int gmb_form_widget_rect(int i, int* x, int* y, int* w, int* h)
{
    if (i < 0 || i >= g_nwid) return 0;
    if (x) *x = g_wid[i].base.x; if (y) *y = g_wid[i].base.y;
    if (w) *w = g_wid[i].base.width; if (h) *h = g_wid[i].base.height;
    return 1;
}

void gmb_form_dispatch_key(const char* key)
{
    if (!key || !key[0]) return;
    unsigned char c = (unsigned char)key[0];
    if (g_focus_wid >= 0 && g_focus_wid < g_nwid) {   /* eventi tastiera (VB6) sul controllo focalizzato */
        fire_ev(g_focus_wid, GMB_EV_KEYDOWN);
        fire_ev(g_focus_wid, GMB_EV_KEYPRESS);
        fire_ev(g_focus_wid, GMB_EV_KEYUP);
    }
    if (g_focus_wid >= 0 && g_focus_wid < g_nwid && g_wid[g_focus_wid].kind == W_TEXT) {
        gmb_pix_widget_t* wd = &g_wid[g_focus_wid];
        char buf[256]; snprintf(buf, sizeof(buf), "%s", wd_str(wd));
        int L = (int)strlen(buf);
        if (c == 0x1b) { set_focus_wid(-1); }
        else if (c == '\r' || c == '\n') { set_focus_wid(-1); }
        else if (c == 0x1c) { if (wd->tcur > 0) wd->tcur--; }
        else if (c == 0x1d) { if (wd->tcur < L) wd->tcur++; }
        else if (c == 0x1e) { wd->tcur = 0; }
        else if (c == 0x1f) { wd->tcur = L; }
        else if (c == '\b' || c == 0x7f) { if (wd->tcur > 0) { memmove(&buf[wd->tcur - 1], &buf[wd->tcur], (size_t)(L - wd->tcur + 1)); wd->tcur--; wd_set_str(wd, buf); } }
        else if (c >= 32 && L < 250) { if (wd->tcur > L) wd->tcur = L; memmove(&buf[wd->tcur + 1], &buf[wd->tcur], (size_t)(L - wd->tcur + 1)); buf[wd->tcur] = (char)c; wd->tcur++; wd_set_str(wd, buf); }
        g_dirty = 1;
    }
    else if (g_focus_wid >= 0 && g_focus_wid < g_nwid) {
        /* Tastiera sui widget "a valore" (spinner/check/listbox/track): frecce e spazio. */
        gmb_pix_widget_t* wd = &g_wid[g_focus_wid];
        if (wd->kind == W_SPINNER) {
            if (c == 0x1e || c == '+') { wd_add_num(wd, wd->vstep > 0 ? wd->vstep : 1); }
            else if (c == 0x1f || c == '-') { wd_add_num(wd, -(wd->vstep > 0 ? wd->vstep : 1)); }
            else return;
            if (wd->vmax > wd->vmin) { double vv = wd_num(wd); if (vv < wd->vmin) vv = wd->vmin; if (vv > wd->vmax) vv = wd->vmax; wd_set_num(wd, vv); }
            g_dirty = 1;
        } else if (wd->kind == W_CHECK || wd->kind == W_TOGGLE) {
            if (c == ' ') { wd_set_num(wd, wd_num(wd) != 0 ? 0 : 1); g_dirty = 1; }
        } else if (wd->kind == W_DROPDOWN) {
            /* SELECT da tastiera (prima non gestito: la freccia giu' non faceva nulla).
             * Chiuso: Giu'/Invio/Spazio aprono. Aperto: Su/Giu' spostano l'evidenziazione,
             * Invio conferma e chiude, Esc chiude senza cambiare. */
            int j;
            if (!wd->open) {
                if (!wd->nitems) return;
                if (c == 0x1f || c == '\r' || c == '\n' || c == ' ') {
                    wd->open = 1;
                    j = (int)(wd_num(wd) + 0.5);
                    if (j < 0) j = 0; if (j >= wd->nitems) j = wd->nitems - 1;
                    wd->hover = j;
                    fire_ev(g_focus_wid, GMB_EV_DROPDOWN);
                    g_dirty = 1;
                }
                return;
            }
            j = wd->hover;
            if (j < 0) j = (int)(wd_num(wd) + 0.5);
            if (c == 0x1e) { if (j > 0) j--; }
            else if (c == 0x1f) { if (j < wd->nitems - 1) j++; }
            else if (c == '\r' || c == '\n') {
                wd_set_num(wd, j);
                wd->open = 0;
                fire_ev(g_focus_wid, GMB_EV_CHANGE);
                g_dirty = 1;
                return;
            } else if (c == 0x1b) {
                wd->open = 0; g_dirty = 1;
                return;   /* Esc chiude il popup, non la finestra */
            } else return;
            wd->hover = j;
            g_dirty = 1;
            return;
        } else if (wd->kind == W_LISTBOX) {
            int j = (int)(wd_num(wd) + 0.5);
            if (c == 0x1e) j--; else if (c == 0x1f) j++; else return;
            if (j < 0) j = 0; if (j >= wd->nitems) j = wd->nitems - 1;
            wd_set_num(wd, j); g_dirty = 1;
        } else if (wd->kind == W_TRACK) {
            double st = (wd->vmax - wd->vmin) / 20.0; if (st <= 0) st = 1;
            if (c == 0x1d) wd_set_num(wd, wd_num(wd) + st);
            else if (c == 0x1c) wd_set_num(wd, wd_num(wd) - st);
            else return;
            g_dirty = 1;
        }
    }
    else if (g_focus_wid >= 0 && g_focus_wid < g_nwid && g_wid[g_focus_wid].kind == W_DATE) {
        gmb_pix_widget_t* wd = &g_wid[g_focus_wid];
        if (c == 0x1b) { set_focus_wid(-1); g_dirty = 1; return; }
        int L = (int)strlen(wd->dmask);
        if (c == '\r' || c == '\n') { if (date_mask_valid(wd)) { date_from_mask(wd); set_focus_wid(-1); } else { wd->open = 0; } g_dirty = 1; }
        else if (c == 0x1c || c == 0x1d) {
            int p = wd->dcur + (c == 0x1d ? 1 : -1);
            if (c == 0x1c) p = wd->dcur - 1;
            while (p >= 0 && p < L && wd->dfmt[p] != 'd' && wd->dfmt[p] != 'm' && wd->dfmt[p] != 'y') p += (c == 0x1d ? 1 : -1);
            if (p >= 0 && p < L) wd->dcur = (c == 0x1d ? p + 1 : p);
            g_dirty = 1;
        }
        else if (c == '\b') {
            int p = wd->dcur - 1;
            while (p >= 0 && wd->dfmt[p] != 'd' && wd->dfmt[p] != 'm' && wd->dfmt[p] != 'y') p--;
            if (p >= 0) { wd->dmask[p] = '_'; wd->dcur = p; }
            g_dirty = 1;
        }
        else if (c >= '0' && c <= '9') {
            int p = wd->dcur;
            while (p < L && wd->dfmt[p] != 'd' && wd->dfmt[p] != 'm' && wd->dfmt[p] != 'y') p++;
            if (p < L) { wd->dmask[p] = (char)c; wd->dcur = p + 1; g_dirty = 1; if (date_mask_complete(wd->dmask)) date_from_mask(wd); }
        }
    }
    if (c == 0x1b) { gmb_form_request_close(); }
    /* Stesso tasto anche ai controlli TUI focalizzati (fkeybar/menu/liste cell-based). */
    { extern void gmb_tui_push_gfx_key(int);
      int k = 0;
      if (c == 0x1b) k = 27; else if (c == '\r' || c == '\n') k = 13; else if (c == '\b') k = 127;
      else if (c == '\t') k = 9; else if (c == 0x1c) k = 19; else if (c == 0x1d) k = 4;
      else if (c == 0x1e) k = 5; else if (c == 0x1f) k = 24; else if (c >= 32) k = c;
      if (k) gmb_tui_push_gfx_key(k);
    }
}

void gmb_form_poll(void)
{
    /* SOLO eventi: legge l'input e dispatcha le callback. NON disegna: il render
     * e' sul main thread (gmb_form_render_if_dirty), quando lo stato e' cambiato
     * (g_dirty). Cosi' il goroutine eventi non fa repaint. */
    float sc = GMB_GFX_RENDER_SCALE();
    if (sc <= 0.0f) sc = 1.0f;

    int type, x, y, button, wheel;
    while (gmb_form_poll_mouse(&type, &x, &y, &button, &wheel))
        gmb_form_dispatch_mouse_px(type, x, y, button, wheel);
    {   /* tastiera: edit inline (del controllo focalizzato) */
        char key[32];
        while (GMB_GFX_POLL_KEY(key, (int)sizeof(key)))
        {
            /* In modalita' READ i tasti di CONTROLLO (Enter/ESC/Tab) sono del loop
             * READ: qui si accodano invece di essere consumati/dispatchati. */
            if (g_read_mode) {
                char c0 = key[0];
                if (c0 == '\r' || c0 == '\n' || c0 == 0x1b || c0 == '\t') { g_read_pending_key = (unsigned char)c0; continue; }
            }
            {   /* un chunk (SDL_TEXTINPUT/AUTOKEY) puo' contenere piu' caratteri */
                for (const char* q = key; *q; q++) { char one[2] = { *q, 0 }; gmb_form_dispatch_key(one); }
            }
        }
    }

    if (!GMB_GFX_SCREEN_ON()) gmb_tui_screen_flush();
}

/* Render dei bottoni sulla pagina grafica: da chiamare SUL MAIN THREAD (dove SDL e
 * la grafica sono leciti), tipicamente dal present. Ridisegna solo se dirty. La
 * finestra/renderScale si crea al primo present, DOPO il primo disegno (a scala 1):
 * quando la scala cambia si ripulisce la pagina una volta (evita la doppia immagine). */
void gmb_form_render_if_dirty(void)
{
    if (g_form_exclusive) return;   /* READ (form Clipper) disegna da sé: non cancellare */
    /* Contatore frame di present (per lo shot di verifica), indipendente dal dirty. */
    static int sframes = 0;
    const char* sp = gmb_opt_env("GMB_FORM_SHOT");
    const char* fr = gmb_opt_env("GMB_FORM_SHOT_FRAME");
    int want = (fr && *fr) ? atoi(fr) : 20;
    sframes++;

    if (!GMB_GFX_SCREEN_ON()) return;
    static float s_last_scale = -1.0f;
    float sc = GMB_GFX_RENDER_SCALE();
    if (sc <= 0.0f) sc = 1.0f;
    if (sc != s_last_scale)
    {
        /* Ripulisce SOLO con un FORM attivo (che poi si ridisegna da sé). In grafica
         * pura il primo present NON deve azzerare la pagina: SLEEP/WAITKEY prima di
         * SAVE SCREEN cancellavano tutto il disegno (bug GraphicsCellPixel). */
        if (gmb_form_active()) {
            /* Pagina = sfondo del tema (non nero): il form e una finestra. */
            gmb_widget_style_t st = gmb_widget_style_current();
            GMB_GFX_FILL_RRECT(0, 0, GMB_GFX_SCREEN_W(), GMB_GFX_SCREEN_H(), 0, st.body);
            g_dirty = 1;
        }
        s_last_scale = sc;
    }
    if (gmb_form_active() && g_dirty) { form_draw(); g_dirty = 0; }
    { extern void gmb_tui_draw_px_all(void); gmb_tui_draw_px_all(); }

    if (sp && *sp && sframes == want) GMB_GFX_SAVE_PNG(sp);
}

static void form_park(int ms)
{
    if (ms <= 0) return;
    /* Park GPM: dentro un task il sleep cooperativo (gmb_yield_ticks) libera il
     * worker senza spin; fuori da un task (main thread) breve attesa reale. */
    if (gmb_cur()) gmb_yield_ticks(ms);
    else gmb_sleep_ms(ms);
}

/* Iniezione tasti per i test (headless-safe): GMB_FORM_AUTOKEY="frame,ESC;f2,testo".
 * Gira FUORI dal present SDL, cosi' i frame avanzano anche senza finestra (READ). */
void gmb_form_autokey_tick(void)
{
    static long k_frame = 0;
    const char* ak = gmb_opt_env("GMB_FORM_AUTOKEY");
    if (ak && *ak)
    {
        char buf[256]; snprintf(buf, sizeof(buf), "%s", ak);
        char* sv = NULL;
        for (char* it = strtok_r(buf, ";", &sv); it; it = strtok_r(NULL, ";", &sv))
        {
            char spec[64] = ""; int f = 0;
            char* comma = strchr(it, ',');
            if (!comma) continue;
            f = atoi(it);
            snprintf(spec, sizeof(spec), "%s", comma + 1);
            if (k_frame == (long)f)
            {
#ifdef GMB_WITH_UI
                if (!strcasecmp(spec, "ESC")) gmb_gfx_input_push_key("\x1b");
                else if (!strcasecmp(spec, "ENTER")) gmb_gfx_input_push_key("\r");
                else if (!strcasecmp(spec, "TAB")) gmb_gfx_input_push_key("\t");
                else gmb_gfx_input_push_key(spec);
#else
                gmb_ui_missing_warn();
#endif
            }
        }
    }
    k_frame++;
}

void gmb_form_frame(int frame_ms)
{
    gmb_form_autokey_tick();
    gmb_form_poll();
    form_park(frame_ms);
}

/* ---- WindowBase (analogo della Form di VB): base di TUTTE le finestre/dialog ---- */
void gmb_form_win_init(gmb_form_win_t* w)
{
    if (!w) return;
    memset(w, 0, sizeof(*w));
    w->visible = 1; w->enabled = 1; w->modal = 1;
}

void gmb_form_win_on(gmb_form_win_t* w, int ev, gmb_form_cb cb)
{
    if (w && ev > 0 && ev < GMB_EV_COUNT) w->ev[ev] = cb;
}

static void win_fire(gmb_form_win_t* w, int ev)
{
    if (w && ev > 0 && ev < GMB_EV_COUNT && w->ev[ev]) w->ev[ev]();
}

/* Loop comune: fira Load/Activate; per ogni frame disegna, fira Paint, drena mouse
 * (MouseDown/Up/Move -> evcb) e tastiera (KeyPress -> evcb; ESC chiude); park GPM;
 * all'uscita fira Unload. Ritorna quando evcb ritorna 1, ESC, o chiusura richiesta. */
void gmb_form_win_run(gmb_form_win_t* w, void (*draw)(void* ctx),
                      int (*evcb)(gmb_form_win_t* w, int ev, int x, int y, void* ctx),
                      void* ctx, int frame_ms)
{
    if (!w || !w->visible) return;
    win_fire(w, GMB_EV_LOAD);
    win_fire(w, GMB_EV_ACTIVATE);
    int closed = 0;
    while (!closed && !gmb_form_closed()) {
        if (draw) draw(ctx);
        win_fire(w, GMB_EV_PAINT);
        int type, x, y, button, wheel;
        while (gmb_form_poll_mouse(&type, &x, &y, &button, &wheel)) {
            int ev = 0;
            if (type == GMB_GFX_MOUSE_DOWN) ev = GMB_EV_MOUSEDOWN;
            else if (type == GMB_GFX_MOUSE_UP) ev = GMB_EV_MOUSEUP;
            else if (type == GMB_GFX_MOUSE_MOVE) ev = GMB_EV_MOUSEMOVE;
            if (ev && evcb && evcb(w, ev, x, y, ctx)) { closed = 1; break; }
        }
        char key[32];
        while (!closed && GMB_GFX_POLL_KEY(key, (int)sizeof(key))) {
            if (evcb) { for (const char* kp = key; *kp; kp++) if (evcb(w, GMB_EV_KEYPRESS, (int)(unsigned char)*kp, 0, ctx)) { closed = 1; break; } }
            else if ((unsigned char)key[0] == 0x1b) { closed = 1; break; }
        }
        if (!GMB_GFX_SCREEN_ON()) gmb_tui_screen_flush();
        form_park(frame_ms);
    }
    win_fire(w, GMB_EV_UNLOAD);
}
