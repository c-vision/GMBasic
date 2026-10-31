/* ============================================================================
 * gmb_tui_clipper.h — emulatore Clipper 5.3 core (specchio ClipperLib/Clipper).
 * ClipperState, ClipperGlobal, ClipperNumbers, ClipperSet, ClipperPrinter,
 * ClipperScrn, ClipperOutput, ClipperInput (headless).
 * ============================================================================ */
#ifndef GMB_TUI_CLIPPER_H
#define GMB_TUI_CLIPPER_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gmb_mouse_action gmb_mouse_action_t;

/* ---- ClipperNumbers (specchio ClipperNumbers.cs) ---- */
int    gmb_clipper_num_abs_int(int n);
double gmb_clipper_num_abs(double n);
int    gmb_clipper_num_int(double n);          /* tronca verso zero */
double gmb_clipper_num_round(double n, int decimals);  /* AwayFromZero, decimals clamp 0..15 */
double gmb_clipper_num_round1(double n);
double gmb_clipper_num_sqrt(double n);
double gmb_clipper_num_exp(double n);
double gmb_clipper_num_log(double n);
double gmb_clipper_num_log10(double n);
double gmb_clipper_num_max(const double* vals, int n);
int    gmb_clipper_num_min_i(const int* vals, int n);
double gmb_clipper_num_min(const double* vals, int n);
/* MOD: resto con segno del divisore (semantica Clipper %). */
double gmb_clipper_num_mod(double dividend, double divisor);
int    gmb_clipper_num_mod_i(int dividend, int divisor);

/* ---- ClipperState (specchio ClipperState.cs) ---- */
#define GMB_CLIPPER_TYPEAHEAD_MAX 64

typedef struct gmb_clipper_state {
    gmb_screen_buffer_t* buffer;
    int row, col;
    char color_spec[32];
    gmb_cell_color_t fg, bg;
    unsigned char typeahead[GMB_CLIPPER_TYPEAHEAD_MAX];
    int typeahead_count;                 /* fronte/retro in coda circolare */
    int typeahead_head, typeahead_tail;
    int last_key, read_key;
    int mouse_x, mouse_y;
    bool updated;
    int pending_key;
    bool has_pending;

    /* SET settings */
    bool set_confirm, set_wrap;
    char set_delimiters[4];
    bool set_escape, set_bell;
    int set_typeahead;
    bool set_intensity;
    char set_date_format[16];
    bool set_century;
    int set_epoch;
    int set_decimals;
    bool set_fixed;

    /* message / prompt */
    bool has_message_row;
    int message_row, message_col;
    char message_text[512];

    /* printer (virtuale, in memoria) */
    bool printer_on;
    char printer_device[16];
    char printer_buffer[8192];
    int printer_buffer_len;
    int printer_row, printer_col;

    /* mouse */
    bool mouse_present;
    int mouse_row, mouse_col;
    bool mouse_down[3], mouse_pressed[3], mouse_released[3];
    bool mouse_shift[3], mouse_ctrl[3];
    bool mouse_moved, mouse_cursor_visible;
    char mouse_cursor_shape[16];
    int mouse_wheel_delta;
    bool mouse_double_clicked;
    int last_click_x, last_click_y;

    /* screen stack (SAVESCREEN/RESTSCREEN) */
    char screen_stack[8][8192];
    int screen_stack_count;

    /* read */
    int read_exit_key;
    bool cursor_visible, blink_on, blink_enabled;

    /* READ state */
    struct gmb_get* gets;
    int get_count, get_cap;
    int get_active;                  /* -1 = nessuno */
    char read_var[256];

    /* SET KEY handlers (headless) */
    int set_key_codes[16];
    void (*set_key_fns[16])(void* ctx);
    void* set_key_ctxs[16];
    bool set_key_used[16];
} gmb_clipper_state_t;

gmb_clipper_state_t* gmb_clipper_state_new(int width, int height);
void gmb_clipper_state_free(gmb_clipper_state_t* st);
void gmb_clipper_state_clear_buffer(gmb_clipper_state_t* st);

/* ---- ClipperGlobal (specchio ClipperGlobal.cs) ---- */
gmb_clipper_state_t* gmb_clipper_global_state(void);
/* Come global_state ma NON crea lo stato se assente (NULL se nessuna console TUI
 * e' mai stata usata). Serve al present grafico: senza, un programma puramente
 * grafico (SCREEN senza WINDOW) si vedeva creare un buffer TUI 80x24 vuoto e
 * ridisegnare celle bianche SOPRA la pagina grafica (flicker/wipe del contenuto). */
gmb_clipper_state_t* gmb_clipper_peek_state(void);
void gmb_clipper_global_set_screen_size(int rows, int cols);
void gmb_clipper_global_screen_size(int* rows, int* cols);

/* ---- ClipperSet (specchio ClipperSet.cs) ---- */
bool gmb_clipper_set_confirm(gmb_clipper_state_t* st, int on);      /* on<0 = getter */
bool gmb_clipper_set_wrap(gmb_clipper_state_t* st, int on);
const char* gmb_clipper_set_delimiters(gmb_clipper_state_t* st, const char* value);
bool gmb_clipper_set_escape(gmb_clipper_state_t* st, int on);
bool gmb_clipper_set_bell(gmb_clipper_state_t* st, int on);
int  gmb_clipper_set_typeahead(gmb_clipper_state_t* st, int size);
bool gmb_clipper_set_intensity(gmb_clipper_state_t* st, int on);
const char* gmb_clipper_set_date_format(gmb_clipper_state_t* st, const char* fmt);
bool gmb_clipper_set_century(gmb_clipper_state_t* st, int on);
int  gmb_clipper_set_epoch(gmb_clipper_state_t* st, int year);
int  gmb_clipper_set_decimals(gmb_clipper_state_t* st, int n);
bool gmb_clipper_set_fixed(gmb_clipper_state_t* st, int on);
int  gmb_clipper_set_format_number(gmb_clipper_state_t* st, double d, char* out, int cap);
/* colore corrente (SET COLOR / WINDOW THEME): fg/bg RGB + flag default */
void gmb_clipper_set_fg_rgb(gmb_clipper_state_t* st, int r, int g, int b, int is_default);
void gmb_clipper_set_bg_rgb(gmb_clipper_state_t* st, int r, int g, int b, int is_default);
int  gmb_clipper_get_fg_packed(gmb_clipper_state_t* st);   /* r<<16 | g<<8 | b */
int  gmb_clipper_get_bg_packed(gmb_clipper_state_t* st);
int  gmb_clipper_get_fg_default(gmb_clipper_state_t* st);
int  gmb_clipper_get_bg_default(gmb_clipper_state_t* st);
bool gmb_clipper_set_printer(gmb_clipper_state_t* st, int on);
const char* gmb_clipper_set_device(gmb_clipper_state_t* st, const char* device);
bool gmb_clipper_set_message(gmb_clipper_state_t* st, int row);     /* 0 disabilita; row<0 getter */
const char* gmb_clipper_set_message_text(gmb_clipper_state_t* st, const char* text);

/* ---- ClipperPrinter (specchio ClipperPrinter.cs) ---- */
int  gmb_clipper_printer_prow(gmb_clipper_state_t* st);
int  gmb_clipper_printer_pcol(gmb_clipper_state_t* st);
void gmb_clipper_printer_eject(gmb_clipper_state_t* st);
const char* gmb_clipper_printer_text(gmb_clipper_state_t* st);
bool gmb_clipper_printer_routing(gmb_clipper_state_t* st);
bool gmb_clipper_printer_only(gmb_clipper_state_t* st);
void gmb_clipper_printer_write(gmb_clipper_state_t* st, const char* s, bool new_line);

/* ---- ClipperScrn (specchio ClipperScrn.cs) ---- */
int  gmb_clipper_scrn_max_col(gmb_clipper_state_t* st);
int  gmb_clipper_scrn_max_row(gmb_clipper_state_t* st);
void gmb_clipper_scrn_scroll(gmb_clipper_state_t* st, int top, int left, int bottom, int right, int rows);
int  gmb_clipper_scrn_save_screen(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                                  char* out, int cap);
int  gmb_clipper_scrn_save_screen_all(gmb_clipper_state_t* st, char* out, int cap);
int  gmb_clipper_scrn_rest_screen(gmb_clipper_state_t* st, const char* s, int top, int left);
const char* gmb_clipper_scrn_tcolor(gmb_clipper_state_t* st);
bool gmb_clipper_scrn_set_blink(gmb_clipper_state_t* st, int on);   /* on<0 getter */
void gmb_clipper_scrn_prompt(gmb_clipper_state_t* st, int row, int col, const char* text);

/* ---- ClipperOutput (specchio ClipperOutput.cs) ---- */
void gmb_clipper_output_set_color(gmb_clipper_state_t* st, const char* spec);
int  gmb_clipper_output_row(gmb_clipper_state_t* st);
int  gmb_clipper_output_col(gmb_clipper_state_t* st);
void gmb_clipper_output_set_pos(gmb_clipper_state_t* st, int row, int col);
void gmb_clipper_output_say(gmb_clipper_state_t* st, int row, int col, const char* expr);
void gmb_clipper_output_say_pic(gmb_clipper_state_t* st, int row, int col, const char* expr,
                                const char* picture);
void gmb_clipper_output_say_color(gmb_clipper_state_t* st, int row, int col, const char* expr,
                                  const char* color);
void gmb_clipper_output_dev_out(gmb_clipper_state_t* st, const char* s);
void gmb_clipper_output_dev_out_pic(gmb_clipper_state_t* st, const char* s, const char* pic);
void gmb_clipper_output_qout(gmb_clipper_state_t* st, const char* s);
void gmb_clipper_output_qqout(gmb_clipper_state_t* st, const char* s);
void gmb_clipper_output_text_block(gmb_clipper_state_t* st, const char* s);
void gmb_clipper_output_box(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                            const char* style);
void gmb_clipper_output_disp_box(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                                 const char* spec);
void gmb_clipper_output_clear_screen(gmb_clipper_state_t* st);
void gmb_clipper_output_clear_region(gmb_clipper_state_t* st, int top, int left, int bottom, int right);
void gmb_clipper_output_set_cursor(gmb_clipper_state_t* st, bool on);
bool gmb_clipper_output_get_cursor(gmb_clipper_state_t* st);

/* ---- ClipperInput (specchio ClipperInput.cs, headless) ---- */
void gmb_clipper_input_keyboard(gmb_clipper_state_t* st, const char* s);
int  gmb_clipper_input_next_key(gmb_clipper_state_t* st);
int  gmb_clipper_input_inkey(gmb_clipper_state_t* st);
int  gmb_clipper_input_last_key(gmb_clipper_state_t* st);
int  gmb_clipper_input_read_key(gmb_clipper_state_t* st);
void gmb_clipper_input_set_key(gmb_clipper_state_t* st, int key, void (*fn)(void* ctx), void* ctx);
/* Accept(ref buf): scrive la stringa finale in out; ritorna 1 ok, 0 ESC.
   NOTA: richiede input via typeahead/keysource (headless). */
int  gmb_clipper_input_accept(gmb_clipper_state_t* st, char* buf, int cap);

/* ---- Get (specchio Get.cs) ---- */
typedef struct gmb_get {
    int row, col;
    char value[512];
    char picture[256];
    bool has_picture;
    char color[32], focus_color[32], error_color[32], message[256];
    bool has_range;
    double range_lo, range_hi;
    bool (*valid)(void* ctx, const char* value);
    void* valid_ctx;
    bool (*accept_key)(void* ctx, char ch);
    void* accept_key_ctx;
    void (*rejected)(void* ctx, char ch);
    void* rejected_ctx;
    bool (*when)(void* ctx);
    void* when_ctx;
    bool changed, updated;
    int cur, sel;
    void (*apply)(void* ctx, const char* value);
    void* apply_ctx;
    bool has_width;
    int width;
    char fill;
    char control_type[16];
    char items[16][256];
    int items_count;
    char caption[256];
    bool has_min, has_max, has_step;
    double min, max, step;
    bool has_height;
    int height;
    int valid_address;
    int scroll_pos;
} gmb_get_t;

void gmb_get_init(gmb_get_t* g);

/* ---- ClipperRead (specchio ClipperRead.cs) ---- */
int  gmb_clipper_read_get_width(const gmb_get_t* g);
bool gmb_clipper_read_is_valid(const gmb_get_t* g);
void gmb_clipper_read_add_get(gmb_clipper_state_t* st, const gmb_get_t* g);
int  gmb_clipper_read_get_active(gmb_clipper_state_t* st);
int  gmb_clipper_read_get_at(gmb_clipper_state_t* st, int col, int row);
const char* gmb_clipper_read_read_var(gmb_clipper_state_t* st);
void gmb_clipper_read_reset_gets(gmb_clipper_state_t* st);
void gmb_clipper_read_apply_all(gmb_clipper_state_t* st);
void gmb_clipper_read_draw_gets(gmb_clipper_state_t* st);
bool gmb_clipper_read_updated(gmb_clipper_state_t* st);
int  gmb_clipper_read_read_exit(gmb_clipper_state_t* st);
/* Read: loop headless. Ritorna il tasto di uscita (0 = Enter). */
int  gmb_clipper_read_read(gmb_clipper_state_t* st);

/* ---- builder per uso semplice/test ---- */
gmb_get_t* gmb_clipper_read_get_ptr(gmb_clipper_state_t* st, int idx);
int  gmb_clipper_read_add_field(gmb_clipper_state_t* st, int row, int col, const char* value,
                                const char* picture, bool has_range, double lo, double hi);
void gmb_clipper_read_set_field_apply(gmb_clipper_state_t* st, int idx,
                                      void (*apply)(void* ctx, const char* value), void* ctx);
void gmb_clipper_read_set_field_options(gmb_clipper_state_t* st, int idx, int width,
                                        const char* control_type, const char* const* items, int items_count,
                                        double min, double max, double step,
                                        bool has_min, bool has_max, bool has_step);
/* GetBuilder completo: colori / messaggio / callback */
void gmb_clipper_read_set_field_colors(gmb_clipper_state_t* st, int idx,
                                       const char* color, const char* focus_color,
                                       const char* error_color, const char* message);
void gmb_clipper_read_set_field_validator(gmb_clipper_state_t* st, int idx,
                                          bool (*valid)(void* ctx, const char* value), void* ctx);
void gmb_clipper_read_set_field_accept(gmb_clipper_state_t* st, int idx,
                                       bool (*accept)(void* ctx, char ch), void* ctx);
void gmb_clipper_read_set_field_rejected(gmb_clipper_state_t* st, int idx,
                                         void (*rejected)(void* ctx, char ch), void* ctx);
void gmb_clipper_read_set_field_when(gmb_clipper_state_t* st, int idx,
                                     bool (*when)(void* ctx), void* ctx);
/* GetBuilder legacy (specchio GetBuilder.cs) */
int gmb_clipper_get_builder(int row, int col, const char* value, char* out, int cap);

/* ---- ClipperDates (specchio ClipperDates.cs) ---- */
typedef struct gmb_clipper_date {
    int y, m, d;
} gmb_clipper_date_t;

gmb_clipper_date_t gmb_clipper_dates_today(void);
/* Time: "HH:MM:SS" — scrive in out. */
int gmb_clipper_dates_time(char* out, int cap);
int gmb_clipper_dates_seconds(void);
int gmb_clipper_dates_year(int y, int m, int d);
int gmb_clipper_dates_month(int y, int m, int d);
int gmb_clipper_dates_day(int y, int m, int d);
/* Dow: 1=domenica … 7=sabato */
int gmb_clipper_dates_dow(int y, int m, int d);
/* CDow: nome giorno inglese */
int gmb_clipper_dates_cdow(int y, int m, int d, char* out, int cap);
/* CMonth: nome mese inglese */
int gmb_clipper_dates_cmonth(int y, int m, int d, char* out, int cap);
/* Dtoc: data nel formato SET DATE FORMAT (century da SET CENTURY). */
int gmb_clipper_dates_dtoc(gmb_clipper_state_t* st, int y, int m, int d, char* out, int cap);
/* Dtos: "yyyymmdd". */
int gmb_clipper_dates_dtos(int y, int m, int d, char* out, int cap);
/* Ctod: stringa → data nel formato SET DATE FORMAT. Ritorna 1 ok. */
int gmb_clipper_dates_ctod(gmb_clipper_state_t* st, const char* s, gmb_clipper_date_t* out);

/* ---- MouseCapture (stub headless, specchio MouseCapture.cs) ---- */
void gmb_clipper_mouse_enable(void);
void gmb_clipper_mouse_enable_motion(void);
void gmb_clipper_mouse_disable(void);

/* ---- Confun (specchio Confun.cs, parte TTY-free) ---- */
int gmb_confun_center(const char* s, int width, char* out, int cap);
int gmb_confun_right(const char* s, int width, char* out, int cap);
int gmb_confun_left(const char* s, int width, char* out, int cap);
int gmb_confun_clip(const char* s, int width, char* out, int cap);
int gmb_confun_boxed(const char* s, char* out, int cap);
int gmb_confun_bold(const char* s, char* out, int cap);
int gmb_confun_fgrgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap);
int gmb_confun_bgrgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap);
void gmb_confun_print(const char* s);

/* ---- ClipperMemoEdit (specchio ClipperMemoEdit.cs, headless) ---- */
/* MemoEdit: editor multi-riga. text iniziale (righe separate da '\n'),
   ritorna il testo finale in out. Loop headless via typeahead. */
int gmb_clipper_memo_edit(gmb_clipper_state_t* st, const char* text, int row, int col,
                          int width, int height, char* out, int cap);

/* ---- ClipperMouse (specchio ClipperMouse.cs) ---- */
bool gmb_clipper_mouse_is_mouse(gmb_clipper_state_t* st);
int  gmb_clipper_mouse_mrow(gmb_clipper_state_t* st);
int  gmb_clipper_mouse_mcol(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mleft(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mright(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mmiddle(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mleftpress(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mrightpress(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mmiddlepress(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mleftless(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mrightless(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mmiddleless(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mleftshift(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mrightshift(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mmiddleshift(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mleftcontrol(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mrightcontrol(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mmiddlecontrol(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mleftm(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mrightm(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mmiddlem(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_moustat(gmb_clipper_state_t* st, int button, int* row, int* col, int* mods);
void gmb_clipper_mouse_mposition(gmb_clipper_state_t* st, int* row, int* col);   /* set se != NULL */
bool gmb_clipper_mouse_mousecursor(gmb_clipper_state_t* st, int* row, int* col, const char* type);
int  gmb_clipper_mouse_mrelease(gmb_clipper_state_t* st, char* out, int cap);
void gmb_clipper_mouse_update_from_event(gmb_clipper_state_t* st, const gmb_mouse_action_t* ma);
void gmb_clipper_mouse_update_simple(gmb_clipper_state_t* st, int kind, int x, int y);   /* bridge C# */
int  gmb_clipper_mouse_mousex(gmb_clipper_state_t* st);
int  gmb_clipper_mouse_mousey(gmb_clipper_state_t* st);
int  gmb_clipper_mouse_mousebutton(gmb_clipper_state_t* st, int has_id, int button_id);
int  gmb_clipper_mouse_mousewheel(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mouseclick(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mouseclick_area(gmb_clipper_state_t* st, int x1, int y1, int x2, int y2);
double gmb_mouse_click(void);
double gmb_mouse_click_area(int x1, int y1, int x2, int y2);
bool gmb_clipper_mouse_mousedblclick(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mousemove(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mousepresent(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mouseleft(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mouseright(gmb_clipper_state_t* st);
bool gmb_clipper_mouse_mousemiddle(gmb_clipper_state_t* st);

/* ---- ClipperAChoice (specchio ClipperAChoice.cs, headless) ---- */
/* AChoice: menu verticale. Ritorna indice selezionato+1, 0 su ESC.
   items_count>0 richiesto. */
int gmb_clipper_achoice(gmb_clipper_state_t* st, int top, int left, int bottom, int right,
                        const char* const* items, int items_count);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_CLIPPER_H */
