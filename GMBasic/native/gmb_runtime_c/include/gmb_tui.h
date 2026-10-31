/* ============================================================================
 * gmb_tui.h — Terminal User Interface (porting C della ClipperLib + CommonLib TUI)
 * File: native/gmb_runtime_c/include/gmb_tui.h
 *
 * Porting in C del layer TUI di CommonLib (Screens + Consoles.Tui) e della
 * ClipperLib (form/controlli). FASE 0: tipi base + encoder ANSI + buffer cella.
 * Fase successive: layout, form, controlli, input mask, event loop.
 *
 * Convenzioni (come gmb_commonlib.h):
 *   - Le funzioni che restituiscono stringhe ANSI scrivono in un buffer
 *     (out, out_cap) fornito dal chiamante; ritornano la lunghezza scritta.
 *   - Strutture opache = `gmb_*_t` allocate su heap con gmb_*_new/free.
 *   - Nessuna dipendenza da stdio/console OS: il terminale è ASTRAZIONE
 *     (buffer di celle + encoder ANSI), testabile in headless.
 * ============================================================================ */
#ifndef GMB_TUI_H
#define GMB_TUI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * CellColor — colore RGB true-color con flag IsDefault (specchio C# CellColor).
 * ------------------------------------------------------------------------- */
typedef struct gmb_cell_color {
    uint8_t r, g, b;
    bool is_default;
} gmb_cell_color_t;

/* Parser colore Clipper 5.3 condiviso: "W"→grigio, "W+"→bianco, "B"→blu, "B+"→blu bright,
 * "N"→nero, "N+"→grigio, "G"/"G+", "BG"/"BG+", "R"/"R+", "RB"/"RB+", "GR"/"GR+". */
gmb_cell_color_t gmb_clipper_parse_color(const char* s);

/* Costanti VGA16 (valori RGB reali, specchio CellColor.cs). */
#define GMB_CELLCOLOR_BLACK        ((gmb_cell_color_t){  0,   0,   0, 0})
#define GMB_CELLCOLOR_WHITE        ((gmb_cell_color_t){255, 255, 255, 0})
#define GMB_CELLCOLOR_RED          ((gmb_cell_color_t){255,   0,   0, 0})
#define GMB_CELLCOLOR_GREEN        ((gmb_cell_color_t){  0, 255,   0, 0})
#define GMB_CELLCOLOR_BLUE         ((gmb_cell_color_t){  0,   0, 255, 0})
#define GMB_CELLCOLOR_YELLOW       ((gmb_cell_color_t){255, 255,   0, 0})
#define GMB_CELLCOLOR_CYAN         ((gmb_cell_color_t){  0, 255, 255, 0})
#define GMB_CELLCOLOR_MAGENTA      ((gmb_cell_color_t){255,   0, 255, 0})
#define GMB_CELLCOLOR_GREY         ((gmb_cell_color_t){128, 128, 128, 0})
#define GMB_CELLCOLOR_DARKRED      ((gmb_cell_color_t){128,   0,   0, 0})
#define GMB_CELLCOLOR_DARKGREEN    ((gmb_cell_color_t){  0, 128,   0, 0})
#define GMB_CELLCOLOR_DARKBLUE     ((gmb_cell_color_t){  0,   0, 128, 0})
#define GMB_CELLCOLOR_DARKYELLOW   ((gmb_cell_color_t){128, 128,   0, 0})
#define GMB_CELLCOLOR_DARKCYAN     ((gmb_cell_color_t){  0, 128, 128, 0})
#define GMB_CELLCOLOR_DARKMAGENTA  ((gmb_cell_color_t){128,   0, 128, 0})
#define GMB_CELLCOLOR_DEFAULT      ((gmb_cell_color_t){  0,   0,   0, 1})

/* Ritorna 1 se i due colori sono uguali (stessa semantica di operator== C#). */
bool gmb_cellcolor_eq(gmb_cell_color_t a, gmb_cell_color_t b);

/* Formatta come "#RRGGBB" o "default" (specchio ToString C#). Ritorna len. */
size_t gmb_cellcolor_tostr(gmb_cell_color_t c, char* out, size_t out_cap);

/* ---------------------------------------------------------------------------
 * ScreenCell — singola cella del buffer di terminale (specchio ScreenCell.cs).
 * Carattere + colori palette (int) + RGB override + attributi.
 * ------------------------------------------------------------------------- */
typedef enum gmb_cell_attr {
    GMB_ATTR_NONE        = 0x00,
    GMB_ATTR_BOLD        = 0x01,
    GMB_ATTR_DIM         = 0x02,
    GMB_ATTR_ITALIC      = 0x04,
    GMB_ATTR_UNDERLINE   = 0x08,
    GMB_ATTR_BLINK       = 0x10,
    GMB_ATTR_REVERSE     = 0x20,
    GMB_ATTR_HIDDEN      = 0x40,
    GMB_ATTR_STRIKE      = 0x80,
} gmb_cell_attr_t;

typedef struct gmb_screen_cell {
    uint32_t character;        /* codepoint (unicode) */
    int32_t  fg_palette;       /* indice palette (0-15) o -1 se RGB override */
    int32_t  bg_palette;
    uint8_t  attrs;            /* OR di gmb_cell_attr_t */
    gmb_cell_color_t fg_rgb;   /* true-color override (valido se fg_palette<0) */
    gmb_cell_color_t bg_rgb;
} gmb_screen_cell_t;

/* Cella vuota: spazio, grigio su nero (specchio ScreenCell.Empty). */
#define GMB_CELL_EMPTY ((gmb_screen_cell_t){' ', 7, 0, 0, \
    GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT})

/* Ritorna 1 se due celle sono uguali (confronto profondo). */
bool gmb_cell_eq(gmb_screen_cell_t a, gmb_screen_cell_t b);

/* ---------------------------------------------------------------------------
 * Encoder ANSI — stringhe di escape (specchio Ansi.cs, CommonLib.Consoles.Tui).
 * Tutte le funzioni scrivono in out e ritornano la lunghezza (o 0 se buffer
 * insufficiente). Niente allocazioni.
 * ------------------------------------------------------------------------- */
/* FgRgb(38;2;r;g;b), BgRgb(48;2;...), Fg256(38;5;n), Bg256(48;5;n). */
size_t gmb_ansi_fgrgb(uint8_t r, uint8_t g, uint8_t b, char* out, size_t out_cap);
size_t gmb_ansi_bgrgb(uint8_t r, uint8_t g, uint8_t b, char* out, size_t out_cap);
size_t gmb_ansi_fg256(int idx, char* out, size_t out_cap);
size_t gmb_ansi_bg256(int idx, char* out, size_t out_cap);
/* MoveTo(col, row) — 1-based, "ESC[row;colH". */
size_t gmb_ansi_moveto(int col, int row, char* out, size_t out_cap);

/* Costanti ANSI (stringhe letterali, specchio Ansi.cs). */
#define GMB_ANSI_ESC         "\x1b"
#define GMB_ANSI_CSI         "\x1b["
#define GMB_ANSI_RESET       "\x1b[0m"
#define GMB_ANSI_BOLD        "\x1b[1m"
#define GMB_ANSI_DIM         "\x1b[2m"
#define GMB_ANSI_ITALIC      "\x1b[3m"
#define GMB_ANSI_UNDERLINE   "\x1b[4m"
#define GMB_ANSI_BLINK       "\x1b[5m"
#define GMB_ANSI_REVERSE     "\x1b[7m"
#define GMB_ANSI_HIDDEN      "\x1b[8m"
#define GMB_ANSI_STRIKE      "\x1b[9m"
#define GMB_ANSI_CURSORSHOW  "\x1b[?25h"
#define GMB_ANSI_CURSORHIDE  "\x1b[?25l"
#define GMB_ANSI_ALTSCREENIN "\x1b[?1049h"
#define GMB_ANSI_ALTSCREENOUT "\x1b[?1049l"
#define GMB_ANSI_CLEARSCREEN "\x1b[2J"
#define GMB_ANSI_CLEARLINE   "\x1b[K"
#define GMB_ANSI_HOME        "\x1b[H"
#define GMB_ANSI_ERASETOEND  "\x1b[0J"
#define GMB_ANSI_ERASELINETOEND "\x1b[0K"

/* ---------------------------------------------------------------------------
 * ScreenBuffer — buffer rettangolare di celle (specchio ScreenBuffer.cs).
 * Cells indicizzate [y*width + x]. Heap-allocato, liberato con gmb_screen_free.
 * ------------------------------------------------------------------------- */
typedef struct gmb_screen_buffer gmb_screen_buffer_t;

/* ---------------------------------------------------------------------------
 * Surface abstraction (mirror IControlSurface/GfxSurface del piano TUI-GUI).
 * Un "sink" dirotta le scritture di una ScreenBuffer su un backend diverso
 * (pixel/gfx) senza toccare i widget, che continuano a usare gmb_screen_*.
 * I parametri restano in coordinate-cella; la surface decide come renderle.
 * ------------------------------------------------------------------------- */
typedef struct gmb_screen_sink {
    void* user;   /* contesto del backend (passato alle callback) */
    int is_gfx;   /* 1 = sink pixel (consente ai widget il chrome nativo) */
    int cell_w, cell_h;       /* dimensione cella in pixel */
    int origin_x, origin_y;   /* origine pixel della griglia */
    void (*put_cell)(void* user, int x, int y, uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg);
    void (*fill)(void* user, int x, int y, int w, int h, gmb_cell_color_t bg);
    void (*destroy)(void* user);
} gmb_screen_sink_t;

void gmb_screen_set_sink(gmb_screen_buffer_t* b, gmb_screen_sink_t* sink);
gmb_screen_sink_t* gmb_screen_get_sink(gmb_screen_buffer_t* b);
bool gmb_screen_has_sink(const gmb_screen_buffer_t* b);
/* Sink pixel: rende le celle sulla pagina gfx attiva col font corrente.
 * origin/cell in pixel; nessun effetto sul contenuto delle celle. */
void gmb_screen_attach_gfx(gmb_screen_buffer_t* b, int origin_x, int origin_y, int cell_w, int cell_h);
/* Rimuove il sink (e libera le risorse del backend se destroy e' definito). */
void gmb_screen_detach_sink(gmb_screen_buffer_t* b);

/* Auto-attach della surface: se lo screen GFX e' attivo e la buffer non ha sink,
 * aggancia un sink pixel (cosi' i controlli resi su un target grafico diventano
 * per-pixel). Default ON; il SDLConsole/TUI (screen gfx spento) resta a celle. */
void gmb_surface_set_autoattach(int on);
void gmb_surface_autoattach(gmb_screen_buffer_t* b);

/* Present del buffer TUI globale sulla finestra grafica (SDL): blit celle->pagina
 * gfx + present_tick. No-op senza screen grafico/SDL o fuori dal main thread. */
void gmb_tui_present_gfx(void);
/* Thread UI: registra il main all'avvio; SDL solo se on_main. */
void gmb_gfx_present_init_main_thread(void);
int  gmb_gfx_present_on_main_thread(void);

/* Primitivo alto livello: rettangolo arrotondato (chrome). Con sink gfx usa il
 * rendering nativo; altrimenti riempie le celle. Coordinate in celle. */
void gmb_surface_rounded_rect(gmb_screen_buffer_t* b, int cx, int cy, int cw, int ch,
                              int radius, gmb_cell_color_t fg, gmb_cell_color_t bg, int filled);

gmb_screen_buffer_t* gmb_screen_new(int width, int height);
void gmb_screen_free(gmb_screen_buffer_t* b);
int gmb_screen_width(const gmb_screen_buffer_t* b);
int gmb_screen_height(const gmb_screen_buffer_t* b);
gmb_screen_cell_t* gmb_screen_cells(gmb_screen_buffer_t* b);
/* Resize preservando le celle sovrapposte (specchio ScreenBuffer.Resize). */
void gmb_screen_resize(gmb_screen_buffer_t* b, int width, int height);
/* Clear: riempie con ' ' e i colori dati (default White/Black). */
void gmb_screen_clear(gmb_screen_buffer_t* b, gmb_cell_color_t fg, gmb_cell_color_t bg);
void gmb_screen_setchar(gmb_screen_buffer_t* b, int x, int y, uint32_t ch,
                        gmb_cell_color_t fg, gmb_cell_color_t bg);
/* SetString: scrive utf8_len byte della stringa (multibyte) partendo da (x,y),
   preservando i colori di cella esistenti. Ritorna la larghezza consumata. */
int gmb_screen_setstring(gmb_screen_buffer_t* b, int x, int y, const char* s,
                         gmb_cell_color_t fg, gmb_cell_color_t bg);
/* SetStringN: come sopra ma max max_chars celle. Ritorna x avanzato. */
int gmb_screen_setstring_n(gmb_screen_buffer_t* b, int x, int y, const char* s,
                           int max_chars, gmb_cell_color_t fg, gmb_cell_color_t bg);
/* FillRect(top,left,bottom,right) inclusivi, specchio FillRect C#. */
void gmb_screen_fillrect(gmb_screen_buffer_t* b, int top, int left, int bottom, int right,
                         uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg);
gmb_screen_buffer_t* gmb_screen_clone(const gmb_screen_buffer_t* b);
/* Legge una cella in out (10 int): [0]=char(codepoint) [1..3]=fg rgb [4]=fg default
   [5..7]=bg rgb [8]=bg default [9]=attrs. Fuori range: azzera out. */
void gmb_screen_get_cell(const gmb_screen_buffer_t* b, int x, int y, int* out);
/* Scrive una cella da campi espliciti: char(codepoint), fg/bg RGB + flag default, attrs. */
void gmb_screen_set_cell(gmb_screen_buffer_t* b, int x, int y, int ch, int attrs,
                         int fg_r, int fg_g, int fg_b, int fg_def,
                         int bg_r, int bg_g, int bg_b, int bg_def);

/* ---------------------------------------------------------------------------
 * ConsoleRenderer — rende un ScreenBuffer in una stringa ANSI (headless).
 * Specchio di ConsoleRenderer.cs MA senza Console: il render produce una
 * stringa deterministica (posizionamento riga + SGR differenziale + testo),
 * testabile senza terminale. La scrittura sul terminale reale è compito del
 * backend. Ritorna la lunghezza scritta (0 se buffer insufficiente).
 * ------------------------------------------------------------------------- */
size_t gmb_render_screen(const gmb_screen_buffer_t* b, char* out, size_t out_cap);
/* Render differenziale (solo celle cambiate vs prev; prev=NULL => tutto). em_fg/em_bg = ultimo
 * colore emesso (persistente tra le chiamate). Confronto/copia dei buffer per il dedup. */
size_t gmb_render_screen_diff(const gmb_screen_buffer_t* cur, const gmb_screen_buffer_t* prev,
                              gmb_cell_color_t* em_fg, gmb_cell_color_t* em_bg,
                              char* out, size_t out_cap);
bool gmb_screen_buffer_equal(const gmb_screen_buffer_t* a, const gmb_screen_buffer_t* b);
void gmb_screen_buffer_copy_to(gmb_screen_buffer_t* dst, const gmb_screen_buffer_t* src);

/* ---------------------------------------------------------------------------
 * Geometry — Point e Rect (specchio ClipperLib.Clipper.Geometry).
 * ------------------------------------------------------------------------- */
typedef struct gmb_point { int x, y; } gmb_point_t;
typedef struct gmb_rect  { int x, y, w, h; } gmb_rect_t;

static inline gmb_point_t gmb_point_new(int x, int y) { gmb_point_t p = {x, y}; return p; }
static inline gmb_rect_t gmb_rect_new(int x, int y, int w, int h) { gmb_rect_t r = {x, y, w, h}; return r; }
static inline int gmb_rect_right(gmb_rect_t r)  { return r.x + r.w - 1; }
static inline int gmb_rect_bottom(gmb_rect_t r) { return r.y + r.h - 1; }
static inline bool gmb_rect_contains(gmb_rect_t r, int px, int py)
{ return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h; }
static inline gmb_point_t gmb_rect_center(gmb_rect_t r)
{ gmb_point_t p = { r.x + r.w / 2, r.y + r.h / 2 }; return p; }
static inline bool gmb_rect_intersect(gmb_rect_t a, gmb_rect_t b, gmb_rect_t* out)
{
    if (!out) return false;
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = gmb_rect_right(a) < gmb_rect_right(b) ? gmb_rect_right(a) : gmb_rect_right(b);
    int y1 = gmb_rect_bottom(a) < gmb_rect_bottom(b) ? gmb_rect_bottom(a) : gmb_rect_bottom(b);
    if (x1 < x0 || y1 < y0) return false;
    out->x = x0; out->y = y0; out->w = x1 - x0 + 1; out->h = y1 - y0 + 1;
    return true;
}
static inline gmb_rect_t gmb_rect_union(gmb_rect_t a, gmb_rect_t b)
{
    gmb_rect_t r;
    r.x = a.x < b.x ? a.x : b.x;
    r.y = a.y < b.y ? a.y : b.y;
    int r1 = gmb_rect_right(a) > gmb_rect_right(b) ? gmb_rect_right(a) : gmb_rect_right(b);
    int b1 = gmb_rect_bottom(a) > gmb_rect_bottom(b) ? gmb_rect_bottom(a) : gmb_rect_bottom(b);
    r.w = r1 - r.x + 1;
    r.h = b1 - r.y + 1;
    return r;
}

/* ---- Geometry.SnapZone (specchio Geometry.cs) ---- */
enum { GMB_SNAP_NONE = 0, GMB_SNAP_LEFT, GMB_SNAP_RIGHT };
int gmb_geometry_snap_zone(gmb_point_t p, gmb_rect_t screen, int threshold);

/* ---------------------------------------------------------------------------
 * GridLayout — suddivide un Rect in una griglia (specchio GridLayout.cs).
 * Split: 0=Horizontal (colonne prima), 1=Vertical (righe prima).
 * Divider: 0=Even, 1=Proportional.
 * out_rects: array del chiamante (cols*rows), ritorna il numero scritto.
 * ------------------------------------------------------------------------- */
typedef enum gmb_split_strategy {
    GMB_SPLIT_HORIZONTAL = 0,
    GMB_SPLIT_VERTICAL = 1,
} gmb_split_strategy_t;

typedef enum gmb_divider_strategy {
    GMB_DIVIDER_EVEN = 0,
    GMB_DIVIDER_PROPORTIONAL = 1,
} gmb_divider_strategy_t;

int gmb_grid_layout(gmb_rect_t area,
                    const int* cols, int col_count,
                    const int* rows, int row_count,
                    gmb_split_strategy_t split,
                    gmb_divider_strategy_t divider,
                    gmb_rect_t* out_rects, int out_cap);

/* ---------------------------------------------------------------------------
 * Layout — suddivisione direzionale di un Rect (specchio Layout.cs).
 * Constraint: tag 0=Length 1=Min 2=Max 3=Percentage 4=Ratio 5=MinMax 6=Proportion.
 * Flex: 0=Legacy 1=Start 2=End 3=Center 4=SpaceBetween 5=SpaceAround.
 * Dir: 0=Horizontal 1=Vertical.
 * ------------------------------------------------------------------------- */
typedef enum gmb_constraint_kind {
    GMB_CONST_LENGTH = 0, GMB_CONST_MIN = 1, GMB_CONST_MAX = 2,
    GMB_CONST_PERCENTAGE = 3, GMB_CONST_RATIO = 4,
    GMB_CONST_MINMAX = 5, GMB_CONST_PROPORTION = 6,
} gmb_constraint_kind_t;

typedef struct gmb_constraint {
    gmb_constraint_kind_t tag;
    int a, b;
} gmb_constraint_t;

typedef struct gmb_margin { int horizontal, vertical; } gmb_margin_t;

typedef enum gmb_layout_direction { GMB_DIR_HORIZONTAL = 0, GMB_DIR_VERTICAL = 1 } gmb_layout_direction_t;
typedef enum gmb_flex_mode { GMB_FLEX_LEGACY = 0, GMB_FLEX_START = 1, GMB_FLEX_END = 2,
    GMB_FLEX_CENTER = 3, GMB_FLEX_SPACE_BETWEEN = 4, GMB_FLEX_SPACE_AROUND = 5 } gmb_flex_mode_t;

/* Base(minimum) size per constraint su un total span (specchio Constraint.Base). */
int gmb_constraint_base(gmb_constraint_t c, int total);
/* MinSize: per MinMax ritorna a, altrimenti Base(INT_MAX) (specchio MinSize). */
int gmb_constraint_minsize(gmb_constraint_t c);

/* Split: subdivide area in n rect (uno per constraint). Ritorna il numero
   scritto (<= out_cap). Margini, spacing e flex come Layout.cs. */
int gmb_layout_split(gmb_rect_t area, gmb_margin_t margin,
                     const gmb_constraint_t* cs, int n,
                     gmb_layout_direction_t dir, int spacing, gmb_flex_mode_t flex,
                     gmb_rect_t* out_rects, int out_cap);

/* ---------------------------------------------------------------------------
 * Input Mask — parsing di template Clipper e apply/mask (specchio Inputs/Mask.cs).
 * Slot: is_literal=true → carattere letterale; false → placeholder valida/trasforma.
 * Simboli editable: A N X 9 # 0 L Y ! (come Mask.Editable).
 * DataType: 0=Text 1=Integer 2=Float 3=Date 4=Time 5=DateTime 6=Bool.
 * ------------------------------------------------------------------------- */
typedef enum gmb_mask_datatype {
    GMB_MASK_TEXT = 0, GMB_MASK_INTEGER = 1, GMB_MASK_FLOAT = 2,
    GMB_MASK_DATE = 3, GMB_MASK_TIME = 4, GMB_MASK_DATETIME = 5, GMB_MASK_BOOL = 6,
} gmb_mask_datatype_t;

typedef struct gmb_mask_slot {
    bool is_literal;
    char literal;      /* char letterale (verbatim) */
    char symbol;       /* simbolo placeholder */
    bool force_upper;
} gmb_mask_slot_t;

/* Opaque mask (heap). */
typedef struct gmb_mask gmb_mask_t;

/* Costruisce la mask dal template (parsing extended: '@' funzioni base). */
gmb_mask_t* gmb_mask_new(const char* mask);
void gmb_mask_free(gmb_mask_t* m);
int gmb_mask_slot_count(const gmb_mask_t* m);
const gmb_mask_slot_t* gmb_mask_slot(const gmb_mask_t* m, int i);
gmb_mask_datatype_t gmb_mask_datatype(const gmb_mask_t* m);
bool gmb_mask_force_uppercase(const gmb_mask_t* m);
int gmb_mask_visible_size(const gmb_mask_t* m);
char gmb_mask_fill_char(const gmb_mask_t* m);

/* Valida un carattere per il simbolo (specchio Mask.Validate). */
bool gmb_mask_validate_char(char c, char sym);
/* True se il char può stare allo slot pos (specchio IsValidChar). */
bool gmb_mask_is_valid_char(const gmb_mask_t* m, int pos, char c);
/* Trasforma (upper se richiesto) — specchio Transform. */
char gmb_mask_transform(const gmb_mask_t* m, int pos, char c);

/* Apply: letterali auto-insert, input validati, invalid skip, blank padding.
   Scrive in out (out_cap) e ritorna la lunghezza. Specchio Mask.Apply. */
int gmb_mask_apply(const gmb_mask_t* m, const char* input, char* out, int out_cap);
/* Empty(): fully masked ma vuoto (Apply("")). */
int gmb_mask_empty(const gmb_mask_t* m, char* out, int out_cap);

/* MaskedInput.ProcessKey: dato testo/cursore e un tasto (o '\b'), calcola
   (nuovo testo, nuovo cursore). Specchio MaskedInput.ProcessKey. */
int gmb_mask_process_key(const gmb_mask_t* m, const char* text, int cursor, char key,
                         char* out, int out_cap, int* out_cursor);

/* ---------------------------------------------------------------------------
 * ColorParser — parsing coppie colore tipo "W/R" (specchio ClipperState.cs).
 * Token: R/G/B/RGB(+ = bright), W=bianco, N=nero, default. ParsePair: "fg/bg".
 * ------------------------------------------------------------------------- */
/* Parsa un token colore: out->rgb + out->is_bright. Ritorna 1 se riconosciuto
   (sempre 1: il default restituisce Default). */
void gmb_color_parse_token(const char* tok, gmb_cell_color_t* out);
/* ParsePair("fg/bg"): out_fg/out_bg. Token default "W"/"N". */
void gmb_color_parse_pair(const char* spec, gmb_cell_color_t* out_fg, gmb_cell_color_t* out_bg);

/* ---------------------------------------------------------------------------
 * FormControl / FormButton — stato + rendering (specchio FormControl.cs/
 * FormButton.cs). Eventi .NET non replicati: il click via ProcessKey usa un
 * callback opzionale (user_data).
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_control {
    int row, col, width, height;
    bool visible, enabled, tabstop, focused;
    char* label;                 /* label (nullable) */
} gmb_form_control_t;

void gmb_form_control_init(gmb_form_control_t* c);
void gmb_form_control_free(gmb_form_control_t* c);

typedef struct gmb_form_button {
    gmb_form_control_t ctrl;
    char* text;
    char* color;                 /* spec colore "fg/bg" (nullable) */
    char* focus_color;           /* spec colore quando focused (nullable) */
    void (*on_click)(void* user);
    void* user_data;
    bool clicked;                /* set su ProcessKey ENTER/space, resettabile */
} gmb_form_button_t;

void gmb_form_button_init(gmb_form_button_t* b);
void gmb_form_button_free(gmb_form_button_t* b);
/* Draw su buffer (specchio FormButton.Draw: "< label >" centrato). */
void gmb_form_button_draw(const gmb_form_button_t* b, gmb_screen_buffer_t* buf,
                          gmb_cell_color_t base_fg, gmb_cell_color_t base_bg,
                          bool blink_on);
/* ProcessKey: ENTER(13) o ' ' → click (on_click chiamato), clicked=true. */
bool gmb_form_button_process_key(gmb_form_button_t* b, int key_code);

/* ---------------------------------------------------------------------------
 * FormCheckBox — toggle ●/○ (specchio FormCheckBox.cs).
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_checkbox {
    gmb_form_control_t ctrl;
    bool checked;
    char* color;
    char* focus_color;
    void (*on_toggle)(void* user, bool new_value);
    void* user_data;
} gmb_form_checkbox_t;

void gmb_form_checkbox_init(gmb_form_checkbox_t* c);
void gmb_form_checkbox_free(gmb_form_checkbox_t* c);
void gmb_form_checkbox_draw(const gmb_form_checkbox_t* c, gmb_screen_buffer_t* buf,
                            gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on);
/* ProcessKey: ' ' → toggle checked (on_toggle chiamato). */
bool gmb_form_checkbox_process_key(gmb_form_checkbox_t* c, int key_code);

/* ---------------------------------------------------------------------------
 * FormRadioButton — selezione ◆/◇ (specchio FormRadioButton.cs). Spazio setta
 * checked=true (non toggle); se già checked non fa nulla.
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_radio {
    gmb_form_control_t ctrl;
    bool checked;
    char* group_name;
    char* color;
    char* focus_color;
    void (*on_check)(void* user, bool new_value);
    void* user_data;
} gmb_form_radio_t;

void gmb_form_radio_init(gmb_form_radio_t* r);
void gmb_form_radio_free(gmb_form_radio_t* r);
void gmb_form_radio_draw(const gmb_form_radio_t* r, gmb_screen_buffer_t* buf,
                         gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on);
/* ProcessKey: ' ' → checked=true se non già checked (on_check chiamato). */
bool gmb_form_radio_process_key(gmb_form_radio_t* r, int key_code);

/* ---------------------------------------------------------------------------
 * FormListBox — lista con selezione (specchio FormListBox.cs).
 * Key code: UP=5, DOWN=24, PGUP=18, PGDN=3, HOME=1, END=6.
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_listbox {
    gmb_form_control_t ctrl;
    char** items;              /* array di stringhe (heap) */
    int item_count;
    int selected_index;        /* -1 = nessuna */
    int top_index;
    char* color;
    char* focus_color;
    char* selected_color;
    void (*on_select)(void* user, int index);
    void* user_data;
} gmb_form_listbox_t;

void gmb_form_listbox_init(gmb_form_listbox_t* l);
void gmb_form_listbox_free(gmb_form_listbox_t* l);
/* Setta gli items (copia). Ritorna 1 ok. */
int gmb_form_listbox_set_items(gmb_form_listbox_t* l, const char* const* items, int count);
/* SelectedValue (index valido) o NULL. */
const char* gmb_form_listbox_selected_value(const gmb_form_listbox_t* l);
void gmb_form_listbox_draw(const gmb_form_listbox_t* l, gmb_screen_buffer_t* buf,
                           gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on);
/* ProcessKey: navigazione + scroll. Ritorna true se consumato. */
bool gmb_form_listbox_process_key(gmb_form_listbox_t* l, int key_code);

/* ---------------------------------------------------------------------------
 * FormComboBox — combo dropdown (specchio FormComboBox.cs).
 * Key: ENTER=13, spazio=32, ESC=27, UP=5, DOWN=24, HOME=1, END=6.
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_combobox {
    gmb_form_control_t ctrl;
    char** items;
    int item_count;
    int selected_index;      /* -1 */
    bool dropped_down;
    int drop_down_height;
    int open_sel;            /* selezione corrente nella lista aperta */
    char* color;
    char* focus_color;
    char* list_color;
    void (*on_change)(void* user, int index);
    void* user_data;
} gmb_form_combobox_t;

void gmb_form_combobox_init(gmb_form_combobox_t* c);
void gmb_form_combobox_free(gmb_form_combobox_t* c);
int gmb_form_combobox_set_items(gmb_form_combobox_t* c, const char* const* items, int count);
const char* gmb_form_combobox_selected_value(const gmb_form_combobox_t* c);
void gmb_form_combobox_draw(const gmb_form_combobox_t* c, gmb_screen_buffer_t* buf,
                            gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on);
/* ProcessKey: open/close dropdown, selezione, navigazione. Ritorna true se consumato. */
bool gmb_form_combobox_process_key(gmb_form_combobox_t* c, int key_code);

/* ---------------------------------------------------------------------------
 * FormMemo — editor di testo multilinea (specchio FormMemo.cs).
 * Key: BS=8, DEL=127, ENTER=13, UP=5, DOWN=24, LEFT=28, RIGHT=29, HOME=1,
 * END=6. Ctrl-HOME/END non replicati (no KeyModifiers).
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_memo {
    gmb_form_control_t ctrl;
    char* value;             /* testo con '\n' */
    int cur_row, cur_col, scroll_row;
    bool word_wrap;
    char* color;
    char* focus_color;
    void (*on_change)(void* user, const char* new_value);
    void* user_data;
} gmb_form_memo_t;

void gmb_form_memo_init(gmb_form_memo_t* m);
void gmb_form_memo_free(gmb_form_memo_t* m);
void gmb_form_memo_set_value(gmb_form_memo_t* m, const char* value);
const char* gmb_form_memo_value(const gmb_form_memo_t* m);
/* Numero di righe (split su '\n'). */
int gmb_form_memo_line_count(const gmb_form_memo_t* m);
/* Copia la riga r in out (out_cap), ritorna la lunghezza. */
int gmb_form_memo_line(const gmb_form_memo_t* m, int r, char* out, int out_cap);
void gmb_form_memo_draw(const gmb_form_memo_t* m, gmb_screen_buffer_t* buf,
                        gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on);
/* ProcessKey: editing multilinea. Ritorna true se consumato. */
bool gmb_form_memo_process_key(gmb_form_memo_t* m, int key_code);

/* ---------------------------------------------------------------------------
 * Data — logica pura del FormDatePicker (specchio FormDatePicker.cs).
 * DayOfWeekIdx: 0=Domenica..6=Sabato (System.DayOfWeek). FormatDate supporta
 * fmt: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy, yyyy-mm-dd, yy-mm-dd,
 * dd-mm-yyyy, dd-mm-yy, mm-dd-yyyy, mm-dd-yy. century: 0 = anno a 2 cifre.
 * ------------------------------------------------------------------------- */
bool gmb_date_is_leap_year(int y);
int gmb_date_days_in_month(int y, int m);
int gmb_date_day_of_week_idx(int y, int m, int d);
/* Ritorna 1 se la data è valida entro min/max anno. */
bool gmb_date_is_valid(int d, int m, int y, int min_year, int max_year);
/* Formatta la data col formato fmt; century 0 → anno a 2 cifre. Scrive in out. */
int gmb_date_format(int d, int m, int y, const char* fmt, bool century, char* out, int out_cap);

/* ---------------------------------------------------------------------------
 * ClipperStrings — funzioni stringa (specchio ClipperStrings.cs).
 * Posizioni 1-based per At/Rat/Substr/Stuff. Tutte le funzioni "string" scrivono
 * in out (out_cap) e ritornano la lunghezza.
 * ------------------------------------------------------------------------- */
int gmb_str_at(const char* search, const char* target);
int gmb_str_rat(const char* search, const char* target);
int gmb_str_substr(const char* s, int start, int count, char* out, int out_cap);
int gmb_str_stuff(const char* s, int start, int delete, const char* insert, char* out, int out_cap);
int gmb_str_strtran(const char* s, const char* search, const char* replace, char* out, int out_cap);
int gmb_str_padl(const char* s, int length, char fill, char* out, int out_cap);
int gmb_str_padc(const char* s, int length, char fill, char* out, int out_cap);
int gmb_str_padr(const char* s, int length, char fill, char* out, int out_cap);
int gmb_str_replicate_str(const char* s, int n, char* out, int out_cap);
int gmb_str_replicate_char(char c, int n, char* out, int out_cap);
/* VAL: legge un numero da stringa (invariant culture), 0 se non valido. */
double gmb_str_val(const char* s);
/* STR: numero → stringa destra-justify (lungh.), '*' se non ci sta. */
int gmb_str_str(double value, int length, int decimals, char* out, int out_cap);
int gmb_str_strzero(double value, int length, int decimals, char* out, int out_cap);
int gmb_str_asc(const char* s);   /* codice primo char, 0 se vuoto */
int gmb_str_chr(int n, char* out, int out_cap);

/* ---------------------------------------------------------------------------
 * StripAnsi — rimuove le sequenze ESC[... da una stringa (specchio StripAnsi.cs).
 * ------------------------------------------------------------------------- */
int gmb_strip_ansi(const char* input, char* out, int out_cap);

/* ---------------------------------------------------------------------------
 * ClipperNumbers — funzioni numeriche (specchio ClipperNumbers.cs).
 * ------------------------------------------------------------------------- */
double gmb_num_abs(double n);
/* INT: tronca verso zero (C#: n>=0 ? (int)n : (int)ceil(n)). */
int gmb_num_int(double n);
/* ROUND(n, decimals): arrotondamento AwayFromZero. */
double gmb_num_round(double n, int decimals);
double gmb_num_sqrt(double n);
double gmb_num_exp(double n);
double gmb_num_log(double n);
double gmb_num_log10(double n);
double gmb_num_max(double a, double b);
int gmb_num_max_i(int a, int b);
double gmb_num_min(double a, double b);
int gmb_num_min_i(int a, int b);
/* MOD: resto con segno del divisore (semantica Clipper %). */
double gmb_num_mod(double dividend, double divisor);
int gmb_num_mod_i(int dividend, int divisor);
int gmb_num_sign(double n);

/* ---------------------------------------------------------------------------
 * Quantize — mapping colore ANSI (specchio Quantize.cs).
 * ------------------------------------------------------------------------- */
/* Palette ANSI16 (0..15) e ANSI256 (16..231 cube + 232..255 greyscale). */
extern const gmb_cell_color_t gmb_ansi16[16];
extern const gmb_cell_color_t gmb_ansi256[256];
/* QuantizeToPalette: 216-color cube entry (16..231). */
int gmb_quantize_palette(gmb_cell_color_t c);
/* QuantizeToGreyscale: 24-step greyscale (232..255). */
int gmb_quantize_greyscale(gmb_cell_color_t c);
/* QuantizeTo16: 0..15 bucket 85-step. */
int gmb_quantize_to16(gmb_cell_color_t c);
/* QuantizeTo256: il più vicino tra cube e greyscale. */
int gmb_quantize_to256(gmb_cell_color_t c);

/* ---------------------------------------------------------------------------
 * FormatPicture — PICTURE Clipper 5.3 (specchio FormatPicture.cs).
 * @-funzioni: A B C D E F I J K L<c> N R S<n> T X Z ( ) $ !.
 * Template: 9 # 0 A N X L Y ! $ * . , ( ) spazi.
 * ------------------------------------------------------------------------- */
typedef struct gmb_picture_spec {
    bool alpha, left_justify_numbers, cr_after_positive, date_set_format,
         date_british, european, insert_mode, justify_right, smart,
         literal_insert, scroll, trim, db_after_negatives, zero_blank,
         parens_with_leading, parens_no_leading, currency, upper;
    char fill_char;
    int scroll_width;
    char* template;
    bool is_numeric, is_date;
    int decimals;
    bool has_thousands;
    char thousands_sep, decimal_sep, floating_fill;   /* floating_fill: '$' '*' o 0 */
} gmb_picture_spec_t;

/* Parse(picture) → spec (template allocato; liberare con gmb_picture_free). */
gmb_picture_spec_t gmb_picture_parse(const char* picture);
void gmb_picture_free(gmb_picture_spec_t* p);

/* Apply(numero, picture): formatta un double col PICTURE. Ritorna len. */
int gmb_picture_apply_number(double value, const char* picture, char* out, int out_cap);
/* Apply(stringa, picture): testuale con placeholder + @-funzioni. Ritorna len. */
int gmb_picture_apply_text(const char* value, const char* picture, char* out, int out_cap);
/* Apply(data, picture): d/m/y col PICTURE data (template "99/99/9999" ecc). */
int gmb_picture_apply_date(int d, int m, int y, const char* picture, char* out, int out_cap);

/* ---------------------------------------------------------------------------
 * KeyConstants — costanti tasti Clipper (specchio KeyConstants.cs).
 * ------------------------------------------------------------------------- */
#define GMB_K_LEFT   19
#define GMB_K_RIGHT  4
#define GMB_K_UP     5
#define GMB_K_DOWN   24
#define GMB_K_ENTER  13
#define GMB_K_ESC    27
#define GMB_K_TAB    9
#define GMB_K_BTAB   15
#define GMB_K_BS     127
#define GMB_K_INS    22
#define GMB_K_DEL    7
#define GMB_K_HOME   1
#define GMB_K_END    6
#define GMB_K_PGUP   18
#define GMB_K_PGDN   3
#define GMB_K_F1     300
#define GMB_K_F2     301
#define GMB_K_F3     302
#define GMB_K_F4     303
#define GMB_K_F5     304
#define GMB_K_F6     305
#define GMB_K_F7     306
#define GMB_K_F8     307
#define GMB_K_F9     308
#define GMB_K_F10    309
#define GMB_K_F11    310
#define GMB_K_F12    311
#define GMB_K_MOUSE_BASE (-1000)

/* DecodeMouseKey: decodifica una key code di mouse → x, y, button, pressed.
   Ritorna 1 se è una key mouse, 0 altrimenti. */
bool gmb_key_decode_mouse(int key, int* x, int* y, int* button, bool* pressed);

/* ---------------------------------------------------------------------------
 * Codepage437 — mappa byte → Unicode (specchio Codepage437.cs).
 * ------------------------------------------------------------------------- */
extern const uint32_t gmb_cp437[256];
/* ToUnicode(byte) → codepoint (byte sconosciuto → 0x20 spazio). */
uint32_t gmb_cp437_tounicode(uint8_t b);
/* TryFromUnicode(cp) → byte CP437; ritorna -1 se non rappresentabile. */
int gmb_cp437_fromunicode(uint32_t cp);
/* Decode: stringa UTF-8 in → bytes CP437 in out (out_len byte). Unchar → '?'.
   Ritorna il numero di byte scritti. */
int gmb_cp437_encode(const char* utf8, uint8_t* out, int out_cap);

/* ---------------------------------------------------------------------------
 * FormProgressBar — barra di avanzamento (specchio FormProgressBar.cs).
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_progress {
    gmb_form_control_t ctrl;
    int value, min, max;
    char* color;
    char* fill_char;        /* default "■" (multibyte) */
    bool show_percent;
} gmb_form_progress_t;

void gmb_form_progress_init(gmb_form_progress_t* p);
void gmb_form_progress_free(gmb_form_progress_t* p);
void gmb_form_progress_draw(const gmb_form_progress_t* p, gmb_screen_buffer_t* buf,
                            gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on);

/* ---------------------------------------------------------------------------
 * FormNumeric — campo numerico con step (specchio FormNumeric.cs).
 * Key code: UP=5, DOWN=24, PGUP=18, PGDN=3 (KeyConstants).
 * ------------------------------------------------------------------------- */
typedef struct gmb_form_numeric {
    gmb_form_control_t ctrl;
    double value, min, max, step;
    int decimals;
    char fill;
    char* color;
    char* focus_color;
    void (*on_value_change)(void* user, double new_value);
    void* user_data;
} gmb_form_numeric_t;

void gmb_form_numeric_init(gmb_form_numeric_t* n);
void gmb_form_numeric_free(gmb_form_numeric_t* n);
double gmb_form_numeric_clamp(const gmb_form_numeric_t* n, double v);
void gmb_form_numeric_draw(const gmb_form_numeric_t* n, gmb_screen_buffer_t* buf,
                           gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on);
/* ProcessKey: UP/DOWN/PGUP/PGDN step, digiti e '.' editing. Ritorna true se consumato. */
bool gmb_form_numeric_process_key(gmb_form_numeric_t* n, int key_code);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_H */
int gmb_geometry_snapped_rect(int zone, gmb_rect_t work, gmb_rect_t* out);
