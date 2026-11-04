/* ============================================================================
 * gmb_tui_widgets.h — framework widget Controls/ (specchio ClipperLib/Controls:
 * Theme.cs, ControlStyle.cs, Control.cs, FocusManager.cs, EventHandler.cs,
 * Damage.cs). Base + dispatch eventi. TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_WIDGETS_H
#define GMB_TUI_WIDGETS_H

#include "gmb_tui.h"
#include "gmb_tui_mouse.h"
#include "gmb_tui_console.h"
#include "gmb_tui_clipper.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_CTRL_MAX_ID 64
#define GMB_CTRL_MAX_ENTRIES 32

/* Helper condivisi tra i moduli widget (ex statici del monolite Controls). */
static inline gmb_cell_color_t crgb(uint8_t r, uint8_t g, uint8_t b)
{
    gmb_cell_color_t c = { r, g, b, 0 };
    return c;
}
static inline bool rect_contains(gmb_rect_t r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* ---- Theme (specchio Theme.cs) ---- */
typedef struct gmb_theme {
    gmb_cell_color_t desktop_bg, window_bg, title_focus, title_blur, title_fg, border,
                     ctrl_fg, close_fg, text, dim, accent, active_bg, shadow;
    /* estensioni (0,0,0 = deriva dai colori base) */
    gmb_cell_color_t cal_today_bg, cal_today_fg, cal_sel_bg, cal_sel_fg;
    gmb_cell_color_t head_bg, head_fg;
    /* Palette PER-VISTA (Turbo Vision): ogni tipo di vista ha i suoi colori.
     * 0,0,0 = deriva dai colori base (window_bg/text). */
    gmb_cell_color_t editor_bg, editor_fg, editor_frame; /* editor: blu/bianco/cornice ciano */
    gmb_cell_color_t menu_bg, menu_fg;                   /* menu bar/dropdown: grigio/nero */
    gmb_cell_color_t dlg_bg, dlg_fg;                     /* dialog: grigio/nero */
    gmb_cell_color_t stat_bg, stat_fg;                   /* statusline: ciano/nero */
    const char* desktop_pattern;   /* UTF-8: "░" / "·" / " " (Theme.cs DesktopPattern) */
} gmb_theme_t;

/* Colori per-vista dal tema. kind: 0=editor, 1=menu, 2=dialog, 3=status. */
void gmb_theme_kind(const gmb_theme_t* t, int kind, gmb_cell_color_t* fg, gmb_cell_color_t* bg);

gmb_theme_t gmb_theme_turbo(void);
gmb_theme_t gmb_theme_qbasic(void);
gmb_theme_t gmb_theme_midnight(void);
gmb_theme_t gmb_theme_nord(void);
gmb_theme_t gmb_theme_light(void);
gmb_theme_t gmb_theme_soft(void);
gmb_theme_t gmb_theme_neon(void);
gmb_theme_t gmb_theme_glass(void);
gmb_theme_t gmb_theme_dracula(void);
gmb_theme_t gmb_theme_monokai(void);
gmb_theme_t gmb_theme_solarized_dark(void);
gmb_theme_t gmb_theme_retro_green(void);
gmb_theme_t gmb_theme_amber(void);
gmb_theme_t gmb_theme_vb6(void);
gmb_theme_t gmb_theme_named(const char* name);
gmb_theme_t gmb_theme_default(void);

/* ---- Stile widget PIXEL (deriva i default dal tema corrente) ---- */
/* Colori in 0xAARRGGBB. shape: 0=flat, 1=rounded. Il tema sceglie la "caratterialita'"
 * (i temi classici Turbo/QBasic/RetroGreen/Amber -> flat+bordo; i moderni -> rounded+ombra). */
typedef struct gmb_widget_style {
    int shape;                 /* 0=flat, 1=rounded */
    int radius;
    unsigned int fill, fill_hover, fill_press, fill_disabled;
    unsigned int border; int border_w;
    unsigned int text, text_disabled;
    unsigned int shadow; int shadow_on, shadow_dx, shadow_dy, shadow_soft;
    unsigned int focus_bg, focus_fg;   /* colori del controllo FOCUS (come i TUI) */
    unsigned int accent;               /* accento del tema */
    unsigned int today_bg, today_fg, sel_bg, sel_fg;   /* calendario (tema, overridabile) */
    unsigned int head_bg, head_fg;     /* intestazioni (grid/menu) */
    int row_h;                         /* altezza riga (0=auto dal font) */
    int pad;                           /* padding celle (0=default) */
    int font_size;             /* 0 = font corrente */
    char font_path[160];       /* "" = font corrente */
    int pad_x, pad_y;
    /* effetti avanzati */
    int bevel;                 /* 0=none, 1=raised (3D), -1=sunken */
    int bevel_depth;           /* spessore bordo 3D (px) */
    unsigned int bevel_light, bevel_dark;
    int emboss;                /* testo embossed/inciso */
    int gradient_on; unsigned int grad_top, grad_bottom;
    int glow_on; unsigned int glow;
    /* cornice finestra (per dialog/alert box) */
    unsigned int title_bg, title_fg, body;
    /* skin: 0=soft(light), 1=neon(light), 2=glass(dark); gradienti accento */
    int skin;
    unsigned int grad_a, grad_b;
} gmb_widget_style_t;

void gmb_ui_set_theme(const gmb_theme_t* t);
void gmb_ui_set_theme_name(const char* name);
gmb_theme_t gmb_ui_theme(void);
const char* gmb_ui_theme_name(void);
int gmb_ui_theme_is_set(void);
gmb_widget_style_t gmb_widget_style_from_theme(const gmb_theme_t* t);
gmb_widget_style_t gmb_widget_style_current(void);

/* ---- ControlStyle (specchio ControlStyle.cs) ---- */
typedef struct gmb_control_style {
    bool has_fg, has_bg, has_fg_focus, has_bg_focus, has_fg_error, has_bg_error, has_sel_fg, has_sel_bg;
    gmb_cell_color_t fg, bg, fg_focus, bg_focus, fg_error, bg_error, sel_fg, sel_bg;
    char fill_char;
} gmb_control_style_t;

void gmb_control_style_init(gmb_control_style_t* s);
gmb_control_style_t gmb_control_style_default(void);
gmb_control_style_t gmb_control_style_from_theme(const gmb_theme_t* t);
gmb_control_style_t gmb_control_style_clone(const gmb_control_style_t* s);
void gmb_control_style_set_theme(const gmb_theme_t* t);
gmb_control_style_t gmb_control_style_global_default(void);
void gmb_control_style_with_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg);
void gmb_control_style_with_focus_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg);
void gmb_control_style_with_error_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg);
void gmb_control_style_with_selected_colors(gmb_control_style_t* s, gmb_cell_color_t fg, gmb_cell_color_t bg);
void gmb_control_style_with_fill(gmb_control_style_t* s, char fill);
void gmb_control_style_effective(const gmb_control_style_t* s, bool focused, bool error, bool selected,
                                 gmb_cell_color_t* fg, gmb_cell_color_t* bg);

/* ---- Window (radice): il TIPO della finestra radice decide il render dei figli. ---- */
typedef enum {
    GMB_WIN_SYSTEM = 0,   /* SystemConsole (stdout/stdin testo) */
    GMB_WIN_STANDARD,     /* StandardConsole (ANSI/testo) */
    GMB_WIN_SDL,          /* SdlConsole (finestra testo SDL) */
    GMB_WIN_GRAPHICS      /* Graphics (pixel) */
} gmb_window_kind_t;

typedef struct gmb_base_window {
    gmb_window_kind_t kind;
    int handle;                       /* handle della finestra principale */
    struct gmb_base_window* parent;   /* catena dei parent fino alla radice */
} gmb_base_window_t;

/* Kind della finestra radice attiva (decisa dal backend/console, non da un flag SCREEN). */
gmb_window_kind_t gmb_base_window_kind(void);
/* Applica le clausole proprieta' a un controllo per handle (bitmask: NAME=1 TAG=2 TOOLTIP=4
 * TABINDEX=8 VISIBLE=16 ENABLED=32 TABSTOP=64). Protegge le stringhe. */
void gmb_control_apply_props_handle(int handle, unsigned flags, const char* name, const char* tag, const char* tooltip,
                                    int tabindex, int visible, int enabled, int tabstop);
/* Applica una spec stile CSS-like (fg=#RRGGBB bg=#RRGGBB uppercase ...) a un controllo per handle. */
void gmb_control_apply_style_handle(int handle, const char* spec);
/* Finestra radice globale (default) a cui si agganciano i controlli. */
gmb_base_window_t* gmb_base_window_root(void);
/* Il backend attivo (console/screen) setta kind+handle della finestra principale. */
void gmb_base_window_set_kind(gmb_window_kind_t kind);
void gmb_base_window_set_handle(int handle);
/* Aggiorna la proprieta' nascosta (window_kind/handle) dei controlli gia' registrati. */
void gmb_tui_refresh_window(void);

/* ---- Control kinds (per accessor generici CONTROL SET/GET) ---- */
enum { GMB_CK_NONE=0, GMB_CK_CHECKBOX, GMB_CK_TOGGLE, GMB_CK_RADIO, GMB_CK_SLIDER,
       GMB_CK_COMBOBOX, GMB_CK_LISTBOX, GMB_CK_SPINNER, GMB_CK_TAB, GMB_CK_GRID,
       GMB_CK_TREE, GMB_CK_COLORPICKER, GMB_CK_DATEPICKER, GMB_CK_STATUSBAR,
       GMB_CK_NUMERICUPDOWN, GMB_CK_STARRATING, GMB_CK_SCROLLBAR, GMB_CK_SCROLLVIEW,
       GMB_CK_LABEL, GMB_CK_DATALIST, GMB_CK_PROPERTYGRID, GMB_CK_LOGVIEW, GMB_CK_HEXVIEW,
       GMB_CK_BIGTEXT, GMB_CK_CODEAREA, GMB_CK_REORDERLISTBOX, GMB_CK_ALERTBOX,
       GMB_CK_CONFIRMATIONBOX, GMB_CK_FILEEXPLORER, GMB_CK_DASHBOARD, GMB_CK_TBROWSE,
       GMB_CK_MENU, GMB_CK_FILEDIALOG, GMB_CK_PRINTERDIALOG, GMB_CK_TEXTBOX,
       /* Solo-grafica (famiglia unica gmb_control_t; no-op in TEXT). */
       GMB_CK_CANVAS, GMB_CK_IMAGE, GMB_CK_IMAGEBUTTON, GMB_CK_QR,
       GMB_CK_RICHTEXT, GMB_CK_HALFBLOCK, GMB_CK_TILING, GMB_CK_USERCONTROL,
       GMB_CK_FKEYBAR, GMB_CK_CANDLESTICK, GMB_CK_CHART,
       GMB_CK_SPLITTER, GMB_CK_CONTEXTMENU };

/* ---- Control (specchio Control.cs, base) ---- */
#define GMB_CTRL_MAX_CONTROLS 64
#define GMB_TUI_EV_COUNT 40
typedef double (*gmb_tui_cb)(void);   /* callback evento controllo (unico, ritorna double come gmb_form_cb) */

typedef struct gmb_control {
    char id[GMB_CTRL_MAX_ID];
    char name[64], tag[64], tooltip[128];   /* clausole proprieta' (NAME/TAG/TOOLTIP) */
    int x, y, width, height;
    int px_x, px_y, px_w, px_h;     /* rect PIXEL (graphics): settato dal render, usato da hit-test/click */
    bool focused, visible, enabled, tab_stop, uppercase, has_error;
    int tab_index;
    gmb_control_style_t style;
    void* user;                     /* dati specifici del widget (sottoclasse) */
    void (*draw_fn)(void* user, struct gmb_control* c, gmb_screen_buffer_t* buf);
    void (*draw_px_fn)(void* user, struct gmb_control* c);  /* render pixel ricco (GRAPHICS) */
    bool (*handle_key_fn)(void* user, struct gmb_control* c, int key);
    bool (*handle_click_fn)(void* user, struct gmb_control* c, int x, int y);
    bool (*handle_mouse_fn)(void* user, struct gmb_control* c, const gmb_mouse_action_t* a);
    /* Area d hit ESTESA (popup/dropdown aperti): se true, l evento va a questo controllo. */
    bool (*hit_extra_fn)(void* user, struct gmb_control* c, int x, int y);
    /* base Control: eventi (VB-like) + polling di stato + handle univoco. */
    int ctrl_kind;                  /* kind controllo (GMB_CK_*) per accessor generici */
    gmb_base_window_t* window;      /* parent fino alla radice (per la gerarchia) */
    int window_handle;              /* [nascosta] handle finestra principale, da creation */
    gmb_window_kind_t window_kind;  /* [nascosta] tipo finestra, da creation: decide il render */
    gmb_tui_cb ev_cb[GMB_TUI_EV_COUNT];
    int last_event;
    int handle;
    double* bind_value;   /* INTO del BASIC: valore sincronizzato ad ogni frame TUI */
    char container[64];   /* contenitore ospite (window/tab/panel/usercontrol); "" = radice/corrente */
    /* Proprietà VB6 non ancora specializzate dal widget (Font, Locked, Appearance,
       MousePointer, RightToLeft, proprietà Data/DataGrid/CommonDialog, ...).
       Restano leggibili/scrivibili via CONTROL SET/GET/GETS e vengono esportate
       dall'IDE senza perdita. */
    char extra_keys[24][40];
    char extra_values[24][96];
    int extra_count;
} gmb_control_t;

void gmb_control_apply_style(gmb_control_t* c, const char* spec);

void gmb_control_init(gmb_control_t* c, const char* id, int x, int y, int w, int h,
                      void (*draw)(void*, gmb_control_t*, gmb_screen_buffer_t*),
                      bool (*key)(void*, gmb_control_t*, int));
gmb_rect_t gmb_control_rect(const gmb_control_t* c);
void gmb_control_draw(gmb_control_t* c, gmb_screen_buffer_t* buf);
/* Disegna una stringa con evidenziazione Borland: lettera tra ~X~ oppure token di tasto
 * (F1..F12 / Alt-X / Ctrl-X / Shift-X) in `hk` (rosso). maxw = larghezza max. */
void gmb_draw_string_keys(gmb_screen_buffer_t* buf, int x, int y, const char* s, int maxw,
                          gmb_cell_color_t fg, gmb_cell_color_t bg, gmb_cell_color_t hk);
bool gmb_control_handle_key(gmb_control_t* c, int key);
bool gmb_control_handle_mouse(gmb_control_t* c, const gmb_mouse_action_t* a);
bool gmb_control_handle_input(gmb_control_t* c, int key);
int  gmb_control_apply_case(gmb_control_t* c, const char* s, char* out, int cap);

/* ---- Controlli utente (UserControl, stile VB6) ----
 * Il metodo BASIC Paint gira sul worker GPM e accumula una DRAW-LIST (sink); il main thread
 * la replaya con le primitive gmb_gfx (ADR-Controlli-Utente-Modello-Classe-VB6, §7).
 * Coordinate del contesto = LOCALI al controllo (0..width/0..height). */
enum { GMB_GCMD_CLEAR=1, GMB_GCMD_SETCOLOR, GMB_GCMD_LINE, GMB_GCMD_RECT, GMB_GCMD_FILLRECT,
       GMB_GCMD_CIRCLE, GMB_GCMD_FILLCIRCLE, GMB_GCMD_TEXT, GMB_GCMD_PLOT };
typedef struct gmb_gfx_cmd {
    int op, a, b, c, d, e; unsigned color; char text[64];
} gmb_gfx_cmd_t;
#define GMB_GFXCTX_MAX_CMDS 512
typedef struct gmb_gfx_context {
    int x, y, width, height;        /* rettangolo del controllo sullo schermo */
    unsigned fg, bg, cur; int focused;
    gmb_gfx_cmd_t cmds[GMB_GFXCTX_MAX_CMDS]; int ncmds;
} gmb_gfx_context_t;
void gmb_gfxctx_begin(gmb_gfx_context_t* g, int x, int y, int w, int h, unsigned fg, unsigned bg, int focused);
void gmb_gfxctx_clear(gmb_gfx_context_t* g);
void gmb_gfxctx_setcolor(gmb_gfx_context_t* g, unsigned color);
void gmb_gfxctx_line(gmb_gfx_context_t* g, int x1, int y1, int x2, int y2);
void gmb_gfxctx_rect(gmb_gfx_context_t* g, int x, int y, int w, int h);
void gmb_gfxctx_fillrect(gmb_gfx_context_t* g, int x, int y, int w, int h);
void gmb_gfxctx_circle(gmb_gfx_context_t* g, int cx, int cy, int r);
void gmb_gfxctx_fillcircle(gmb_gfx_context_t* g, int cx, int cy, int r);
void gmb_gfxctx_text(gmb_gfx_context_t* g, const char* s, int x, int y);
void gmb_gfxctx_plot(gmb_gfx_context_t* g, int x, int y);
void gmb_gfxctx_replay(const gmb_gfx_context_t* g);   /* chiama le gmb_gfx_* reali (main thread) */

typedef struct gmb_usercontrol {
    gmb_control_t ctrl;
    void* instance;                                       /* istanza di classe BASIC */
    void (*paint)(void* instance, gmb_gfx_context_t* g);  /* thunk → metodo BASIC Paint */
    char container[64];                                   /* nome contenitore (window/tab/panel/usercontrol); "" = corrente */
    gmb_gfx_context_t ctx;                                /* sink draw-list (riusato a ogni paint) */
} gmb_usercontrol_t;
void gmb_usercontrol_init(gmb_usercontrol_t* u, int x, int y, int w, int h,
                          void (*paint)(void*, gmb_gfx_context_t*), void* instance,
                          const char* container, const char* name);
gmb_control_t* gmb_tui_add_usercontrol(int x, int y, int w, int h,
                                       void (*paint)(void*, gmb_gfx_context_t*), void* instance,
                                       const char* container, const char* name);

/* GraphicsContext per HANDLE (backend C): `Paint(g AS DOUBLE)` riceve l'handle;
 * le primitive sono funzioni runtime. Vedi gmb_tui_usercontrol.c. */
int  gmb_gfxctx_open(gmb_gfx_context_t* g);
void gmb_gfxctx_close(int h);
int  gmb_gfxctx_h_width(int h);
int  gmb_gfxctx_h_height(int h);
void gmb_gfxctx_h_clear(int h);
void gmb_gfxctx_h_setcolor(int h, double c);
void gmb_gfxctx_h_line(int h, double x1, double y1, double x2, double y2);
void gmb_gfxctx_h_rect(int h, double x, double y, double w, double hh);
void gmb_gfxctx_h_fillrect(int h, double x, double y, double w, double hh);
void gmb_gfxctx_h_circle(int h, double cx, double cy, double r);
void gmb_gfxctx_h_fillcircle(int h, double cx, double cy, double r);
void gmb_gfxctx_h_text(int h, const char* s, double x, double y);
void gmb_gfxctx_h_plot(int h, double x, double y);
void gmb_gfxctx_h_op(int h, int op, double a, double b, double c, double d, const char* s);

/* ---- Damage / Delta (specchio Damage.cs) ---- */
typedef struct gmb_delta {
    int start, end;
} gmb_delta_t;

gmb_delta_t gmb_delta_unchanged(void);
bool gmb_delta_is_empty(const gmb_delta_t* d);
void gmb_delta_add(gmb_delta_t* d, int i);
void gmb_delta_add_range(gmb_delta_t* d, int start, int end);
bool gmb_delta_contains(const gmb_delta_t* d, int i);
void gmb_delta_to_range(const gmb_delta_t* d, int* start, int* end);
void gmb_delta_clear(gmb_delta_t* d);
void gmb_delta_set_all(gmb_delta_t* d, int len);

typedef struct gmb_damage_tracker {
    int start, end, height;
} gmb_damage_tracker_t;

void gmb_damage_tracker_init(gmb_damage_tracker_t* t, int height);
void gmb_damage_tracker_resize(gmb_damage_tracker_t* t, int height);
void gmb_damage_tracker_mark(gmb_damage_tracker_t* t, int row);
void gmb_damage_tracker_mark_range(gmb_damage_tracker_t* t, int start, int end);
void gmb_damage_tracker_mark_all(gmb_damage_tracker_t* t);
void gmb_damage_tracker_clear(gmb_damage_tracker_t* t);
bool gmb_damage_tracker_is_dirty(const gmb_damage_tracker_t* t);
void gmb_damage_tracker_dirty_rows(const gmb_damage_tracker_t* t, int* start, int* end);

/* ---- FocusManager (specchio FocusManager.cs) ---- */
typedef struct gmb_focus_entry {
    char id[GMB_CTRL_MAX_ID];
    gmb_rect_t area;
    bool enabled, tab_stop;
    int tab_index;
    gmb_control_t* ctrl;         /* se live rect */
    bool has_ctrl;
} gmb_focus_entry_t;

typedef struct gmb_focus_manager {
    gmb_focus_entry_t entries[GMB_CTRL_MAX_ENTRIES];
    int entry_count;
    int current;
    char bindings_chords[16][32];
    char bindings_actions[16][32];
    int bindings_count;
} gmb_focus_manager_t;

void gmb_focus_manager_init(gmb_focus_manager_t* f);
void gmb_focus_manager_bind_key(gmb_focus_manager_t* f, const char* chord, const char* action);
const char* gmb_focus_manager_resolve_key(gmb_focus_manager_t* f, const char* chord);
void gmb_focus_manager_register_rect(gmb_focus_manager_t* f, const char* id, gmb_rect_t area);
void gmb_focus_manager_register_ctrl(gmb_focus_manager_t* f, const char* id, gmb_control_t* ctrl);
void gmb_focus_manager_set_enabled(gmb_focus_manager_t* f, const char* id, bool enabled);
void gmb_focus_manager_set_area(gmb_focus_manager_t* f, const char* id, gmb_rect_t area);
const char* gmb_focus_manager_current_id(gmb_focus_manager_t* f);
bool gmb_focus_manager_set_focus_by_id(gmb_focus_manager_t* f, const char* id);
bool gmb_focus_manager_set_focus_by_point(gmb_focus_manager_t* f, int x, int y);
const char* gmb_focus_manager_next(gmb_focus_manager_t* f);
const char* gmb_focus_manager_previous(gmb_focus_manager_t* f);
bool gmb_focus_manager_handle_key(gmb_focus_manager_t* f, int key);
/* UiEvent unificato (specchio FocusManager.HandleEvent). */
const char* gmb_focus_manager_handle_event(gmb_focus_manager_t* f, const gmb_ui_event_t* ev);
void gmb_focus_manager_clear(gmb_focus_manager_t* f);

/* ---- EventHandler (specchio EventHandler.cs) ---- */
typedef struct gmb_ctrl_event_handler {
    gmb_focus_manager_t focus;
    gmb_control_t* controls[GMB_CTRL_MAX_CONTROLS];
    char ids[GMB_CTRL_MAX_CONTROLS][GMB_CTRL_MAX_ID];
    int control_count;
} gmb_ctrl_event_handler_t;

void gmb_ctrl_event_handler_init(gmb_ctrl_event_handler_t* h);
void gmb_ctrl_event_handler_register(gmb_ctrl_event_handler_t* h, const char* id, gmb_control_t* ctrl);
void gmb_ctrl_event_handler_set_focus_by_id(gmb_ctrl_event_handler_t* h, const char* id);
gmb_control_t* gmb_ctrl_event_handler_focused(gmb_ctrl_event_handler_t* h);
bool gmb_ctrl_event_handler_dispatch(gmb_ctrl_event_handler_t* h, const gmb_ui_event_t* e);
int  gmb_ctrl_event_handler_event_to_navigation(const gmb_ui_event_t* e);
bool gmb_ctrl_event_handler_is_quit(const gmb_ui_event_t* e);

/* Routing input grafico → controlli: drena gmb_gfx_input (tasti UTF-8 + mouse
 * pixel) e lo consegna al controllo/handler, convertendo pixel→cella.
 * Ritorna il numero di eventi gestiti. Stesso thread del pump SDL. */
int gmb_tui_pump_control(gmb_control_t* c, int cell_w, int cell_h);
int gmb_tui_pump_event_handler(gmb_ctrl_event_handler_t* h, int cell_w, int cell_h);

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * Widget (specchio ClipperLib/Controls: Button/CheckBox/ToggleButton/Label/
 * HLine/VLine/ProgressBar/NumericUpDown/RadioGroup/ComboBox/ListBox/ScrollBar/
 * Tab/Panel). Ogni widget estende gmb_control_t (primo campo).
 * ============================================================================ */

typedef struct gmb_button {
    gmb_control_t ctrl;
    char label[64];
    void (*on_click)(void* ctx);
    void* click_ctx;
} gmb_button_t;
void gmb_button_init(gmb_button_t* b, int x, int y, int w, const char* label);
void gmb_button_set_onclick(gmb_button_t* b, void (*fn)(void* ctx), void* ctx);
int  gmb_button_render_line(gmb_button_t* b, char* out, int cap);

typedef struct gmb_checkbox {
    gmb_control_t ctrl;
    bool checked;
    char label[64];
} gmb_checkbox_t;
void gmb_checkbox_init(gmb_checkbox_t* c, int x, int y, int w, const char* label);
void gmb_checkbox_set_checked(gmb_checkbox_t* c, bool v);
int  gmb_checkbox_render_line(gmb_checkbox_t* c, char* out, int cap);

/* ---- Splitter (due pannelli + barra divisoria trascinabile) ---- */
typedef struct {
    gmb_control_t ctrl;
    int orient;   /* 0 = verticale (A|B), 1 = orizzontale (A/B) */
    int thick;    /* spessore barra */
    int pos;      /* posizione barra (colonna/riga) */
    int min_a, min_b;
    int dragging;
} gmb_splitter_t;
void gmb_splitter_init(gmb_splitter_t* s, int x, int y, int w, int h, int orient);

/* ---- ContextMenu (popup con voci/separatori) ---- */
typedef struct {
    gmb_control_t ctrl;
    int n;
    char items[16][64];
    int sep[16];    /* 1 = separatore */
    int en[16];     /* 1 = abilitata */
    int sel;        /* voce selezionata (-1) */
    int open;       /* 0 = chiuso (default) */
    int chosen;     /* indice scelto (-1) */
    int px, py, pw; /* posizione/larghezza popup (relative al controllo) */
} gmb_context_menu_t;
void gmb_context_menu_init(gmb_context_menu_t* m, int x, int y, int w, int h);
void gmb_context_menu_set_items(gmb_context_menu_t* m, const char** items, int n);

typedef struct gmb_toggle_button {
    gmb_control_t ctrl;
    char caption[64];
    bool pressed;
} gmb_toggle_button_t;
void gmb_toggle_button_init(gmb_toggle_button_t* t, int x, int y, int w, const char* caption);

typedef struct gmb_label {
    gmb_control_t ctrl;
    char text[128];
    int align;             /* 0=left 1=center 2=right */
} gmb_label_t;
void gmb_label_init(gmb_label_t* l, int x, int y, int w, const char* text);
void gmb_label_set_text(gmb_label_t* l, const char* text);
int  gmb_label_render_line(gmb_label_t* l, char* out, int cap);

typedef struct gmb_hline {
    gmb_control_t ctrl;
    char label[64];
} gmb_hline_t;
void gmb_hline_init(gmb_hline_t* h, int x, int y, int w, const char* label);

typedef struct gmb_vline {
    gmb_control_t ctrl;
} gmb_vline_t;
void gmb_vline_init(gmb_vline_t* v, int x, int y, int h);

typedef struct gmb_progress_bar {
    gmb_control_t ctrl;
    int min, max, value;
    bool show_percent;
} gmb_progress_bar_t;
void gmb_progress_bar_init(gmb_progress_bar_t* p, int x, int y, int w);
void gmb_progress_bar_set(gmb_progress_bar_t* p, int value);
void gmb_progress_bar_set_percentage(gmb_progress_bar_t* p, int pct);
int  gmb_progress_bar_render_line(gmb_progress_bar_t* p, char* out, int cap);

typedef struct gmb_numeric_updown {
    gmb_control_t ctrl;
    int value, min, max, step;
} gmb_numeric_updown_t;
void gmb_numeric_updown_init(gmb_numeric_updown_t* n, int x, int y, int w);
void gmb_numeric_updown_set_range(gmb_numeric_updown_t* n, int min, int max);
void gmb_numeric_updown_set_value(gmb_numeric_updown_t* n, int value);
int  gmb_numeric_updown_render_line(gmb_numeric_updown_t* n, char* out, int cap);

/* ---- Candlestick (widget nuovo: candele OHLC) ---- */
#define GMB_CANDLE_MAX 256
typedef struct gmb_candle { double o, h, l, c; } gmb_candle_t;
typedef struct gmb_candlestick {
    gmb_control_t ctrl;
    gmb_candle_t candles[GMB_CANDLE_MAX];
    int count;
    int cursor;
} gmb_candlestick_t;
void gmb_candlestick_init(gmb_candlestick_t* k, int x, int y, int w, int h);
void gmb_candlestick_set_candles(gmb_candlestick_t* k, const gmb_candle_t* candles, int n);
int  gmb_candlestick_cursor(const gmb_candlestick_t* k);

typedef struct gmb_radio_group {
    gmb_control_t ctrl;
    char options[16][64];
    int options_count;
    int selected;
} gmb_radio_group_t;
void gmb_radio_group_init(gmb_radio_group_t* r, int x, int y, int w, int h);
void gmb_radio_group_set_options(gmb_radio_group_t* r, const char* const* options, int count);
const char* gmb_radio_group_selected_value(gmb_radio_group_t* r);
int  gmb_radio_group_render_line(gmb_radio_group_t* r, int idx, char* out, int cap);

typedef struct gmb_combobox {
    gmb_control_t ctrl;
    char items[16][64];
    int items_count;
    int selected;
    bool open;
    int highlight;          /* ComboBox.Highlight */
    int drop_down_height;   /* ComboBox.DropDownHeight (default 5) */
} gmb_combobox_t;
void gmb_combobox_init(gmb_combobox_t* c, int x, int y, int w);
void gmb_combobox_set_items(gmb_combobox_t* c, const char* const* items, int count);
const char* gmb_combobox_selected_value(gmb_combobox_t* c);
int  gmb_combobox_render_line(gmb_combobox_t* c, char* out, int cap);

typedef struct gmb_listbox {
    gmb_control_t ctrl;
    char items[32][64];
    int items_count;
    int selected;
    int scroll_offset;
} gmb_listbox_t;
void gmb_listbox_init(gmb_listbox_t* l, int x, int y, int w, int h);
void gmb_listbox_set_items(gmb_listbox_t* l, const char* const* items, int count);
const char* gmb_listbox_selected_value(gmb_listbox_t* l);
int  gmb_listbox_render_line(gmb_listbox_t* l, int idx, char* out, int cap);

typedef struct gmb_scrollbar {
    gmb_control_t ctrl;
    int min, max, value;
    int orientation;     /* 0=orizzontale 1=verticale */
    bool show_arrows;
} gmb_scrollbar_t;
void gmb_scrollbar_init(gmb_scrollbar_t* s, int x, int y, int len, int orientation);
void gmb_scrollbar_set(gmb_scrollbar_t* s, int value, int min, int max);
int  gmb_scrollbar_render_line(gmb_scrollbar_t* s, int index, char* out, int cap);

typedef struct gmb_tab {
    gmb_control_t ctrl;
    char tabs[16][64];
    int tabs_count;
    int selected;
} gmb_tab_t;
void gmb_tab_init(gmb_tab_t* t, int x, int y, int w);
void gmb_tab_set_tabs(gmb_tab_t* t, const char* const* tabs, int count);
int  gmb_tab_render_line(gmb_tab_t* t, char* out, int cap);

enum { GMB_PB_SINGLE = 0, GMB_PB_DOUBLE, GMB_PB_ROUNDED, GMB_PB_NONE };
void gmb_border_chars(int border, uint32_t out[8]);
gmb_rect_t gmb_panel_inner(gmb_control_t* c, int border);
typedef struct gmb_panel {
    gmb_control_t ctrl;
    char title[64];
    int border;
    bool draggable;
    bool is_dragging;
    int drag_off_x;
    int drag_off_y;
} gmb_panel_t;
void gmb_panel_init(gmb_panel_t* p, int x, int y, int w, int h, const char* title);
void gmb_panel_with_title(gmb_panel_t* p, const char* title);
void gmb_panel_with_border(gmb_panel_t* p, int border);
gmb_rect_t gmb_panel_inner_area(const gmb_panel_t* p);
void gmb_panel_start_drag(gmb_panel_t* p, int x, int y);
bool gmb_panel_update_drag(gmb_panel_t* p, int x, int y);
void gmb_panel_end_drag(gmb_panel_t* p);
typedef struct gmb_panel_w {
    gmb_control_t ctrl;
    char title[128];
    int border;
    bool draggable;
    bool is_dragging;
    int drag_off_x;
    int drag_off_y;
} gmb_panel_w_t;
void gmb_panel_w_init(gmb_panel_w_t* p, int x, int y, int w, int h);
typedef struct gmb_window {
    gmb_control_t ctrl;
    char title[128];
    int border;
    bool closable;
    bool draggable;
    bool is_dragging;
    int drag_off_x;
    int drag_off_y;
} gmb_window_t;
void gmb_window_init(gmb_window_t* w, int x, int y, int ww, int h);
bool gmb_window_title_bar_contains(gmb_window_t* w, int cx, int cy);
bool gmb_window_close_button_contains(gmb_window_t* w, int cx, int cy);

/* ---- ScrollView (specchio ScrollView.cs) ---- */
typedef struct gmb_scroll_view {
    gmb_control_t ctrl;
    char content[64][256];
    int content_count;
    int offset;
} gmb_scroll_view_t;
void gmb_scroll_view_init(gmb_scroll_view_t* s, int x, int y, int w, int h);
void gmb_scroll_view_set_content(gmb_scroll_view_t* s, const char* const* lines, int count);
void gmb_scroll_view_push(gmb_scroll_view_t* s, const char* line);
int  gmb_scroll_view_render_line(gmb_scroll_view_t* s, int idx, char* out, int cap);
double gmb_scroll_view_scroll_fraction(gmb_scroll_view_t* s);

/* ---- FormField + Form (specchio Form.cs) ---- */
typedef struct gmb_form_field_w {
    char label[64];
    char value[512];
    bool editing;
} gmb_form_field_w_t;

typedef struct gmb_form_widget {
    gmb_control_t ctrl;
    gmb_form_field_w_t fields[16];
    int fields_count;
    int selected_index;
} gmb_form_widget_t;
void gmb_form_widget_init(gmb_form_widget_t* f, int x, int y, int w, int h);
void gmb_form_widget_add_field(gmb_form_widget_t* f, const char* label, const char* value);
const char* gmb_form_widget_get_value(gmb_form_widget_t* f, int idx);
int  gmb_form_widget_render_line(gmb_form_widget_t* f, int idx, char* out, int cap);

/* ---- StatusBar (specchio StatusBar.cs) ---- */
typedef struct gmb_status_bar {
    gmb_control_t ctrl;
    char text[256];
} gmb_status_bar_t;
void gmb_status_bar_init(gmb_status_bar_t* s, int x, int y, int w);
void gmb_status_bar_set_text(gmb_status_bar_t* s, const char* text);
int  gmb_status_bar_render_line(gmb_status_bar_t* s, char* out, int cap);

/* ---- ClipboardUtils (specchio Clipboard.cs) ---- */
bool gmb_clipboard_is_image_path(const char* path);
void gmb_clipboard_set_text(const char* text);
const char* gmb_clipboard_try_get_text(void);

/* ---- AnsiStr (specchio AnsiStr.cs, utilità pure) ---- */
int  gmb_ansistr_width(const char* s);
int  gmb_ansistr_truncate(const char* s, int max_width, char* out, int cap);
int  gmb_ansistr_cut(const char* s, int start, int end, char* out, int cap);
int  gmb_ansistr_center(const char* s, int width, char* out, int cap);
int  gmb_ansistr_char_width(uint32_t cp);

/* ---- Menu (specchio Menu.cs) ---- */
typedef struct gmb_menu_sub_item {
    char label[64];
    bool separator;
    void (*action)(void* ctx);
    void* action_ctx;
    char hotkey[16];
} gmb_menu_sub_item_t;

typedef struct gmb_menu_item {
    char label[64];
    gmb_menu_sub_item_t sub[8];
    int sub_count;
    bool separator;
    void (*action)(void* ctx);
    void* action_ctx;
    char hotkey[16];
} gmb_menu_item_t;

typedef struct gmb_menu {
    gmb_control_t ctrl;
    gmb_menu_item_t items[16];
    int items_count;
    int selected_index;
    bool is_open;
    int sub_selected_index;
} gmb_menu_t;

void gmb_menu_init(gmb_menu_t* m, int x, int y, int w);
gmb_menu_item_t* gmb_menu_add_item(gmb_menu_t* m, const char* label);
gmb_menu_sub_item_t* gmb_menu_item_add_sub(gmb_menu_item_t* item, const char* label);
void gmb_menu_item_set_action(gmb_menu_item_t* item, void (*fn)(void* ctx), void* ctx);
void gmb_menu_sub_item_set_action(gmb_menu_sub_item_t* item, void (*fn)(void* ctx), void* ctx);
int  gmb_menu_compute_width(gmb_menu_t* m);
int  gmb_menu_submenu_width(gmb_menu_t* m);
const char* gmb_menu_get_selected_label(gmb_menu_t* m);

/* ---- BoxDrawing (specchio BoxDrawing.cs) ---- */
enum { GMB_W_NORMAL = 0, GMB_W_THICK, GMB_W_DOUBLED };
enum { GMB_BS_PLAIN = 0, GMB_BS_EM, GMB_BS_EN, GMB_BS_DOT, GMB_BS_CURVED };

typedef struct gmb_box_char {
    int up, right, left, down;   /* -1=null 0=normal 1=thick 2=doubled */
    int style;                    /* GMB_BS_* */
    bool ascii;
} gmb_box_char_t;

gmb_box_char_t gmb_box_char_empty(void);
gmb_box_char_t gmb_box_char_vertical(int w);
gmb_box_char_t gmb_box_char_horizontal(int w);
gmb_box_char_t gmb_box_char_cross(int w);
gmb_box_char_t gmb_box_char_upper_left(int w);
gmb_box_char_t gmb_box_char_upper_right(int w);
gmb_box_char_t gmb_box_char_lower_left(int w);
gmb_box_char_t gmb_box_char_lower_right(int w);
gmb_box_char_t gmb_box_char_up_tee(int w);
gmb_box_char_t gmb_box_char_right_tee(int w);
gmb_box_char_t gmb_box_char_left_tee(int w);
gmb_box_char_t gmb_box_char_down_tee(int w);
gmb_box_char_t gmb_box_char_thin(void);
gmb_box_char_t gmb_box_char_thick(void);
gmb_box_char_t gmb_box_char_round(void);
gmb_box_char_t gmb_box_char_with_style(gmb_box_char_t b, int style);
gmb_box_char_t gmb_box_char_with_up(gmb_box_char_t b, int w);
gmb_box_char_t gmb_box_char_with_right(gmb_box_char_t b, int w);
gmb_box_char_t gmb_box_char_with_left(gmb_box_char_t b, int w);
gmb_box_char_t gmb_box_char_with_down(gmb_box_char_t b, int w);
uint32_t gmb_box_char_to_char(gmb_box_char_t b);
unsigned char gmb_box_char_to_ascii(gmb_box_char_t b);

void gmb_box_drawing_draw_box(gmb_screen_buffer_t* buffer, int x, int y, int w, int h,
                              gmb_box_char_t style, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);

typedef struct gmb_box_widget {
    int x, y, w, h;
    gmb_box_char_t style;
    char title[128];
    bool has_fg, has_bg;
    gmb_cell_color_t fg, bg;
} gmb_box_widget_t;
void gmb_box_widget_init(gmb_box_widget_t* b, int x, int y, int w, int h);
gmb_rect_t gmb_box_widget_inner(gmb_box_widget_t* b);
void gmb_box_widget_draw(gmb_box_widget_t* b, gmb_screen_buffer_t* buffer);

/* ---- BigText (specchio BigText.cs, font ASCII art 7x5) ---- */
typedef struct gmb_big_text {
    gmb_control_t ctrl;
    char text[256];
    bool has_color, has_bg;
    gmb_cell_color_t color, bg;
} gmb_big_text_t;

void gmb_big_text_init(gmb_big_text_t* b, int x, int y);
void gmb_big_text_set_text(gmb_big_text_t* b, const char* text);
int  gmb_bigtext_get(uint32_t key, char rows[7][8]);
int  gmb_bigtext_width(uint32_t key);
int  gmb_bigtext_to_block(const char* text, char out[7][512]);
void gmb_bigtext_render_to_buffer(gmb_screen_buffer_t* buf, int x, int y, const char* text,
                                  const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_big_text_render(gmb_big_text_t* b, gmb_screen_buffer_t* buffer, int x, int y);
void gmb_big_text_dimensions(gmb_big_text_t* b, int* w, int* h);

/* ---- StarRating (specchio StarRating.cs) ---- */
typedef struct gmb_star_rating {
    gmb_control_t ctrl;
    int value, max;
} gmb_star_rating_t;
void gmb_star_rating_init(gmb_star_rating_t* s, int x, int y, int w);
int  gmb_star_rating_render_line(gmb_star_rating_t* s, char* out, int cap);

/* ---- Spinner (specchio Spinner.cs) ---- */
typedef struct gmb_spinner {
    gmb_control_t ctrl;
    char frames[12][16];
    int frames_count;
    int index;
} gmb_spinner_t;
void gmb_spinner_init(gmb_spinner_t* s, int x, int y, int w);
void gmb_spinner_set_preset(gmb_spinner_t* s, const char* name);
void gmb_spinner_tick(gmb_spinner_t* s);
int  gmb_spinner_render_line(gmb_spinner_t* s, char* out, int cap);

/* ---- Slider (specchio Slider.cs) ---- */
typedef struct gmb_slider {
    gmb_control_t ctrl;
    int min, max, value, step;
    bool dragging;
} gmb_slider_t;
void gmb_slider_init(gmb_slider_t* s, int x, int y, int w);
void gmb_slider_set_range(gmb_slider_t* s, int min, int max);
void gmb_slider_set_value(gmb_slider_t* s, int value);
int  gmb_slider_render_line(gmb_slider_t* s, char* out, int cap);

/* ---- Dashboard (specchio Dashboard.cs) ---- */
typedef struct gmb_metric {
    char label[64];
    float value, max;
    char unit[8];
} gmb_metric_t;

typedef struct gmb_dashboard {
    gmb_control_t ctrl;
    gmb_metric_t metrics[16];
    int metrics_count;
    int selected_index;
    int bar_width;
} gmb_dashboard_t;
void gmb_dashboard_init(gmb_dashboard_t* d, int x, int y, int w, int h);
gmb_metric_t* gmb_dashboard_add_metric(gmb_dashboard_t* d, const char* label, float value, float max, const char* unit);
int  gmb_dashboard_render_line(gmb_dashboard_t* d, int idx, char* out, int cap);

/* ---- Validator (specchio Validator.cs) ---- */
typedef struct gmb_validator {
    bool (*is_valid)(const struct gmb_validator* v, const char* text);
    const char* (*error)(const struct gmb_validator* v, const char* text);
    bool is_numeric;
    long v_min, v_max;       /* int/float range */
    bool (*fn)(void* ctx, const char* text);
    void* fn_ctx;
    const char* fn_message;
    const char* const* oneof_values;
    int oneof_count;
    char error_buf[256];
} gmb_validator_t;

void gmb_validator_int(gmb_validator_t* v, long min, long max);
void gmb_validator_float(gmb_validator_t* v, double min, double max);
void gmb_validator_fn(gmb_validator_t* v, bool (*fn)(void*, const char*), void* ctx, const char* message);
void gmb_validator_required(gmb_validator_t* v);
void gmb_validator_oneof(gmb_validator_t* v, const char* const* values, int count);
void gmb_validator_pathfree(gmb_validator_t* v);
bool gmb_validator_is_valid(const gmb_validator_t* v, const char* text);
bool gmb_validator_is_numeric(const gmb_validator_t* v);
const char* gmb_validator_error(const gmb_validator_t* v, const char* text);

/* ---- TableModel (specchio TableModel.cs) ---- */
enum { GMB_HALIGN_LEFT = 0, GMB_HALIGN_CENTER, GMB_HALIGN_RIGHT };
int gmb_text_align(const char* text, int width, int align, char* out, int cap);
typedef struct gmb_column {
    char name[64];
    int index;
    int width;
    bool has_width;
    int align;
} gmb_column_t;
void gmb_column_init(gmb_column_t* c, const char* name, int index);

/* ---- TextLayout (specchio TextLayout.cs) ---- */
enum { GMB_ITEM_BOX = 0, GMB_ITEM_GLUE, GMB_ITEM_PENALTY };
typedef struct gmb_layout_item {
    int type;
    float width;
    float stretch;
    float shrink;
    float cost;
    bool flagged;
} gmb_layout_item_t;
gmb_layout_item_t gmb_layout_box(float width);
gmb_layout_item_t gmb_layout_glue(float width, float stretch, float shrink);
gmb_layout_item_t gmb_layout_penalty(float width, float cost, bool flagged);
gmb_layout_item_t gmb_layout_forced_break(void);
float gmb_layout_natural_width(const gmb_layout_item_t* items, int count);

/* ---- WinControl (specchio WinControl.cs, enum) ---- */
enum {
    GMB_WINCTL_NONE = 0, GMB_WINCTL_BUTTON, GMB_WINCTL_LABEL, GMB_WINCTL_TEXTBOX,
    GMB_WINCTL_CHECKBOX, GMB_WINCTL_RADIO, GMB_WINCTL_COMBO, GMB_WINCTL_LISTBOX,
    GMB_WINCTL_PROGRESS, GMB_WINCTL_TREE, GMB_WINCTL_TAB, GMB_WINCTL_SCROLLBAR,
    GMB_WINCTL_SLIDER, GMB_WINCTL_PANEL, GMB_WINCTL_MENU, GMB_WINCTL_TOOLBAR
};
const char* gmb_win_control_name(int kind);

/* ---- StyleBox (specchio StyleBox.cs) + TextMeasurer ---- */
enum { GMB_SALIGN_LEFT = 0, GMB_SALIGN_CENTER, GMB_SALIGN_RIGHT };
typedef struct gmb_border_style {
    uint32_t top_left;
    uint32_t top_right;
    uint32_t bottom_left;
    uint32_t bottom_right;
    uint32_t horizontal;
    uint32_t vertical;
    bool has_fg;
    gmb_cell_color_t fg;
    bool has_bg;
    gmb_cell_color_t bg;
} gmb_border_style_t;
gmb_border_style_t gmb_border_style_default(void);
gmb_border_style_t gmb_border_style_thick(void);
gmb_border_style_t gmb_border_style_round(void);
gmb_border_style_t gmb_border_style_with_fg(gmb_border_style_t b, gmb_cell_color_t c);
gmb_border_style_t gmb_border_style_with_bg(gmb_border_style_t b, gmb_cell_color_t c);
typedef struct gmb_box_style {
    bool has_fg;
    gmb_cell_color_t fg;
    bool has_bg;
    gmb_cell_color_t bg;
    int align;
    int pad_left, pad_right, pad_top, pad_bottom;
    bool has_border;
    gmb_border_style_t border;
} gmb_box_style_t;
gmb_box_style_t gmb_box_style_new(void);
gmb_box_style_t gmb_box_style_with_fg(gmb_box_style_t s, gmb_cell_color_t c);
gmb_box_style_t gmb_box_style_with_bg(gmb_box_style_t s, gmb_cell_color_t c);
gmb_box_style_t gmb_box_style_with_padding_all(gmb_box_style_t s, int all);
gmb_box_style_t gmb_box_style_with_border(gmb_box_style_t s, gmb_border_style_t b);
gmb_box_style_t gmb_box_style_with_align(gmb_box_style_t s, int align);
int  gmb_text_measurer_measure(const char* text, int max_width);
int  gmb_text_measurer_prepare_text(const char* text, int max_width, char out[][256], int cap);
void gmb_text_measurer_render_boxed(gmb_box_style_t style, char lines[][256], int line_count,
                                    gmb_screen_buffer_t* buffer, int x, int y, int w, int h);

/* ---- ColorPicker (specchio ColorPicker.cs) ---- */
typedef struct gmb_color_picker {
    gmb_control_t ctrl;
    gmb_cell_color_t palette[256];
    int columns;
    int selected_index;
    int top_row;
} gmb_color_picker_t;
void gmb_color_picker_init(gmb_color_picker_t* p, int x, int y, int w, int h);
int gmb_color_picker_row_count(gmb_color_picker_t* p);
const gmb_cell_color_t* gmb_color_picker_cell_at(gmb_color_picker_t* p, int row, int col);
bool gmb_color_picker_is_cell_selected(gmb_color_picker_t* p, int row, int col);
int gmb_color_picker_render_line(gmb_color_picker_t* p, int row, char* out, int cap);

/* ---- DataList (specchio DataList.cs) ---- */
#define GMB_DL_MAX_ITEMS 32
typedef struct gmb_data_list {
    gmb_control_t ctrl;
    char items[GMB_DL_MAX_ITEMS][256];
    int items_count;
    int selected_index;
    int scroll_offset;
    bool multi_select;
    bool selected_items[GMB_DL_MAX_ITEMS];
    bool searchable;
    char search_buffer[64];
    gmb_cell_color_t sel_fg;
    gmb_cell_color_t sel_bg;
    int selected;
} gmb_data_list_t;
void gmb_data_list_init(gmb_data_list_t* d, int x, int y, int w, int h);
void gmb_data_list_set_items(gmb_data_list_t* d, const char* const* items, int count);
int  gmb_data_list_render_line(gmb_data_list_t* d, int idx, char* out, int cap);

/* ---- Grid (specchio Grid.cs) ---- */
typedef struct gmb_grid {
    gmb_control_t ctrl;
    char cells[128][256];
    int cells_count;
    int cols;
    int cursor_row;
    int cursor_col;
    int sel_row;
    int sel_col;
    bool has_selected;
    int align;        /* ALIGN (0 default/left, 1 center, 2 right) — compat Clipper Grid.Align */
    bool auto_lite;   /* AUTOLITE (sfondo evidenziato sulla cella corrente) */
} gmb_grid_t;
void gmb_grid_init(gmb_grid_t* g, int x, int y, int w, int h);
void gmb_grid_set_cols(gmb_grid_t* g, int cols);
void gmb_grid_set_cells(gmb_grid_t* g, const char* const* cells, int count);
int  gmb_grid_render_line(gmb_grid_t* g, int r, char* out, int cap);

/* ---- LogView (specchio LogView.cs) ---- */
enum { GMB_LOG_TRACE = 0, GMB_LOG_DEBUG, GMB_LOG_INFO, GMB_LOG_WARN, GMB_LOG_ERROR };
typedef struct gmb_log_record {
    int level;
    char target[64];
    char message[256];
} gmb_log_record_t;
typedef struct gmb_log_view {
    gmb_control_t ctrl;
    gmb_log_record_t records[256];
    int records_count;
    int capacity;
    int min_level;
    int offset;
} gmb_log_view_t;
void gmb_log_view_init(gmb_log_view_t* l, int x, int y, int w, int h);
void gmb_log_view_push(gmb_log_view_t* l, int level, const char* target, const char* message);
int  gmb_log_view_filtered_count(gmb_log_view_t* l);
int  gmb_log_view_render_line(gmb_log_view_t* l, int idx, char* out, int cap);
double gmb_log_view_scroll_fraction(gmb_log_view_t* l);

/* ---- TextArea (specchio TextArea.cs) ---- */
typedef struct gmb_text_area {
    gmb_control_t ctrl;
    char lines[64][256];
    int lines_count;
    int cursor_line;
    int cursor_col;
    int scroll_offset;
    bool modified;
    bool blink_on;
} gmb_text_area_t;
void gmb_text_area_init(gmb_text_area_t* t, int x, int y, int w, int h);
void gmb_text_area_set_lines(gmb_text_area_t* t, const char* const* lines, int count);
void gmb_text_area_scroll_up(gmb_text_area_t* t, int lines);
void gmb_text_area_scroll_down(gmb_text_area_t* t, int lines);
int  gmb_text_area_render_line(gmb_text_area_t* t, int idx, char* out, int cap);

/* ---- TreeView (specchio TreeView.cs) ---- */
typedef struct gmb_tree_node {
    char label[128];
    int depth;
    bool expanded;
    bool has_children;
} gmb_tree_node_t;
typedef struct gmb_tree_view {
    gmb_control_t ctrl;
    gmb_tree_node_t nodes[64];
    int nodes_count;
    int selected_index;
    int scroll_offset;
} gmb_tree_view_t;
void gmb_tree_view_init(gmb_tree_view_t* t, int x, int y, int w, int h);
gmb_tree_node_t* gmb_tree_view_add_node(gmb_tree_view_t* t, const char* label, int depth, bool has_children);
int gmb_tree_view_visible_indices(gmb_tree_view_t* t, int* out, int cap);
int gmb_tree_view_render_line(gmb_tree_view_t* t, int vi, char* out, int cap);

/* ---- TableModel (specchio TableModel.cs) ---- */
int gmb_text_align_align(const char* text, int width, int align, char* out, int cap);
typedef struct gmb_table_column {
    char name[64];
    int index;
} gmb_table_column_t;
typedef struct gmb_table_columns {
    gmb_table_column_t cols[16];
    int count;
} gmb_table_columns_t;
void gmb_table_columns_init(gmb_table_columns_t* c);
int  gmb_table_columns_add(gmb_table_columns_t* c, const char* name);
int  gmb_table_columns_add_with_index(gmb_table_columns_t* c, const char* name, int index);
bool gmb_table_columns_remove(gmb_table_columns_t* c, int i);
bool gmb_table_columns_insert(gmb_table_columns_t* c, int i, const char* name);
bool gmb_table_columns_reorder(gmb_table_columns_t* c, int from, int to);
gmb_table_column_t* gmb_table_columns_get(gmb_table_columns_t* c, int i);

/* ---- HexView (specchio HexView.cs) ---- */
typedef struct gmb_hex_view {
    gmb_control_t ctrl;
    unsigned char data[1024];
    int data_len;
    int bytes_per_row;
    int bytes_per_group;
    const char* group_separator;
    bool show_address;
    bool show_ascii;
    int address_format;
    unsigned long start_addr;
    int offset;
} gmb_hex_view_t;
void gmb_hex_view_init(gmb_hex_view_t* h, int x, int y, int w, int hgt);
void gmb_hex_view_set_data(gmb_hex_view_t* h, const unsigned char* data, int len);
int  gmb_hex_view_render_line(gmb_hex_view_t* h, int row, char* out, int cap);

/* ---- RichText (specchio RichText.cs) ---- */
typedef struct gmb_rich_span {
    char text[256];
    bool has_fg;
    gmb_cell_color_t fg;
    bool has_bg;
    gmb_cell_color_t bg;
    bool bold;
    bool italic;
    bool underline;
} gmb_rich_span_t;
int gmb_richtext_parse_color(const char* tok, gmb_cell_color_t* out);
int gmb_richtext_parse(const char* markup, gmb_rich_span_t* out, int max);
void gmb_richtext_render_to_buffer(gmb_screen_buffer_t* buf, int x, int y, gmb_rich_span_t* spans, int count, int max_width);

/* ---- KeyMap (specchio KeyMap.cs) ---- */
typedef struct gmb_key_map {
    char chords[16][32];
    int actions[16];
    int count;
} gmb_key_map_t;
void gmb_key_map_init(gmb_key_map_t* k);
void gmb_key_map_bind(gmb_key_map_t* k, const char* chord, int action);
int  gmb_key_map_resolve(gmb_key_map_t* k, const char* chord);
bool gmb_key_map_contains(gmb_key_map_t* k, const char* chord);
int  gmb_key_map_count(gmb_key_map_t* k);
bool gmb_key_map_is_empty(gmb_key_map_t* k);
void gmb_key_map_clear(gmb_key_map_t* k);

/* ---- FunctionKeyBar (specchio FunctionKeyBar.cs) ---- */
typedef struct gmb_function_key_bar {
    int x;
    int y;
    int width;
    char keys[10][24];
    int last_key;   /* indice dell.ultimo tasto premuto (-1 = nessuno) */
    gmb_cell_color_t fg;
    gmb_cell_color_t bg;
    gmb_cell_color_t key_fg;
    gmb_cell_color_t key_bg;
} gmb_function_key_bar_t;
void gmb_function_key_bar_init(gmb_function_key_bar_t* b, int x, int y, int width);
void gmb_function_key_bar_draw(gmb_function_key_bar_t* b, gmb_screen_buffer_t* buffer);
bool gmb_function_key_bar_handle_key(gmb_function_key_bar_t* b, int key);
int  gmb_function_key_bar_get_command(gmb_function_key_bar_t* b, int key);

/* ---- ImageButton (specchio ImageButton.cs) ---- */
typedef struct gmb_image_button {
    gmb_control_t ctrl;
    char icon[32];
    char label[64];
    void (*on_click)(void* ctx);
    void* click_ctx;
} gmb_image_button_t;
void gmb_image_button_init(gmb_image_button_t* b, int x, int y, int w);
int  gmb_image_button_render_line(gmb_image_button_t* b, char* out, int cap);

/* ---- ProgressBuilder (specchio ProgressBuilder.cs) ---- */
typedef struct gmb_bar_builder {
    int width;
    uint32_t filled_char;
    uint32_t empty_char;
    char left_bracket;
    char right_bracket;
} gmb_bar_builder_t;
gmb_bar_builder_t gmb_bar_builder_new(void);
gmb_bar_builder_t gmb_bar_builder_with_width(gmb_bar_builder_t b, int w);
gmb_bar_builder_t gmb_bar_builder_with_filled(gmb_bar_builder_t b, uint32_t c);
gmb_bar_builder_t gmb_bar_builder_with_empty(gmb_bar_builder_t b, uint32_t c);
gmb_bar_builder_t gmb_bar_builder_with_brackets(gmb_bar_builder_t b, char l, char r);
int gmb_bar_render(gmb_bar_builder_t b, double fraction, char* out, int cap);
int gmb_bar_render_with_percent(gmb_bar_builder_t b, double fraction, char* out, int cap);
int gmb_byte_bar_render(long done, long total, int width, char* out, int cap);

/* ---- PropertyGrid (specchio PropertyGrid.cs) ---- */
typedef struct gmb_property_grid {
    gmb_control_t ctrl;
    char names[32][64];
    char values[32][128];
    char edited_value[128];
    int items_count;
    int selected_index;
    int scroll_offset;
    bool editing;
} gmb_property_grid_t;
void gmb_property_grid_init(gmb_property_grid_t* p, int x, int y, int w, int h);
void gmb_property_grid_add(gmb_property_grid_t* p, const char* name, const char* value);
void gmb_property_grid_add_kv(gmb_property_grid_t* p, const char* kv);
int  gmb_property_grid_render_line(gmb_property_grid_t* p, int idx, char* out, int cap);

/* ---- DatePicker (specchio DatePicker.cs) ---- */
typedef struct gmb_date_picker {
    gmb_control_t ctrl;
    int day;
    int month;
    int year;
    bool open;
    int popup_height;
    void (*on_date_selected)(void* ctx, int year, int month, int day);
    void* date_ctx;
} gmb_date_picker_t;
void gmb_date_picker_init(gmb_date_picker_t* d, int x, int y, int w);
void gmb_date_picker_draw_popup(gmb_date_picker_t* d, gmb_screen_buffer_t* buffer);
bool gmb_date_picker_contains_point(gmb_date_picker_t* d, int x, int y);
int  gmb_date_picker_get_formatted(gmb_date_picker_t* d, gmb_clipper_state_t* st, char* out, int cap);

/* ---- AlertBox (specchio AlertBox.cs) ---- */
typedef struct gmb_alert_box {
    gmb_control_t ctrl;
    char title[128];
    char message[512];
    int border;
    char ok_label[32];
    /* Parità AlertBox.cs (nessuna proprietà persa nel porting C): */
    int align;                 /* 0=Left 1=Center 2=Right (AlertAlign) */
    int draggable;
    int box_width;             /* 0 = auto (larghezza dal contenuto) */
    int has_fg, has_bg;        /* colori opzionali (null in C# -> default) */
    gmb_cell_color_t fg, bg;
    int buttons_count;         /* 0 = solo OKButton; >0 = CONFIRM (Buttons[]) */
    char buttons[3][32];       /* etichette pulsanti CONFIRM */
    int focused_button;        /* indice del pulsante corrente (0-based) */
    int result;                /* AlertResult: 1..N = pulsante; 0 = chiusa (ESC/X) */
    int dragging;
    int drag_off_x, drag_off_y;
    int btn_off_x, btn_off_y;
} gmb_alert_box_t;
void gmb_alert_box_init(gmb_alert_box_t* a, int x, int y, int w, int h);
void gmb_alert_box_with_title(gmb_alert_box_t* a, const char* t);
void gmb_alert_box_with_message(gmb_alert_box_t* a, const char* m);
void gmb_alert_box_with_border(gmb_alert_box_t* a, int b);
void gmb_alert_box_with_alignment(gmb_alert_box_t* a, int al);
void gmb_alert_box_with_width(gmb_alert_box_t* a, int w);
void gmb_alert_box_with_colors(gmb_alert_box_t* a, int has_fg, gmb_cell_color_t fg, int has_bg, gmb_cell_color_t bg);
void gmb_alert_box_with_colors_rgb(gmb_alert_box_t* a, int has_fg, int fr, int fg, int fb, int has_bg, int br, int bg, int bb);
void gmb_alert_box_with_buttons(gmb_alert_box_t* a, const char* b0, const char* b1, const char* b2);
void gmb_alert_box_centered(gmb_alert_box_t* a, int term_w, int term_h, const char* title, const char* message, int width, int align);
int  gmb_alert_box_show(gmb_alert_box_t* a);   /* via unica: modale; ritorna result (1..N, 0=chiusa) */
int  gmb_alert_wrap_text(const char* text, int width, char out[][256], int cap);

/* ---- ConfirmationBox (specchio ConfirmationBox.cs) ---- */
enum { GMB_CONF_CONFIRM = 0, GMB_CONF_CANCEL };
typedef struct gmb_confirmation_box {
    gmb_control_t ctrl;
    int border;
    int focused_choice;
    bool draggable;
    char confirm_label[32];
    char cancel_label[32];
    char retry_label[32];
    bool has_retry;
    char message[256];
    char title[64];
    bool is_dragging;
    int drag_off_x;
    int drag_off_y;
    int result; /* -1 non risolto; altrimenti ConfirmationChoice */
} gmb_confirmation_box_t;
void gmb_confirmation_box_init(gmb_confirmation_box_t* c, int x, int y, int w, int h);
void gmb_confirmation_box_with_title(gmb_confirmation_box_t* c, const char* title);
void gmb_confirmation_box_with_message(gmb_confirmation_box_t* c, const char* message);
void gmb_confirmation_box_with_buttons(gmb_confirmation_box_t* c, const char* confirm, const char* cancel, const char* retry);
void gmb_confirmation_box_confirm_cancel(gmb_confirmation_box_t* c, int x, int y, int w, int h, const char* title, const char* msg);
void gmb_confirmation_box_centered(gmb_confirmation_box_t* c, int term_w, int term_h, const char* title, const char* msg);
bool gmb_confirmation_box_handle_key(gmb_confirmation_box_t* c, int key);
bool gmb_confirmation_box_handle_click(gmb_confirmation_box_t* c, int x, int y);

/* ---- ReorderListBox (specchio ReorderListBox.cs) ---- */
typedef struct gmb_reorder_listbox {
    gmb_control_t ctrl;
    char items[32][256];
    bool checked[32];
    int items_count;
    int top;
    int selected;
    bool has_selected;
    bool multi_select;
    bool ctrl_held;
} gmb_reorder_listbox_t;
void gmb_reorder_listbox_init(gmb_reorder_listbox_t* r, int x, int y, int w, int h);
void gmb_reorder_listbox_set_items(gmb_reorder_listbox_t* r, const char* const* items, int count);
int  gmb_reorder_listbox_render_line(gmb_reorder_listbox_t* r, int idx, char* out, int cap);

/* ---- TextInput (specchio TextInput.cs) ---- */
typedef struct gmb_text_input {
    gmb_control_t ctrl;
    char text[512];
    int cursor;
    int max_length;
    bool read_only;
    bool blink_on;
    bool password_mode;
    char format[32];       /* FORMAT (Picture/mask Clipper) — proprietá di compatibilitá */
    bool is_valid;         /* VALID/ISVALID: esito dell'ultima validazione */
    bool (*validator)(void* ctx, const char* text);
    void* validator_ctx;
} gmb_text_input_t;
void gmb_text_input_init(gmb_text_input_t* t, int x, int y, int w);
void gmb_text_input_set_text(gmb_text_input_t* t, const char* text);
bool gmb_text_input_is_valid(gmb_text_input_t* t);
int  gmb_text_input_render_line(gmb_text_input_t* t, char* out, int cap);

/* ---- HalfBlock (specchio HalfBlock.cs) ---- */
typedef struct gmb_half_cell {
    uint32_t ch;
    bool has_top;
    bool has_bottom;
    gmb_cell_color_t top_color;
    gmb_cell_color_t bottom_color;
    bool has_char;
    gmb_cell_color_t char_color;
} gmb_half_cell_t;
typedef struct gmb_half_block_canvas {
    int width;
    int height;
    gmb_half_cell_t* cells;
} gmb_half_block_canvas_t;
void gmb_half_block_canvas_init(gmb_half_block_canvas_t* c, int width, int height, uint32_t fill);
void gmb_half_block_canvas_free(gmb_half_block_canvas_t* c);
void gmb_half_block_canvas_clear_char(gmb_half_block_canvas_t* c, uint32_t fill);
void gmb_half_block_canvas_set(gmb_half_block_canvas_t* c, int x, int y, gmb_cell_color_t color);
void gmb_half_block_canvas_print(gmb_half_block_canvas_t* c, int x, int pixel_y, const char* text, gmb_cell_color_t fg, gmb_cell_color_t bg);
void gmb_half_block_canvas_render(gmb_half_block_canvas_t* c, gmb_screen_buffer_t* buffer, int offset_x, int offset_y);

/* ---- Graphics / Base64 ---- */
enum { GMB_GFX_NONE = 0, GMB_GFX_KITTY, GMB_GFX_SIXEL };
int gmb_graphics_detect(void);
int gmb_graphics_write_image(int proto, const unsigned char* rgba, int w, int h, int col, int row, int id, char* out, int cap);
int gmb_graphics_delete_image(int id, char* out, int cap);

/* ---- DialogBuilder (specchio DialogBuilder.cs) ---- */
enum { GMB_QK_INPUT = 0, GMB_QK_PASSWORD, GMB_QK_CONFIRM, GMB_QK_INT, GMB_QK_FLOAT, GMB_QK_SELECT, GMB_QK_MULTISELECT, GMB_QK_EXPAND };
typedef struct gmb_answer {
    char str[256];
    bool has_string;
    bool bool_;
    bool has_bool;
    long int_;
    bool has_int;
    double float_;
    bool has_float;
} gmb_answer_t;
typedef struct gmb_question {
    char name[64];
    int kind;
    char message[256];
    char default_str[256];
    bool has_default_bool; bool default_bool;
    bool has_default_int; long default_int;
    bool has_default_float; double default_float;
    bool has_default_index; int default_index;
    char choices[16][256];
    bool choices_sep[16];
    int choices_count;
    char multi_texts[16][256];
    bool multi_checked[16];
    int multi_count;
    char expand_keys[8]; char expand_texts[8][256];
    int expand_count;
    bool ask_if_answered;
} gmb_question_t;
void gmb_question_init(gmb_question_t* q, const char* name, int kind);
void gmb_question_add_choice(gmb_question_t* q, const char* text);
void gmb_question_add_choice_checked(gmb_question_t* q, const char* text, bool checked_);
void gmb_question_add_expand_choice(gmb_question_t* q, char key, const char* text);
int  gmb_question_resolve(gmb_question_t* q, const char* raw, gmb_answer_t* out);

/* ---- Feeder (specchio Feeder.cs) ---- */
typedef struct gmb_feeder {
    char directory[1024];
} gmb_feeder_t;
void gmb_feeder_init(gmb_feeder_t* f, const char* dir);
int  gmb_feeder_filter(const char* const* items, int n, const char* text, char out[][256], int cap);

/* ---- SpreadsheetModel (specchio SpreadsheetModel.cs) ---- */
enum { GMB_CWK_AUTO = 0, GMB_CWK_MIN, GMB_CWK_MAX, GMB_CWK_BOUND, GMB_CWK_FIXED };
typedef struct gmb_column_width {
    int kind;
    int a;
    int b;
} gmb_column_width_t;
gmb_column_width_t gmb_column_width_auto(void);
gmb_column_width_t gmb_column_width_min(int m);
gmb_column_width_t gmb_column_width_max(int mx);
gmb_column_width_t gmb_column_width_bound(int mn, int mx);
gmb_column_width_t gmb_column_width_fixed(int w);
int gmb_column_width_resolve(gmb_column_width_t cw, int max_width);
typedef struct gmb_column_def {
    char title[64];
    gmb_column_width_t width;
    int alignment;
} gmb_column_def_t;
void gmb_column_def_init(gmb_column_def_t* c, const char* title, gmb_column_width_t width);
int  gmb_column_def_resolved_width(gmb_column_def_t* c, int max_width);
typedef struct gmb_cell_cursor {
    int row;
    int col;
    bool has_selected[64][32];
    bool selected[64][32];
    bool column_select;
} gmb_cell_cursor_t;
void gmb_cell_cursor_init(gmb_cell_cursor_t* c);
void gmb_cell_cursor_move_left(gmb_cell_cursor_t* c, int n, int rows, int cols);
void gmb_cell_cursor_move_right(gmb_cell_cursor_t* c, int n, int rows, int cols);
void gmb_cell_cursor_move_up(gmb_cell_cursor_t* c, int n, int rows, int cols);
void gmb_cell_cursor_move_down(gmb_cell_cursor_t* c, int n, int rows, int cols);
void gmb_cell_cursor_toggle_select(gmb_cell_cursor_t* c);
void gmb_cell_cursor_column_select_all(gmb_cell_cursor_t* c, int rows);
bool gmb_cell_cursor_is_selected(gmb_cell_cursor_t* c, int r, int col);

/* ---- TilingModel (specchio TilingModel.cs) ---- */
#define GMB_TILING_MAX_NODES 16
typedef struct gmb_tiling_node {
    int id;
    gmb_rect_t rect;
    int split;
    double ratio;
    int offset;
    bool has_view;
    int view;
    bool removed;
    int child0, child1;
    bool has_child0, has_child1;
} gmb_tiling_node_t;
typedef struct gmb_tiling_model {
    gmb_tiling_node_t nodes[GMB_TILING_MAX_NODES];
    int node_count;
    int root;
} gmb_tiling_model_t;
void gmb_tiling_model_init(gmb_tiling_model_t* m);
int  gmb_tiling_model_split_leaf(gmb_tiling_model_t* m, int id, int direction, double ratio);
bool gmb_tiling_model_remove(gmb_tiling_model_t* m, int id);
void gmb_tiling_model_set_view(gmb_tiling_model_t* m, int id, int view);
int  gmb_tiling_model_view_of(gmb_tiling_model_t* m, int id);
double gmb_tiling_model_ratio_of(gmb_tiling_model_t* m, int id);
void gmb_tiling_model_set_ratio(gmb_tiling_model_t* m, int id, double ratio);
void gmb_tiling_model_move_offset(gmb_tiling_model_t* m, int id, int delta);
int  gmb_tiling_model_layout(gmb_tiling_model_t* m, gmb_rect_t area, int* out_ids, gmb_rect_t* out_rects, int cap);

/* ---- FileDialog (specchio FileDialog.cs) ---- */
typedef struct gmb_file_entry {
    char name[256];
    bool is_dir;
} gmb_file_entry_t;
typedef struct gmb_file_dialog {
    gmb_control_t ctrl;
    gmb_file_entry_t entries[64];
    int entries_count;
    int selected_index;
    int scroll_offset;
} gmb_file_dialog_t;
void gmb_file_dialog_init(gmb_file_dialog_t* f, int x, int y, int w, int h);
void gmb_file_dialog_set_entries(gmb_file_dialog_t* f, const gmb_file_entry_t* entries, int count);
int  gmb_file_dialog_render_line(gmb_file_dialog_t* f, int idx, char* out, int cap);

/* ---- Pretty (specchio Pretty.cs) ---- */
enum { GMB_DOC_NIL = 0, GMB_DOC_TEXT, GMB_DOC_LINE, GMB_DOC_HARDLINE, GMB_DOC_NEST, GMB_DOC_CONCAT, GMB_DOC_GROUP };
typedef struct gmb_doc {
    int kind;
    char text[256];
    int indent;
    struct gmb_doc* left;
    struct gmb_doc* right;
} gmb_doc_t;
gmb_doc_t* gmb_doc_nil(void);
gmb_doc_t* gmb_doc_text(const char* s);
gmb_doc_t* gmb_doc_line(void);
gmb_doc_t* gmb_doc_line_empty(void);
gmb_doc_t* gmb_doc_hard_line(void);
gmb_doc_t* gmb_doc_nest(int indent, gmb_doc_t* doc);
gmb_doc_t* gmb_doc_concat(gmb_doc_t* a, gmb_doc_t* b);
gmb_doc_t* gmb_doc_group(gmb_doc_t* doc);
gmb_doc_t* gmb_doc_append(gmb_doc_t* a, gmb_doc_t* b);
gmb_doc_t* gmb_doc_space(void);
gmb_doc_t* gmb_doc_soft_line(void);
int gmb_doc_render_string(gmb_doc_t* doc, int width, char* out, int cap);
void gmb_doc_free(gmb_doc_t* doc);

/* ---- BufferWidgets (specchio BufferWidgets.cs) ---- */
typedef struct gmb_border_box {
    gmb_cell_color_t border_color;
    char title[64];
} gmb_border_box_t;
void gmb_border_box_draw(gmb_border_box_t* b, gmb_screen_buffer_t* buf, gmb_rect_t area);

typedef struct gmb_paragraph {
    gmb_rich_span_t spans[16];
    int spans_count;
} gmb_paragraph_t;
void gmb_paragraph_draw(gmb_paragraph_t* p, gmb_screen_buffer_t* buf, gmb_rect_t area);

#define GMB_BW_LIST_MAX 32
typedef struct gmb_bw_list {
    char items[GMB_BW_LIST_MAX][256];
    int items_count;
    int selected;
    gmb_cell_color_t sel_fg;
    gmb_cell_color_t sel_bg;
} gmb_bw_list_t;
void gmb_bw_list_draw(gmb_bw_list_t* l, gmb_screen_buffer_t* buf, gmb_rect_t area);

#define GMB_BW_TABS_MAX 16
typedef struct gmb_bw_tabs {
    char titles[GMB_BW_TABS_MAX][64];
    int titles_count;
    int selected;
    gmb_cell_color_t sel_fg;
    gmb_cell_color_t sel_bg;
} gmb_bw_tabs_t;
void gmb_bw_tabs_draw(gmb_bw_tabs_t* t, gmb_screen_buffer_t* buf, gmb_rect_t area);

typedef struct gmb_bw_table {
    char headers[8][128];
    int headers_count;
    char rows[16][8][128];
    int rows_count;
    int row_cols;
    gmb_cell_color_t border_color;
} gmb_bw_table_t;
void gmb_bw_table_draw(gmb_bw_table_t* t, gmb_screen_buffer_t* buf, gmb_rect_t area);

/* ---- FileIcons (specchio FileIcons.cs) ---- */
typedef struct gmb_icon_info {
    char icon[16];
    bool has_color;
    gmb_cell_color_t color;
} gmb_icon_info_t;
gmb_icon_info_t gmb_file_theme_icon_for_path(const char* path);
gmb_icon_info_t gmb_file_theme_icon_for(const char* name, bool is_dir, bool is_symlink, bool is_executable);

/* ---- Tiling (specchio Tiling.cs) ---- */
typedef struct gmb_window_id {
    unsigned long id;
} gmb_window_id_t;
enum { GMB_TILING_H = 0, GMB_TILING_V };
enum { GMB_DIR_LEFT = 0, GMB_DIR_RIGHT, GMB_DIR_ABOVE, GMB_DIR_BELOW };
int gmb_tiling_orientation_flip(int o);
int gmb_tiling_direction_opposite(int d);
#define GMB_STACK_MAX 16
typedef struct gmb_tiling_stack {
    gmb_window_id_t windows[GMB_STACK_MAX];
    int windows_count;
    int focused;
} gmb_tiling_stack_t;
void gmb_tiling_stack_init(gmb_tiling_stack_t* s);
int  gmb_tiling_stack_add_window(gmb_tiling_stack_t* s, gmb_window_id_t w, bool focus);
bool gmb_tiling_stack_remove_window(gmb_tiling_stack_t* s, gmb_window_id_t w);
int  gmb_tiling_stack_cycle_focus(gmb_tiling_stack_t* s, int dir);
int  gmb_tiling_stack_insert_after(gmb_tiling_stack_t* s, gmb_window_id_t w, gmb_window_id_t after, bool focus);
bool gmb_tiling_stack_contains(gmb_tiling_stack_t* s, gmb_window_id_t w);
int  gmb_tiling_stack_focused_index(gmb_tiling_stack_t* s);
gmb_window_id_t gmb_tiling_stack_focused_window(gmb_tiling_stack_t* s);
void gmb_tiling_split_rect(gmb_rect_t rect, int orientation, float ratio, gmb_rect_t* first, gmb_rect_t* second);
typedef struct gmb_tiler {
    gmb_window_id_t windows[GMB_STACK_MAX];
    int windows_count;
    gmb_rect_t work_area;
} gmb_tiler_t;
void gmb_tiler_init(gmb_tiler_t* t);
void gmb_tiler_set_work_area(gmb_tiler_t* t, gmb_rect_t r);
int  gmb_tiler_count(gmb_tiler_t* t);
bool gmb_tiler_is_empty(gmb_tiler_t* t);
bool gmb_tiler_contains(gmb_tiler_t* t, gmb_window_id_t w);
bool gmb_tiler_add(gmb_tiler_t* t, gmb_window_id_t w);
bool gmb_tiler_remove(gmb_tiler_t* t, gmb_window_id_t w);

/* ---- Fuzzy (specchio Fuzzy.cs) ---- */
typedef struct gmb_fuzzy_result {
    int score;
    int matched;
} gmb_fuzzy_result_t;
gmb_fuzzy_result_t gmb_fuzzy_score(const char* query, const char* candidate);
bool gmb_fuzzy_matches(const char* query, const char* candidate);
int  gmb_fuzzy_filter(const char* query, const char* const* items, int count, int* indexes, int max_results);

/* ---- Qr (specchio Qr.cs) ---- */
enum { GMB_QR_ECC_L = 0, GMB_QR_ECC_M, GMB_QR_ECC_Q, GMB_QR_ECC_H };
typedef struct gmb_qr_matrix {
    int width;
    int height;
    unsigned char* modules;
} gmb_qr_matrix_t;
gmb_qr_matrix_t* gmb_qr_encode(const char* text, int ecc_level);
bool gmb_qr_get(const gmb_qr_matrix_t* m, int x, int y);
void gmb_qr_set(gmb_qr_matrix_t* m, int x, int y, bool v);
void gmb_qr_matrix_to_halfblock(const gmb_qr_matrix_t* m, char* out, int cap);
void gmb_qr_string(const char* text, int ecc_level, char* out, int cap);
void gmb_qr_matrix_free(gmb_qr_matrix_t* m);

/* ---- Repl (specchio Repl.cs) ---- */
#define GMB_REPL_MAX_CMDS 32
#define GMB_REPL_MAX_HISTORY 64
typedef void (*gmb_repl_handler_t)(const char* line, void* userdata);
typedef void (*gmb_repl_completer_t)(const char* line, int pos, char out[][256], int* out_count, int max, void* userdata);
typedef struct gmb_repl_cmd {
    char name[64];
    char description[128];
    gmb_repl_handler_t handler;
} gmb_repl_cmd_t;
typedef struct gmb_repl {
    char prompt[64];
    gmb_repl_cmd_t commands[GMB_REPL_MAX_CMDS];
    int commands_count;
    char history[GMB_REPL_MAX_HISTORY][256];
    int history_count;
    gmb_repl_completer_t completer;
    void* completer_userdata;
    void* userdata;
} gmb_repl_t;
void gmb_repl_init(gmb_repl_t* r, const char* prompt);
void gmb_repl_register(gmb_repl_t* r, const char* name, const char* description, gmb_repl_handler_t handler);
void gmb_repl_register_completer(gmb_repl_t* r, gmb_repl_completer_t c, void* userdata);
int  gmb_repl_complete(gmb_repl_t* r, const char* line, int pos, char out[][256], int max);
int  gmb_repl_history_add(gmb_repl_t* r, const char* line);
int  gmb_repl_history_prev(gmb_repl_t* r, const char* current, char* out, int cap);
int  gmb_repl_history_next(gmb_repl_t* r, char* out, int cap);
bool gmb_repl_save_history(const gmb_repl_t* r, const char* path);
int  gmb_repl_load_history(gmb_repl_t* r, const char* path);
bool gmb_repl_dispatch(gmb_repl_t* r, const char* line);

/* ---- WindowManager (specchio WindowManager.cs) ---- */
enum { GMB_WSTATE_FLOATING = 0, GMB_WSTATE_MAXIMIZED, GMB_WSTATE_MINIMIZED };
enum { GMB_WM_BORDER_SINGLE = 0, GMB_WM_BORDER_DOUBLE, GMB_WM_BORDER_ROUNDED, GMB_WM_BORDER_NONE };
typedef struct gmb_tile_grid {
    int rows;
    int cols;
} gmb_tile_grid_t;
void gmb_tile_grid_init(gmb_tile_grid_t* g, int rows, int cols);
int gmb_tile_grid_cells(const gmb_tile_grid_t* g);
void gmb_tile_grid_rowcol(const gmb_tile_grid_t* g, int index, int* row, int* col);
typedef struct gmb_managed_window {
    int id;
    char title[128];
    char name[128];        /* identificatore logico (WindowByName) */
    int frame_style;       /* FrameStyle C#: 0=Single 1=Double 2=Rounded */
    int shadow;
    gmb_rect_t rect;
    int z;
    int state;
    gmb_rect_t restore_rect;
    bool minimized;
    int min_order;   /* ordine di minimizzazione (chip in coda) */
    int border;
    bool focused;
    bool closable;
    bool draggable;
    bool editable;
    int desktop;
    /* ---- Modello di CONTENUTO (parità ManagedWindow.cs: Lines/ScrollOffset/Cursor) ---- */
    char lines[256][256];   /* buffer testo della finestra (GMB_MWIN_MAX_LINES/COLS) */
    int lines_count;
    int scroll_offset;
    int cursor_line;
    int cursor_col;
    /* --- Finestre PIXEL (graphics screen) --- */
    int pixel;               /* 1 = finestra PIXEL: rect in PIXEL LOGICI, resa dal compositor gfx */
    unsigned int body_color; /* 0 = colore del tema */
    unsigned int fg_color;
    unsigned int title_color;
} gmb_managed_window_t;
gmb_rect_t gmb_managed_window_content_rect(const gmb_managed_window_t* w);
gmb_rect_t gmb_managed_window_titlebar_rect(const gmb_managed_window_t* w);
void gmb_managed_window_append_line(gmb_managed_window_t* w, const char* line);
void gmb_managed_window_set_lines(gmb_managed_window_t* w, const char* const* lines, int n);
void gmb_managed_window_scroll_to(gmb_managed_window_t* w, int offset);
void gmb_managed_window_scroll_by(gmb_managed_window_t* w, int delta);
const char* gmb_managed_window_say_at(gmb_managed_window_t* w, int row, int col, const char* text);
void gmb_managed_window_box_at(gmb_managed_window_t* w, int top, int left, int bottom, int right, const char* border);
void gmb_managed_window_snapshot(const gmb_managed_window_t* w, gmb_managed_window_t* out);
void gmb_managed_window_restore_from(gmb_managed_window_t* w, const gmb_managed_window_t* snap);
#define GMB_WM_MAX_WINDOWS 16
typedef struct gmb_window_manager {
    gmb_rect_t work_area;
    gmb_managed_window_t windows[GMB_WM_MAX_WINDOWS];
    int count;
    int focus;
    int next_id;
    int next_z;
    int current_desktop;
    char status_text[128];
    int drag_id, drag_ox, drag_oy;
    int resize_id;
    int resize_edges;          /* bitmask: N=1 S=2 E=4 W=8 (corner = 2 edge) */
    int resize_ow, resize_oh;  /* dimensione al momento dello start */
} gmb_window_manager_t;
void gmb_window_manager_init(gmb_window_manager_t* m, gmb_rect_t work);
int  gmb_window_manager_add(gmb_window_manager_t* m, const char* title, gmb_rect_t rect);
int  gmb_window_manager_add_ex(gmb_window_manager_t* m, const char* name, const char* title, gmb_rect_t rect, int border, int frame_style, int shadow, int desktop);
int  gmb_managed_window_control_at(const gmb_managed_window_t* w, int x, int y);
int  gmb_managed_window_resize_edge_at(const gmb_managed_window_t* w, int x, int y);
int  gmb_managed_window_get_int(const gmb_managed_window_t* w, int field);
void gmb_managed_window_set_int(gmb_managed_window_t* w, int field, int v);
const char* gmb_managed_window_get_str(const gmb_managed_window_t* w, int field);
void gmb_managed_window_set_str(gmb_managed_window_t* w, int field, const char* v);
const char* gmb_managed_window_get_line(const gmb_managed_window_t* w, int index);
int  gmb_managed_window_lines_count(const gmb_managed_window_t* w);
const char* gmb_managed_window_line_get(const gmb_managed_window_t* w, int i);
void gmb_managed_window_line_set(gmb_managed_window_t* w, int i, const char* s);
void gmb_managed_window_line_insert(gmb_managed_window_t* w, int i, const char* s);
void gmb_managed_window_line_remove(gmb_managed_window_t* w, int i);
void gmb_managed_window_lines_clear(gmb_managed_window_t* w);
gmb_managed_window_t* gmb_window_manager_get(gmb_window_manager_t* m, int id);
int  gmb_window_manager_focused(gmb_window_manager_t* m);
int  gmb_window_manager_topmost_at(gmb_window_manager_t* m, int x, int y);
void gmb_window_manager_raise(gmb_window_manager_t* m, int id);
void gmb_window_manager_deactivate(gmb_window_manager_t* m, int id);
int  gmb_window_manager_find_by_title(gmb_window_manager_t* m, const char* title);
void gmb_window_manager_move_by(gmb_window_manager_t* m, int id, int dx, int dy);
void gmb_window_manager_move_to(gmb_window_manager_t* m, int id, int x, int y);
void gmb_window_manager_resize_by(gmb_window_manager_t* m, int id, int dw, int dh);
void gmb_window_manager_minimize(gmb_window_manager_t* m, int id);
void gmb_window_manager_restore(gmb_window_manager_t* m, int id);
/* Istanza statica del window manager (condivisa da WINDOW DEFINE/ACTION e dal
 * backend C99) — inizializzata lazy. */
gmb_window_manager_t* gmb_window_manager_static(void);
void gmb_window_manager_maximize(gmb_window_manager_t* m, int id);
void gmb_window_manager_tile(gmb_window_manager_t* m, int rows, int cols);
void gmb_window_manager_snap_zone(gmb_window_manager_t* m, int id, int zone);
void gmb_window_manager_cascade(gmb_window_manager_t* m, int offset_x, int offset_y);
void gmb_window_manager_arrange_tiled(gmb_window_manager_t* m);
void gmb_window_manager_arrange_cascaded(gmb_window_manager_t* m);
void gmb_window_manager_remove(gmb_window_manager_t* m, int id);
/* --- 100% parity WindowManager.cs:1321 (2026-08-26) --- */
enum { GMB_WM_CTRL_CLOSE = 0, GMB_WM_CTRL_MIN = 1, GMB_WM_CTRL_MAX = 2 };
enum { GMB_WM_EDGE_NONE = 0, GMB_WM_EDGE_N = 'N', GMB_WM_EDGE_S = 'S', GMB_WM_EDGE_E = 'E', GMB_WM_EDGE_W = 'W' };
void gmb_window_manager_set_statusbar(gmb_window_manager_t* m, const char* text);
bool gmb_window_manager_has_status(const gmb_window_manager_t* m);
void gmb_window_manager_sync_workarea(gmb_window_manager_t* m, int width, int height);
void gmb_window_manager_switch_desktop(gmb_window_manager_t* m, int desktop);
void gmb_window_manager_move_to_desktop(gmb_window_manager_t* m, int id, int desktop);
int  gmb_window_manager_select(gmb_window_manager_t* m, int id);
int  gmb_window_manager_select_title(gmb_window_manager_t* m, const char* title);
void gmb_window_manager_send_to_cell(gmb_window_manager_t* m, int id, int rows, int cols, int row, int col, int gap);
void gmb_window_manager_tile_all(gmb_window_manager_t* m, int rows, int cols, int gap);
void gmb_window_manager_maximize_toggle(gmb_window_manager_t* m, int id);
void gmb_window_manager_unminimize(gmb_window_manager_t* m, int id);
bool gmb_window_manager_is_minimized(gmb_window_manager_t* m, int id);
void gmb_window_manager_start_drag(gmb_window_manager_t* m, int id, int x, int y);
bool gmb_window_manager_update_drag(gmb_window_manager_t* m, int x, int y);
int  gmb_window_manager_end_drag(gmb_window_manager_t* m);
int  gmb_window_manager_window_at_titlebar(gmb_window_manager_t* m, int x, int y);
bool gmb_window_manager_start_resize(gmb_window_manager_t* m, int id, int x, int y, const char* edges);
bool gmb_window_manager_update_resize(gmb_window_manager_t* m, int x, int y);
void gmb_window_manager_end_resize(gmb_window_manager_t* m);
void gmb_window_manager_control_action(gmb_window_manager_t* m, int id, int ctrl);
int  gmb_window_manager_focus_next(gmb_window_manager_t* m, int forward);
int  gmb_window_manager_taskbar_chip_at(gmb_window_manager_t* m, int x, int y);
/* Render composito (desktop + cornici + titlebar + statusbar) — il contenuto resta al chiamante. */
void gmb_window_manager_render(gmb_window_manager_t* m, gmb_screen_buffer_t* buf, const gmb_theme_t* th, const char* header);
void gmb_window_manager_render_named(gmb_window_manager_t* m, gmb_screen_buffer_t* buf, const char* theme_name, const char* header);
/* Helper client C# (render): alloca/popola la struct C dalla rappresentazione C#. */
gmb_window_manager_t* gmb_window_manager_new(int x, int y, int w, int h);
void gmb_window_manager_free(gmb_window_manager_t* m);
void gmb_window_manager_clear_windows(gmb_window_manager_t* m);
void gmb_window_manager_set_current_desktop(gmb_window_manager_t* m, int d);
int  gmb_window_manager_push(gmb_window_manager_t* m, const char* title, int id, int x, int y, int w, int h,
                             int z, int state, int minimized, int border, int focused, int desktop, int closable);
void gmb_window_manager_window_rect(gmb_window_manager_t* m, int id, int* out4);
int  gmb_window_manager_window_state(gmb_window_manager_t* m, int id);
int  gmb_window_manager_window_focused(gmb_window_manager_t* m, int id);
int  gmb_window_manager_window_desktop(gmb_window_manager_t* m, int id);
int  gmb_window_manager_window_z(gmb_window_manager_t* m, int id);

/* --- Query per il client C# (P/Invoke): lo stato vive SOLO qui, il C# non tiene copie. --- */
int  gmb_window_manager_window_count(gmb_window_manager_t* m, int desktop);  /* desktop<=0 = tutti */
int  gmb_window_manager_window_id_at(gmb_window_manager_t* m, int index);    /* ordine di creazione, -1 se fuori */
int  gmb_window_manager_current_desktop(gmb_window_manager_t* m);
const char* gmb_window_manager_status_text(gmb_window_manager_t* m);
void gmb_window_manager_set_title(gmb_window_manager_t* m, int id, const char* title);
int  gmb_window_manager_is_dragging(gmb_window_manager_t* m);
int  gmb_window_manager_is_resizing(gmb_window_manager_t* m);
int  gmb_window_manager_drag_id(gmb_window_manager_t* m);
int  gmb_window_manager_resize_id(gmb_window_manager_t* m);
void gmb_window_manager_clear_focus(gmb_window_manager_t* m);
void gmb_window_manager_work_area4(gmb_window_manager_t* m, int* out4);

/* ---- PrinterDialog (specchio PrinterDialog.cs) ---- */
#define GMB_PRINTERS_MAX 16
typedef struct gmb_printer_dialog {
    gmb_control_t ctrl;                 /* PrinterDialog : Control */
    char printers[GMB_PRINTERS_MAX][128];
    int printers_count;
    int selected_index;
    unsigned short copies;
    bool duplex;
    bool confirmed;
    int btn_focus;
    int border;
    char title[64];
    char ok_label[32];
    char cancel_label[32];
} gmb_printer_dialog_t;
void gmb_printer_dialog_init(gmb_printer_dialog_t* d);
void gmb_printer_dialog_set_bounds(gmb_printer_dialog_t* d, int x, int y, int width, int height);
void gmb_printer_dialog_add_printer(gmb_printer_dialog_t* d, const char* name);
const char* gmb_printer_dialog_selected_printer(gmb_printer_dialog_t* d);
void gmb_printer_dialog_centered(gmb_printer_dialog_t* d, int term_w, int term_h, int rows);

/* ---- FileExplorer (specchio FileExplorer.cs) ---- */
#define GMB_FE_MAX_ENTRIES 512
typedef struct gmb_fe_entry {
    char name[256];
    bool is_dir;
    char path[1024];
} gmb_fe_entry_t;
typedef struct gmb_file_explorer {
    gmb_control_t ctrl;                 /* FileExplorer : Control (X/Y/Width/Height) */
    char current_path[1024];
    bool show_hidden;
    bool show_icons;
    gmb_fe_entry_t entries[GMB_FE_MAX_ENTRIES];
    int entry_count;
    int selected;
    int offset;
    char search_query[128];
    bool search_mode;
    int last_action;                    /* 0 none, 1 selected, 2 changedir */
    char selected_path[1024];
} gmb_file_explorer_t;
void gmb_file_explorer_init(gmb_file_explorer_t* e, const char* path);
void gmb_file_explorer_set_bounds(gmb_file_explorer_t* e, int x, int y, int width, int height);
int  gmb_file_explorer_last_action(const gmb_file_explorer_t* e);
const char* gmb_file_explorer_selected_path(const gmb_file_explorer_t* e);
bool gmb_file_explorer_search_mode(const gmb_file_explorer_t* e);
void gmb_file_explorer_refresh(gmb_file_explorer_t* e);
void gmb_file_explorer_set_path(gmb_file_explorer_t* e, const char* path);
int  gmb_file_explorer_entry_count(gmb_file_explorer_t* e);
int  gmb_file_explorer_selected_index(gmb_file_explorer_t* e);
const char* gmb_file_explorer_get_selected(gmb_file_explorer_t* e);
void gmb_file_explorer_next(gmb_file_explorer_t* e);
void gmb_file_explorer_prev(gmb_file_explorer_t* e);
void gmb_file_explorer_set_search(gmb_file_explorer_t* e, const char* query);
bool gmb_file_explorer_navigate_into(gmb_file_explorer_t* e);
int  gmb_file_explorer_offset(const gmb_file_explorer_t* e);
bool gmb_file_explorer_navigate(gmb_file_explorer_t* e, int delta);
int  gmb_file_explorer_filter_entries(const gmb_file_explorer_t* e, const char* query,
                                     int* out_idx, int* out_is_dir, int cap);

/* ---- CodeArea ext (specchio CodeArea.cs) ---- */
void gmb_codearea_format_line_number(int n, int width, char* out, int cap);
int  gmb_codearea_gutter_inner_width(int lines);
int  gmb_codearea_gutter_width(int lines);

/* ---- CodeArea (specchio CodeArea.cs): Control con gutter, marker, syntax, scrollbar ---- */
#define GMB_CODE_MAX_LINES 256
#define GMB_CODE_MAX_COLS  256
#define GMB_CODE_MAX_KW    64
typedef struct gmb_code_area {
    gmb_control_t ctrl;
    char lines[GMB_CODE_MAX_LINES][GMB_CODE_MAX_COLS];
    int lines_count;
    int scroll_offset;
    int cursor_line, cursor_col;
    bool show_line_numbers;
    bool breakpoints[GMB_CODE_MAX_LINES];
    bool bookmarks[GMB_CODE_MAX_LINES];
    bool errors[GMB_CODE_MAX_LINES];     /* righe con errore di lint (squiggle nel gutter) */
    int current_exec_line;
    char keywords[GMB_CODE_MAX_KW][32];
    int keyword_count;
    gmb_cell_color_t keyword_color;
    gmb_cell_color_t gutter_color;
} gmb_code_area_t;
void gmb_code_area_init(gmb_code_area_t* ca, int x, int y, int w, int h);
void gmb_code_area_set_lines(gmb_code_area_t* ca, const char* const* lines, int n);
void gmb_code_area_set_breakpoint(gmb_code_area_t* ca, int line, bool on);
void gmb_code_area_set_bookmark(gmb_code_area_t* ca, int line, bool on);
void gmb_code_area_set_error(gmb_code_area_t* ca, int line, bool on);
void gmb_code_area_clear_errors(gmb_code_area_t* ca);
void gmb_code_area_set_exec_line(gmb_code_area_t* ca, int line);
void gmb_code_area_add_keyword(gmb_code_area_t* ca, const char* kw);
int  gmb_code_area_gutter(const gmb_code_area_t* ca);
int  gmb_code_area_cursor_line(const gmb_code_area_t* ca);
typedef struct gmb_text_selection {
    int anchor_line, anchor_col;
    int end_line, end_col;
    bool active;
} gmb_text_selection_t;
void gmb_text_selection_set(gmb_text_selection_t* s, int al, int ac, int el, int ec);
void gmb_text_selection_clear(gmb_text_selection_t* s, int cur_line, int cur_col);
bool gmb_text_selection_has(const gmb_text_selection_t* s);
void gmb_text_selection_select_line(gmb_text_selection_t* s, int line, int line_len);

/* ---- DialogAdapter (specchio DialogAdapter.cs) ---- */
void gmb_dialog_build_input(const gmb_question_t* q, char* out, int cap);
bool gmb_dialog_build_confirm(const gmb_question_t* q);
int  gmb_dialog_build_select(const gmb_question_t* q);
int  gmb_dialog_build_multiselect(const gmb_question_t* q);

/* ---- Fsm (specchio Fsm.cs) ---- */
enum { GMB_TRANSITION_DO_NOTHING = 0, GMB_TRANSITION_CHANGE_STATE, GMB_TRANSITION_ABORT };
enum { GMB_ABORT_USER = 0, GMB_ABORT_ERROR };
typedef struct gmb_fsm_transition {
    int kind;
    int new_state;
    int abort_reason;
} gmb_fsm_transition_t;
typedef gmb_fsm_transition_t (*gmb_fsm_on_event_t)(void* ctx, int current_state, int event_id);
typedef struct gmb_fsm {
    int current_state;
    bool running;
    gmb_fsm_on_event_t on_event;
    void* ctx;
} gmb_fsm_t;
void gmb_fsm_init(gmb_fsm_t* f, int initial, gmb_fsm_on_event_t handler, void* ctx);
gmb_fsm_transition_t gmb_fsm_transition_nothing(void);
gmb_fsm_transition_t gmb_fsm_transition_change(int new_state);
gmb_fsm_transition_t gmb_fsm_transition_abort(int reason);
int gmb_fsm_fire(gmb_fsm_t* f, int event_id);

/* ---- ImageWidget (specchio ImageWidget.cs) ---- */
#define GMB_IW_MAX 256
typedef struct gmb_image_widget {
    gmb_control_t ctrl;             /* ImageWidget : Control (X/Y/Width/Height) */
    int img_w;
    int img_h;
    gmb_cell_color_t pixels[GMB_IW_MAX * GMB_IW_MAX];
    bool has_pixels;
    int protocol;
    int image_id;
    int last_click_x;               /* legacy: click grezzo */
    int last_click_y;
    int last_pixel_x;               /* C# LastClickPixelX/Y */
    int last_pixel_y;
} gmb_image_widget_t;
void gmb_image_widget_init(gmb_image_widget_t* w);
void gmb_image_widget_set_bounds(gmb_image_widget_t* w, int x, int y, int width, int height);
bool gmb_image_widget_set_pixels(gmb_image_widget_t* w, int wdt, int hgt, const gmb_cell_color_t* px);
bool gmb_image_widget_pixel_at(const gmb_image_widget_t* w, int x, int y, gmb_cell_color_t* out);
const char* gmb_image_widget_render_protocol(const gmb_image_widget_t* w);
int gmb_image_widget_last_click_x(const gmb_image_widget_t* w);
int gmb_image_widget_last_click_y(const gmb_image_widget_t* w);
int gmb_image_widget_last_pixel_x(const gmb_image_widget_t* w);
int gmb_image_widget_last_pixel_y(const gmb_image_widget_t* w);
void gmb_image_widget_click(gmb_image_widget_t* w, int x, int y);

/* ---- NumericUpDown (specchio NumericUpDown.cs) ---- */
typedef struct gmb_numeric_up_down {
    gmb_control_t ctrl;
    double value;
    double min;
    double max;
    double step;
    int decimals;
} gmb_numeric_up_down_t;
void gmb_numeric_up_down_init(gmb_numeric_up_down_t* n, int x, int y, int w);
void gmb_numeric_up_down_set_range(gmb_numeric_up_down_t* n, double min, double max, double step);
void gmb_numeric_up_down_set_value(gmb_numeric_up_down_t* n, double v);
double gmb_numeric_up_down_value(gmb_numeric_up_down_t* n);
void gmb_numeric_up_down_increment(gmb_numeric_up_down_t* n);
void gmb_numeric_up_down_decrement(gmb_numeric_up_down_t* n);
int gmb_numeric_up_down_render(gmb_numeric_up_down_t* n, char* out, int cap);



#endif /* GMB_TUI_WIDGETS_H */
