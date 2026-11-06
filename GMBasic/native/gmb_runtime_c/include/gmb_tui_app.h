/* ============================================================================
 * gmb_tui_app.h — layer App (specchio ClipperLib/App: BlinkClock.cs,
 * EventLoop.cs, ModalHost.cs, IInputSource.cs, EventDispatcher.cs,
 * ClipperInputSource.cs). Headless/TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_APP_H
#define GMB_TUI_APP_H

#include "gmb_tui.h"
#include "gmb_tui_console.h"
#include "gmb_tui_clipper.h"
#include "gmb_tui_widgets.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_BLINK_MS 400

/* ---- IInputSource (interfaccia) ---- */
typedef struct gmb_input_source {
    void* user;
    int (*read)(void* user, int timeout_ms, gmb_ui_event_t* out);
} gmb_input_source_t;

/* ---- BlinkClock (specchio BlinkClock.cs) ---- */
typedef struct gmb_blink_clock {
    long period_ms;
    long next_flip;
    bool blink_on;
    bool has_flip;
} gmb_blink_clock_t;

void gmb_blink_clock_init(gmb_blink_clock_t* c);
void gmb_blink_clock_reset(gmb_blink_clock_t* c);
bool gmb_blink_clock_tick(gmb_blink_clock_t* c, gmb_clipper_state_t* st);

/* ---- EventLoop (specchio EventLoop.cs) ---- */
typedef struct gmb_event_loop {
    gmb_input_source_t input;
    gmb_blink_clock_t clock;
} gmb_event_loop_t;

void gmb_event_loop_init(gmb_event_loop_t* l, const gmb_input_source_t* input);
int  gmb_event_loop_next(gmb_event_loop_t* l, int timeout_ms, gmb_ui_event_t* out);
bool gmb_event_loop_blink_tick(gmb_event_loop_t* l, gmb_clipper_state_t* st);

/* ---- ModalHost (specchio ModalHost.cs) ---- */
typedef struct gmb_modal_host {
    gmb_input_source_t input;
    gmb_event_loop_t loop;
} gmb_modal_host_t;

void gmb_modal_host_init(gmb_modal_host_t* h, const gmb_input_source_t* input);
/* Loop dell'host corrente, o nuovo loop sull'input di fallback. */
gmb_event_loop_t* gmb_modal_host_current_loop(gmb_event_loop_t* fallback);
/* Esegue una sessione modale: chiama modal(ev) finché non ritorna 0. */
void gmb_modal_host_run_modal(gmb_modal_host_t* h, int (*modal)(const gmb_ui_event_t* ev, void* ctx), void* ctx);

/* ---- ClipperInputSource headless (legge da typeahead/keysource dello stato) ---- */
void gmb_clipper_input_source_init(gmb_input_source_t* src, gmb_clipper_state_t* st);

/* ---- EventDispatcher (specchio EventDispatcher.cs, essenziale) ---- */
#define GMB_DISPATCH_MAX 32

typedef struct gmb_event_dispatcher {
    int key_combos[GMB_DISPATCH_MAX];
    int key_combo_count;
    void (*key_actions[GMB_DISPATCH_MAX])(void* ctx);
    void* key_ctxs[GMB_DISPATCH_MAX];
    int win_kinds[GMB_DISPATCH_MAX];
    /* handle della finestra per la registrazione (ON WINDOW <ev> OF <handle>):
     * -1 = qualunque finestra (forma senza OF, comportamento storico). */
    int win_handles[GMB_DISPATCH_MAX];
    void (*win_actions[GMB_DISPATCH_MAX])(void* ctx);
    void* win_ctxs[GMB_DISPATCH_MAX];
    int win_count;
    int mouse_kinds[GMB_DISPATCH_MAX];
    void (*mouse_actions[GMB_DISPATCH_MAX])(void* ctx, int x, int y);
    void* mouse_ctxs[GMB_DISPATCH_MAX];
    int mouse_count;
} gmb_event_dispatcher_t;

void gmb_event_dispatcher_init(gmb_event_dispatcher_t* d);
gmb_event_dispatcher_t* gmb_event_dispatcher_new(void);
void gmb_event_dispatcher_register_key(gmb_event_dispatcher_t* d, int combo,
                                       void (*fn)(void* ctx), void* ctx);
void gmb_event_dispatcher_register_window(gmb_event_dispatcher_t* d, int kind,
                                          void (*fn)(void* ctx), void* ctx);
/* Variante con HANDLE (ON WINDOW <ev> OF <h>): handle=-1 = qualunque finestra. */
void gmb_event_dispatcher_register_window_h(gmb_event_dispatcher_t* d, int kind, int handle,
                                            void (*fn)(void* ctx), void* ctx);
void gmb_event_dispatcher_register_mouse(gmb_event_dispatcher_t* d, int kind,
                                         void (*fn)(void* ctx, int x, int y), void* ctx);
int  gmb_event_dispatcher_dispatch_key(gmb_event_dispatcher_t* d, int combo);
void gmb_event_dispatcher_dispatch_window(gmb_event_dispatcher_t* d, int kind);
/* Variante con HANDLE: invoca le registrazioni per (kind, handle) e quelle wildcard (-1). */
void gmb_event_dispatcher_dispatch_window_h(gmb_event_dispatcher_t* d, int kind, int handle);
void gmb_event_dispatcher_dispatch_mouse(gmb_event_dispatcher_t* d, int kind, int x, int y);
/* has / unregister (rispecchiano HasKeyHandler/Unregister* del lato C#) */
int  gmb_event_dispatcher_has_key(gmb_event_dispatcher_t* d, int combo);
void gmb_event_dispatcher_unregister_key(gmb_event_dispatcher_t* d, int combo);
void gmb_event_dispatcher_unregister_window(gmb_event_dispatcher_t* d, int kind);
/* Variante con HANDLE: rimuove solo le registrazioni di quella finestra (-1 = wildcard). */
void gmb_event_dispatcher_unregister_window_h(gmb_event_dispatcher_t* d, int kind, int handle);
void gmb_event_dispatcher_unregister_mouse(gmb_event_dispatcher_t* d, int kind);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_APP_H */

/* ---- IApp (specchio IApp.cs, interfaccia) ---- */
typedef struct gmb_app {
    void* user;
    const char* (*name)(void* user);
    bool (*close_requested)(void* user);
    void (*caret)(void* user, int* x, int* y);
    bool (*cursor_visible)(void* user);
    void (*resize)(void* user, int cols, int rows);
    void (*handle_event)(void* user, const gmb_ui_event_t* ev);
    void (*draw)(void* user, gmb_screen_buffer_t* buffer);
} gmb_app_t;

/* ---- AppRunner (specchio AppRunner.cs) ---- */
#define GMB_APP_RUNNER_MAX_KEYS 32
typedef struct gmb_app_runner {
    int key_codes[GMB_APP_RUNNER_MAX_KEYS];
    void (*key_handlers[GMB_APP_RUNNER_MAX_KEYS])(void* app_ctx);
    int key_count;
    void (*mouse_handler)(const gmb_mouse_action_t* action, void* app_ctx);
    void* mouse_app_ctx;
    void (*global_handler)(const gmb_ui_event_t* ev, void* app_ctx);
    void* global_app_ctx;
    void (*resize_handler)(int w, int h, void* app_ctx);
    void* resize_app_ctx;
    void (*render)(gmb_screen_buffer_t* sb, void* ctx);
    void* render_ctx;
} gmb_app_runner_t;

void gmb_app_runner_init(gmb_app_runner_t* r);
gmb_app_runner_t* gmb_app_runner_on_key(gmb_app_runner_t* r, int key, void (*fn)(void* app_ctx));
gmb_app_runner_t* gmb_app_runner_on_mouse(gmb_app_runner_t* r, void (*fn)(const gmb_mouse_action_t* action, void* app_ctx));
gmb_app_runner_t* gmb_app_runner_on_resize(gmb_app_runner_t* r, void (*fn)(int w, int h, void* app_ctx));
gmb_app_runner_t* gmb_app_runner_on_event(gmb_app_runner_t* r, void (*fn)(const gmb_ui_event_t* ev, void* app_ctx));
void gmb_app_runner_set_render(gmb_app_runner_t* r, void (*fn)(gmb_screen_buffer_t* sb, void* ctx), void* ctx);
/* Esegue il loop dell'app: legge da `input` finché app.close_requested(). */
void gmb_app_runner_run(gmb_app_runner_t* r, const gmb_app_t* app, gmb_input_source_t* input,
                        gmb_clipper_state_t* st, int width, int height);

/* ---- Control modal interattivo (C99 emitter, test, host) ---- */
/* Esegue un controllo (gmb_control_t*) in un loop modale con input reale:
 * disegna ogni frame, dispatcha i tasti, rende a stdout. Typeahead iniettata
 * (gmb_tui_inject_keys) prioritaria, poi tty reale. Ritorna il tasto di uscita
 * (GMB_K_ENTER o GMB_K_ESC). */
int  gmb_tui_run_control_modal(gmb_control_t* ctrl);
/* Modello NON-bloccante (mirror pixel gmb_form_*): registry + frame che ritorna. */
void gmb_tui_controls_reset(void);
/* Canale tasti host: la frame cell accoda i tasti non consumati dal controllo focalizzato. */
void gmb_tui_host_key_push(int k);
int  gmb_tui_host_key_pop(void);
int  gmb_tui_control_add(gmb_control_t* c);
int  gmb_tui_control_count(void);
gmb_control_t* gmb_tui_control_at(int i);
long long gmb_tui_control_selrow(int i);   /* riga selezionata di un controllo TBROWSE, -1 altrimenti */
/* Testo (CodeArea/TextField) del controllo registrato all'indice i. Ritorna len. */
int gmb_tui_control_text(int i, char* out, int cap);
/* Valore/selezione generica (TAB: indice tab attivo). -1 se n/d. */
long long gmb_tui_control_value(int i);
/* Errore di lint (squiggle) su una riga del CodeArea registrato. */
void gmb_tui_control_error(int i, int line, int on);
void gmb_tui_control_clear_errors(int i);
/* FFI-friendly (BASIC): ritorni `const char*` su buffer statico; setter per indice. */
const char* gmb_tui_control_text_ptr(int i);                 /* CodeArea: righe '\n'-unite; ListBox: selezionato */
void gmb_tui_control_set_text(int i, const char* text);      /* split su '\n' -> righe CodeArea / voci ListBox */
void gmb_tui_control_set_errors(int i, const char* lines_1based); /* squiggle: righe 1-based, sep '|' o ';' */
long long gmb_code_area_goto_line(int i, int line);               /* riga cursore CodeArea (0-based), -1 se non CodeArea */
long long gmb_tui_code_cursor_line(int i);                        /* riga cursore CodeArea (0-based), -1 se non CodeArea */
long long gmb_tui_code_line_count(int i);                         /* numero righe CodeArea, -1 se non CodeArea */
long long gmb_tui_code_insert(int i, const char* text);           /* inserisce testo alla posizione del cursore */
const char* gmb_tui_code_line_text(int i, int n);                 /* testo della riga n (0-based) */
long long gmb_tui_code_delete_line(int i, int n);                 /* cancella la riga n */
long long gmb_tui_code_find(int i, const char* needle, int from); /* prima riga >= from contenente needle, o -1 */
long long gmb_tui_code_replace(int i, const char* find, const char* repl); /* sostituisci tutte, ritorna il conteggio */
int  gmb_tui_control_cursor(int i, int* row, int* col);      /* cella (0-based) del cursore del CodeArea, 1 se ok */
gmb_control_t* gmb_tui_control_find_by_name(const char* name);
/* Origine assoluta del CONTENUTO di un controllo ospite (nome), risolvendo ricorsivamente il
 * suo contenitore (window per nome, oppure un altro controllo/UserControl per nome). */
int  gmb_tui_control_content_origin(const char* name, int* out_x, int* out_y);
int  gmb_tui_controls_frame(int frame_ms);
/* Tasto dalla finestra grafica (SDL) → i controlli TUI lo consumano nel frame. */
void gmb_tui_push_gfx_key(int k);
/* Drain dei tasti della finestra grafica nel dispatch controlli (senza disegnare). */
void gmb_tui_process_pending_keys(void);
void gmb_tui_process_pending_mouse(void);
/* Mouse in PIXEL LOGICI dal form: dispatch ai controlli TUI (converte in celle). */
void gmb_tui_dispatch_mouse_px(int type, int x, int y, int button, int wheel);
/* FFI-friendly (BASIC): dimensione del buffer di rendering + ripristino cursore. */
int  gmb_tui_screen_width(void);
int  gmb_tui_screen_height(void);
void gmb_tui_show_cursor(void);
/* Focus del controllo (indice nel registry): imposta/legge. */
void gmb_tui_control_focus(int i);
long long gmb_tui_control_focused(void);
/* App SDL: SdlConsole a CELLE. L'app (che nel C99Native gira sul MAIN THREAD) possiede la
 * console: start (crea+mostra) -> pump (eventi+tasti) -> controls_frame (disegno) -> present. */
void gmb_tui_sdl_start(int grid_w, int grid_h);
int  gmb_tui_sdl_pump(void);
void gmb_tui_sdl_present(void);
void gmb_tui_sdl_cursor_solid(void);   /* cursore solido (alla pressione di un tasto) */
void gmb_tui_sdl_stop(void);
int  gmb_tui_sdl_active(void);
long long gmb_tui_now_ms(void);   /* tempo corrente ms (monotono) per time-out nel BASIC */
/* Coda eventi MOUSE verso l'host: pop() ritorna il kind (0=move,1=down,2=up) o -1; poi x/y/ctrl. */
void gmb_tui_host_mouse_push(int kind, int x, int y, int ctrl);
long long gmb_tui_host_mouse_pop(void);
long long gmb_tui_host_mouse_x(void);
long long gmb_tui_host_mouse_y(void);
long long gmb_tui_host_mouse_ctrl(void);
/* Menu TV: riempi (spec "Top:sub|sub;Top2:sub|sub") e leggi la voce attivata (top*100+sub, -1). */
void      gmb_tui_menu_set(int idx, const char* spec);
long long gmb_tui_menu_poll(void);
int       gmb_tui_menu_any_open(void);   /* 1 se un menu dropdown e' aperto (cursore nascosto) */
void      gmb_tui_menu_close_all(void);  /* chiude tutti i menu (click fuori dal menu, TV) */
int       gmb_tui_menu_covers(int x, int y); /* 1 se un menu aperto copre la cella (x,y) */
int       gmb_tui_cursor_covered(void);  /* 1 se il caret e' coperto da una window/overlay sopra */
int       gmb_tui_sdl_cursor_visible(void); /* stato cursore nativo console (test headless) */
int       gmb_tui_sdl_cursor_row(void);      /* riga cursore nativo (0-based) */
int       gmb_tui_sdl_cursor_col(void);      /* colonna cursore nativo (0-based) */
void gmb_tui_draw_px_all(void);
void gmb_tui_px_invalidate(void);
/* Eventi controllo TUI (base): per handle. */
void gmb_tui_set_event_id(int handle, int ev, gmb_tui_cb cb);
void gmb_tui_fire(int handle, int ev);
/* §11.7-3: cabla un metodo-evento di una classe UserControl (thunk + istanza) sul firing. */
void gmb_tui_bind_usercontrol_event(int handle, int ev, void (*fn)(void*), void* inst);
double gmb_tui_clicked(int handle);
double gmb_tui_changed(int handle);
double gmb_tui_gotfocused(int handle);
double gmb_tui_lostfocused(int handle);
int    gmb_tui_last_handle(void);
int    gmb_tui_firing_control(void);
int    gmb_tui_firing_event(void);
/* Pump unificato (pixel o TUI secondo SCREEN) + factory controlli TUI nel registry. */
void gmb_ui_frame(int ms);
/* INTO del BASIC per i controlli TUI: valore sincronizzato ad ogni frame (out=NULL sgancia). */
void gmb_tui_bind_value(int handle, double* out);
/* INTO stringa: lega il testo selezionato del controllo (COMBOBOX/LISTBOX/RADIO/MENU) e lo
 * sincronizza subito (il valore è leggibile già alla creazione, come su IL). */
void gmb_tui_bind_str(int handle, gmb_string_t* out);
gmb_control_t* gmb_tui_add_button(int x, int y, int w, const char* label);
gmb_control_t* gmb_tui_add_checkbox(int x, int y, int w, const char* label);
gmb_control_t* gmb_tui_add_listbox(int x, int y, int w, int h, const char* items_csv);
gmb_control_t* gmb_tui_add_toggle(int x, int y, int w, const char* caption);
gmb_control_t* gmb_tui_add_combobox(int x, int y, int w, const char* items_csv);
gmb_control_t* gmb_tui_add_radiogroup(int x, int y, int w, int h, const char* items_csv);
gmb_control_t* gmb_tui_add_tab(int x, int y, int w, const char* items_csv);
gmb_control_t* gmb_tui_add_menu(int x, int y, int w, const char* items_csv);
gmb_control_t* gmb_tui_add_spinner(int x, int y, int w, const char* preset);
gmb_control_t* gmb_tui_add_slider(int x, int y, int w, int mn, int mx, int val);
gmb_control_t* gmb_tui_add_numericupdown(int x, int y, int w, int mn, int mx, int val);
gmb_control_t* gmb_tui_add_starrating(int x, int y, int w, int mx, int val);
gmb_control_t* gmb_tui_add_scrollbar(int x, int y, int len, int mn, int mx, int val);
gmb_control_t* gmb_tui_add_scrollview(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_label(int x, int y, int w, const char* text);
gmb_control_t* gmb_tui_add_textinput(int x, int y, int w, const char* initial);
gmb_control_t* gmb_tui_add_datalist(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_propertygrid(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_logview(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_hexview(int x, int y, int w, int h, const char* data);
gmb_control_t* gmb_tui_add_bigtext(int x, int y, const char* text);
gmb_control_t* gmb_tui_add_codearea(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_reorderlistbox(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_alertbox(int x, int y, int w, int h, const char* text);
gmb_control_t* gmb_tui_add_confirmationbox(int x, int y, int w, int h, const char* text);
gmb_control_t* gmb_tui_add_fileexplorer(int x, int y, int w, int h, const char* path);
gmb_control_t* gmb_tui_add_dashboard(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_splitter(int x, int y, int w, int h, int orient);
gmb_control_t* gmb_tui_add_contextmenu(int x, int y, int w, int h, const char* csv);
gmb_control_t* gmb_tui_add_statusbar(int x, int y, int w, const char* text);
gmb_control_t* gmb_tui_add_datepicker(int x, int y, int w);
gmb_control_t* gmb_tui_add_colorpicker(int x, int y, int w, int h);
gmb_control_t* gmb_tui_add_grid(int x, int y, int w, int h, int cols, const char* data_csv);
gmb_control_t* gmb_tui_add_treeview(int x, int y, int w, int h, const char* items_csv);
gmb_control_t* gmb_tui_add_filedialog(const char* title, const char* mask);
gmb_control_t* gmb_tui_add_printerdialog(const char* title);
gmb_control_t* gmb_tui_add_tbrowse(int row, int col, int w, int h, const char* cols_csv, const char* data_csv);
/* Solo-grafica (famiglia unica gmb_control_t; no-op in TEXT). */
gmb_control_t* gmb_tui_add_canvas(int x, int y, int w, int h);
gmb_control_t* gmb_tui_add_image(int x, int y, int w, int h, const char* text);
gmb_control_t* gmb_tui_add_imagebutton(int x, int y, int w, int h, const char* text);
gmb_control_t* gmb_tui_add_qr(int x, int y, int w, int h, const char* text);
gmb_control_t* gmb_tui_add_richtext(int x, int y, int w, int h, const char* text);
gmb_control_t* gmb_tui_add_halfblock(int x, int y, int w, int h, const char* text);
gmb_control_t* gmb_tui_add_tiling(int x, int y, int w, int h, const char* text);
/* FunctionKeyBar (riga tasti funzione): keys = "1Help|2Save|..." (10 max, vuoto = default). */
gmb_control_t* gmb_tui_add_fkeybar(int x, int y, int w, const char* keys);
/* Candlestick (candele OHLC): csv = "o,h,l,c;..." (fino a GMB_CANDLE_MAX, vuoto = demo). */
gmb_control_t* gmb_tui_add_candlestick(int x, int y, int w, int h, const char* csv);
/* Chart (linee): csv = "v1;v2;..." (oppure "x,y;x,y;..."; vuoto = demo). Controllo a celle,
 * motore autonomo gmb_chart; reso a celle in TEXT e a pixel in GRAPHICS. */
gmb_control_t* gmb_tui_add_chart(int x, int y, int w, int h, const char* csv);
void gmb_tui_control_register(gmb_control_t* c);
void gmb_tui_inject_keys(const char* s);
/* Stato Clipper globale (typeahead iniettata con gmb_tui_inject_keys/SENDKEY). */
gmb_clipper_state_t* gmb_tui_global_state(void);
/* Buffer modale condiviso (NULL se mai creato): per SAVE/RESTORE SCREEN headless. */
gmb_screen_buffer_t* gmb_tui_modal_buffer(void);
int gmb_tui_global_typeahead_read(void);
void gmb_tui_global_typeahead_clear(void);
