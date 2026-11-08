/* ============================================================================
 * gmb_form.h — Form/event-loop di controllo GPM-safe (C99Native).
 *
 * Modello (regola GPM, AGENTS §0ter):
 *  - La SUB BASIC che contiene il loop gira come TASK GPM su un worker. Il loop
 *    chiama gmb_form_poll() (NON bloccante) e poi SLEEP/YIELD (park-based) per
 *    cedere il worker. NIENTE busy-spin, niente gmb_sleep_ms nel loop.
 *  - gmb_form_poll() legge le code input gia' tradotte (gmb_gfx_input.*): il
 *    worker e' l'UNICO lettore (lock-free 1-scrittore main/1-lettore worker).
 *    Il worker NON tocca SDL (vietato: SDL solo sul main thread).
 *  - Le callback dei bottoni sono SUB BASIC invocate INLINE sul worker (che e'
 *    un task GPM): MAI dal thread SDL/present. Stesso canone di ON KEY.
 *  - gmb_form_closed() usa il flag di chiusura della finestra (SDL) o un flag
 *    interno in headless.
 * ========================================================================== */
#ifndef GMB_FORM_H
#define GMB_FORM_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Handler di un bottone: SUB BASIC emessa come `gmb_double_t gmb_user_<Nome>(void)`.
 * Il valore di ritorno e' ignorato al fire (come ON KEY). */
typedef gmb_double_t (*gmb_form_cb)(void);

/* INTO stringa dei controlli TUI (COMBOBOX/LISTBOX/RADIO/MENU): lega il testo selezionato.
 * Usata dal codice generato quando l'INTO del BASIC è una variabile stringa. */
void gmb_tui_bind_str(int handle, gmb_string_t* out);

/* Kind pubblici dei widget pixel (stesso ordine dell'enum interna W_* in gmb_form.c). */
enum { GMB_PX_CHECK = 1, GMB_PX_RADIO, GMB_PX_TRACK, GMB_PX_DROPDOWN, GMB_PX_PROGRESS,
       GMB_PX_TOGGLE, GMB_PX_SPINNER, GMB_PX_STATUSBAR, GMB_PX_TABS, GMB_PX_LISTBOX,
       GMB_PX_COLORPICKER, GMB_PX_DATE, GMB_PX_GRID, GMB_PX_FILE, GMB_PX_TREE, GMB_PX_MENUBAR, GMB_PX_TEXT,
       GMB_PX_CANVAS, GMB_PX_IMAGE, GMB_PX_IMAGEBUTTON, GMB_PX_QR, GMB_PX_RICHTEXT, GMB_PX_HALFBLOCK, GMB_PX_TILING, GMB_PX_PANEL };
/* Render pixel ricco di un widget "virtuale" (bridge per i draw_px_fn dei controlli). */
void gmb_px_draw_ex(int wkind, int x, int y, int w, int h, const char* label,
                    const char* const* items, int nitems, double value, double vmin, double vmax,
                    const double* var, int first, const unsigned* colors, int ncolors);

/* ---- Control BASE (come VB): evento -> callback ---- */
/* Eventi modello VB6: comuni a tutti i controlli + specifici per tipo. */
enum {
    /* comuni (VB6) */
    GMB_EV_CLICK = 1, GMB_EV_DBLCLICK, GMB_EV_CHANGE, GMB_EV_GOTFOCUS, GMB_EV_LOSTFOCUS,
    GMB_EV_MOUSEDOWN, GMB_EV_MOUSEUP, GMB_EV_MOUSEMOVE, GMB_EV_KEYDOWN, GMB_EV_KEYPRESS, GMB_EV_KEYUP,
    GMB_EV_DRAGDROP, GMB_EV_DRAGOVER, GMB_EV_VALIDATE,
    /* specifici */
    GMB_EV_SCROLL, GMB_EV_DROPDOWN, GMB_EV_UPCLICK, GMB_EV_DOWNCLICK,
    GMB_EV_NODECLICK, GMB_EV_EXPAND, GMB_EV_COLLAPSE, GMB_EV_SELCHANGE, GMB_EV_ROWCOLCHANGE, GMB_EV_WHEN,
    /* finestra (VB6 Form) */
    GMB_EV_LOAD, GMB_EV_UNLOAD, GMB_EV_ACTIVATE, GMB_EV_DEACTIVATE, GMB_EV_QUERYUNLOAD, GMB_EV_RESIZE, GMB_EV_PAINT,
    GMB_EV_COUNT
};
/* Registra una callback per un evento sull'ULTIMO controllo aggiunto. */
void gmb_form_set_event(int ev, gmb_form_cb cb);
/* Nome evento -> indice GMB_EV_*. */
int  gmb_form_ev_index(const char* name);
/* Registra (cb!=NULL) o rimuove (cb==NULL, STOP) un evento su un controllo per HANDLE. */
void gmb_form_set_event_id(int handle, int ev, gmb_form_cb cb);

/* INPUT unico della base Control: polla il mouse e converte SEMPRE in coordinate
 * LOGICHE (le rect sono logiche). Da usare in ogni loop (form e dialog modali). */
int gmb_form_poll_mouse(int* type, int* x, int* y, int* button, int* wheel);
void gmb_form_dispatch_mouse_px(int type, int x, int y, int button, int wheel);
void gmb_form_dispatch_key(const char* key);
void gmb_form_focus_widget(int i);
int  gmb_form_focused_widget(void);

/* ---- WindowBase (come la Form di VB): base di TUTTE le finestre/dialog ---- */
typedef struct gmb_form_win {
    char caption[128];
    int x, y, w, h;
    int visible, enabled, modal, resizable;
    int result;
    gmb_form_cb ev[GMB_EV_COUNT];
} gmb_form_win_t;

void gmb_form_win_init(gmb_form_win_t* w);
void gmb_form_win_on(gmb_form_win_t* w, int ev, gmb_form_cb cb);
/* Loop comune della finestra: draw(ctx) ogni frame; evcb(ev,x,y,ctx) per input;
 * frame_ms = park; ritorna quando close_requested(). Fira Load/Activate/Paint/Unload. */
void gmb_form_win_run(gmb_form_win_t* w,
                      void (*draw)(void* ctx),
                      int (*evcb)(gmb_form_win_t* w, int ev, int x, int y, void* ctx),
                      void* ctx, int frame_ms);

/* Azzera bottoni e stato (nuovo form / nuovo SCREEN). */
void gmb_form_reset(void);

/* Registra un bottone. Coordinate LOGICHE (come gmb_gfx_*); l'hit-test le mappa
 * in pixel nativi con gmb_gfx_render_scale(). Ritorna l'indice o -1. */
int  gmb_form_add_button(int x, int y, int w, int h, const char* label, gmb_form_cb cb);
/* Factory POLIMORFICA (un verbo, dispatch per console): TEXT -> registry TUI,
 * GRAPHICS -> registry form. Ritorna l'handle unificato (TUI=indice, form=1000+i). */
int  gmb_control_add_button(int x, int y, int w, int h, const char* label, gmb_form_cb cb);

/* Come add_button ma applica uno stile CSS-like (override dei default del tema):
 *   "rounded radius=8 shadow=3,3 emboss gradient=#2a2a2a,#101010 glow=#6cf"
 *   "flat border=1,#888 bg=#c83c3c fg=white font=Tahoma,16 bevel=sunken"
 * colore: #RRGGBB | #AARRGGBB | white/black/red/green/blue/gray/none. */
int  gmb_form_add_button_css(int x, int y, int w, int h, const char* label, gmb_form_cb cb, const char* spec);

/* Widget pixel (stesso gmb_widget_style_t/tema). Il valore vive in un double* del
 * programma (INTO var). Interattivi via mouse in gmb_form_poll. */
int  gmb_form_add_checkbox(int x, int y, const char* label, double* var);
/* Factory POLIMORFICA CHECKBOX (TEXT -> registry TUI + bind INTO, GRAPHICS -> form). */
int  gmb_control_add_checkbox(int x, int y, int w, const char* label, double* var);
int  gmb_control_add_toggle(int x, int y, int w, const char* label, double* var);
int  gmb_control_add_radio(int x, int y, int w, const char* items, double* var);
int  gmb_control_add_listbox(int x, int y, int w, int h, const char* items, double* var);
int  gmb_control_add_combobox(int x, int y, int w, const char* items, double* var);
int  gmb_control_add_numericupdown(int x, int y, int w, double mn, double mx, double step, double* var);
int  gmb_control_add_starrating(int x, int y, int w, int mx, int val, double* var);
int  gmb_control_add_scrollbar(int x, int y, int len, int mn, int mx, int val, double* var);
int  gmb_control_add_grid(int x, int y, int w, int h, const char* headers, const char* data, double* var);
int  gmb_control_add_tree(int x, int y, int w, int h, const char* items, double* var);
int  gmb_control_add_colorpicker(int x, int y, int w, const char* palette, double* var);
int  gmb_control_add_tab(int x, int y, int w, const char* items, double* var);
int  gmb_form_add_radio(int x, int y, const char* items, double* var);       /* items "a|b|c" */
int  gmb_form_add_track(int x, int y, int w, double vmin, double vmax, double* var);
int  gmb_form_add_dropdown(int x, int y, int w, const char* items, double* var);
/* Factory PIXEL senza dispatch polimorfico (test headless) + caption del widget. */
int  gmb_form_add_widget_px(int kind, int x, int y, int w, int h, const char* items, double* var);
int  gmb_form_add_checkbox_px(int x, int y, const char* label, double* var);
const char* gmb_form_widget_caption(int i);
int  gmb_form_add_progress(int x, int y, int w, const double* var);          /* display 0..100 */
int  gmb_form_add_progress_v(int x, int y, int w, double value);            /* idem, con valore iniziale */
int  gmb_form_add_canvas(int x, int y, int w, int h);                        /* superficie disegno */
int  gmb_form_add_image(int x, int y, int w, int h, const char* label);      /* immagine (placeholder) */
int  gmb_form_add_imagebutton(int x, int y, int w, int h, const char* label);/* bottone con icona */
int  gmb_form_add_qr(int x, int y, int w, int h, const char* text);          /* codice QR */
int  gmb_form_add_richtext(int x, int y, int w, int h, const char* text);    /* testo ricco (righe "|") */
int  gmb_form_add_halfblock(int x, int y, int w, int h, const char* label);  /* blocchi */
int  gmb_form_add_tiling(int x, int y, int w, int h, const char* label);
int  gmb_form_add_panel(int x, int y, int w, int h, const char* title);     /* pattern piastrelle */
int  gmb_form_add_toggle(int x, int y, const char* label, double* var);      /* switch on/off */
int  gmb_form_add_spinner(int x, int y, int w, double vmin, double vmax, double step, double* var);
int  gmb_form_add_statusbar(int x, int y, int w, const char* text);          /* barra testo */
int  gmb_form_add_tabs(int x, int y, const char* items, double* var);        /* tab index */
int  gmb_form_add_listbox(int x, int y, int w, int h, const char* items, double* var);  /* select idx */
int  gmb_form_add_colorpicker(int x, int y, const char* palette, double* var); /* palette "RRGGBB,..." */
int  gmb_form_add_datepicker(int x, int y, void* strptr, const char* fmt);   /* fmt es. "dd/mm/yyyy" */
int  gmb_form_add_grid(int x, int y, int w, int h, const char* headers, const char* data, double* var); /* headers "H1|H2", data "a|b;c|d" */
int  gmb_form_add_filedialog(int x, int y, int w, int h, const char* dir, void* strptr);   /* elenco file della cartella */
int  gmb_form_add_tree(int x, int y, int w, int h, const char* items, double* var);       /* items "a/b|c/d" */
int  gmb_form_add_menubar(int x, int y, const char* items, double* var);                  /* barra menu */
int  gmb_form_add_textbox(int x, int y, int w, int h, void* strptr);     /* campo testo (caret/editing) */
int  gmb_form_printerdialog(void* pstr, double* copies, double* duplex);                  /* dialogo stampa */

/* 1 = finestra chiusa (SDL_QUIT/X) o chiusura richiesta. */
int  gmb_form_closed(void);
/* MODALE (livello sopra la pagina: velo + ombra + input catturato). */
struct gmb_control;
void gmb_form_modal_open(struct gmb_control* c);
void gmb_form_modal_close(void);
int  gmb_form_modal_active(void);
struct gmb_control* gmb_form_modal_control(void);
/* Pull-eventi (gambe gestite): 1 = letto (ev, indice widget), 0 = coda vuota. */
int  gmb_form_poll_event(int* ev, int* widx);
/* Richiede la chiusura (la callback puo' chiamarla per terminare il loop). */
void gmb_form_request_close(void);

/* Step NON bloccante: drena l'input disponibile e dispatcha le callback (inline
 * sul task chiamante). NON disegna: vedi gmb_form_render_if_dirty(). */
void gmb_form_poll(void);

/* Render dei bottoni sulla pagina grafica: SOLO MAIN THREAD, dal present, e solo
 * se lo stato e' cambiato (dirty). Cosi' il goroutine eventi non fa repaint e a
 * riposo non c'e' alcun ridisegno. */
void gmb_form_render_if_dirty(void);

/* Esclusività del render: mentre è attivo (es. loop READ del form Clipper che disegna
 * da sé), gmb_form_render_if_dirty() non tocca la pagina (non fa CLS). */
void gmb_form_set_exclusive(int on);
/* Applica uno stile CSS-like (override colori) all'ULTIMO widget/controllo aggiunto. */
void gmb_form_set_last_style(const char* spec);
/* Applica una spec CSS-like (colori + proprietà base + `metric`) a un controllo per HANDLE,
 * in QUALSIASI registry (form widget/button, poi TUI). Unico punto per gli emitter: le
 * clausole del controllo non possono più andare perse a seconda del tipo. */
void gmb_form_apply_style_handle(int handle, const char* spec);
/* Nota l'handle dell'ultimo controllo creato (form o TUI): gli emitter applicano le
 * clausole dichiarate (NAME/STYLE/metric/…) a QUEST'ultimo, qualunque sia il registry. */
void gmb_form_note_handle(int handle);
/* Applica le clausole all'ULTIMO controllo creato, nel registry corretto (form o TUI). */
void gmb_form_apply_last_style(const char* spec);

/* Proprieta' base (VB) del controllo: impostabili anche nella spec di STYLE
 * ("name=Ok tag=t visible=1 enabled=1 tabindex=2 tabstop=1 focus"). Getter per indice. */
const char* gmb_form_widget_name(int i);
const char* gmb_form_widget_tooltip(int i);
const char* gmb_form_widget_tag(int i);
int gmb_form_widget_visible(int i);
int gmb_form_widget_enabled(int i);
int gmb_form_widget_tabindex(int i);
int gmb_form_widget_tabstop(int i);
/* Come sopra, per il Control BUTTON (stesso modello base). */
const char* gmb_form_button_name(int i);
const char* gmb_form_button_tooltip(int i);
int gmb_form_button_visible(int i);
int gmb_form_button_enabled(int i);
int gmb_form_button_tabindex(int i);

/* ---- Metodi a runtime per NOME (VB): controllano widget e Control-bottone ---- */
int  gmb_form_control_find(const char* name);                 /* handle: i, o 1000+i (button), -1 */
void gmb_form_control_set(const char* name, const char* prop, double num, const char* str);
double gmb_form_control_get(const char* name, const char* prop);
const char* gmb_form_control_get_str(const char* name, const char* prop);
void gmb_form_control_move(const char* name, int x, int y, int w, int h);   /* w/h <=0 = invariato */
void gmb_form_control_focus(const char* name);                /* SetFocus */
void gmb_form_control_zorder(const char* name, int front);    /* 1 = porta davanti, 0 = dietro */
void gmb_form_control_invalidate(const char* name);           /* §11.7: marca da ridisegnare */
void gmb_form_control_refresh(const char* name);
/* Chip pixel virtuale con lo stile del TEMA (riusa draw_button_body). */
void gmb_form_draw_chip(int x, int y, int w, int h, const char* label, int state);

/* ID handle del controllo appena creato (widget = indice, bottone = 1000+indice). */
int  gmb_form_last_handle(void);
void gmb_form_set_last_handle(int h);
void gmb_form_set_last_value(double v);

/* Polling di stato (NON consuma; l'ultimo evento resta finche' non ne arriva un altro). */
double gmb_form_clicked(int handle);
double gmb_form_dblclicked(int handle);
double gmb_form_changed(int handle);
double gmb_form_gotfocused(int handle);
double gmb_form_lostfocused(int handle);
double gmb_mouse_clicked(void);
double gmb_mouse_dblclicked(void);
double gmb_mouse_pressed(void);
double gmb_key_pressed(void);
/* Contesto "evento in corso" (per il thunk managed) + innalzamento programmatico. */
int  gmb_form_firing_control(void);
int  gmb_form_firing_event(void);
void gmb_form_fire_event(int handle, int ev);
/* Aggiorna solo lo stato last_event (senza invocare alcun handler). */
void gmb_form_set_last_event(int handle, int ev);

/* Un frame del loop: gmb_form_poll() + PARK GPM fino a frame_ms. Dentro un task
 * usa gmb_yield_ticks (park cooperativo, NIENTE blocco del worker, NIENTE spin);
 * fuori da un task (main thread) ripiega su gmb_sleep_ms. Questa e' la primitiva
 * da usare nel loop eventi. */
void gmb_form_frame(int frame_ms);

/* Dialog grafico modale (ALERT/CONFIRM in grafica): pannello tematizzato + bottoni
 * stilizzati (stesso stile dei BUTTON; `spec` = override opzionale CSS-like).
 * Ritorna l'indice del bottone premuto (0..2) o -1 (ESC/chiusura). */
int  gmb_form_dialog(const char* title, const char* msg,
                     const char* b0, const char* b1, const char* b2, const char* spec,
                     const char* icon);

/* Message box generico: bottoni separati da '|' (max 3), `spec` = stile (CSS-like).
 * Ritorna l'indice del bottone premuto o -1. */
int  gmb_form_msgbox(const char* title, const char* msg, const char* buttons, const char* spec);

/* Input box grafico: finestra con campo di testo (editing) + OK/Annulla. Ritorna il
 * testo (gmb_string_t) confermato, o `def` se annullato. */
gmb_string_t gmb_form_inputbox(const char* title, const char* prompt, const char* def, const char* spec);

int  gmb_form_button_count(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FORM_H */
