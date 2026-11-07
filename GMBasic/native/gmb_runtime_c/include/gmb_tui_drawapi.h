/* gmb_tui_drawapi.h — API di DISEGNO dei controlli per i client C# (P/Invoke).
 *
 * UNICA FONTE del disegno: il modello + il render dei controlli vivono nel runtime C
 * (gmb_tui_*.c). Il client C# mantiene solo lo stato e chiama `gmb_tui_draw_control`
 * passando il proprio buffer (`gmb_screen_buffer_t*`, lo stesso allocato da
 * `gmb_screen_new`). Nessuna copia della logica di disegno in C#.
 *
 * Direzione unica: C# -> C. Il runtime C non conosce il client.
 */
#ifndef GMB_TUI_DRAWAPI_H
#define GMB_TUI_DRAWAPI_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kind disegnabili (stabile: NON riordinare, il client passa questi valori). */
enum {
    GMB_DK_NONE = 0,
    GMB_DK_BUTTON = 1,
    GMB_DK_CHECKBOX,
    GMB_DK_TOGGLE,
    GMB_DK_LABEL,
    GMB_DK_TEXTINPUT,
    GMB_DK_LISTBOX,
    GMB_DK_COMBOBOX,
    GMB_DK_RADIOGROUP,
    GMB_DK_PROGRESS,
    GMB_DK_SCROLLBAR,
    GMB_DK_TAB,
    GMB_DK_PANEL,
    GMB_DK_SPINNER,
    GMB_DK_SLIDER,
    GMB_DK_STATUSBAR,
    GMB_DK_MENU,
    GMB_DK_TEXTAREA,
    GMB_DK_CODEAREA,
    GMB_DK_ALERTBOX,
    GMB_DK_BIGTEXT,
    GMB_DK_NUMERIC,
    GMB_DK_HLINE,
    GMB_DK_VLINE,
    GMB_DK_STARRATING,
    GMB_DK_FILEEXPLORER,
    GMB_DK_FKEYBAR,
    GMB_DK_SPLITTER,
    GMB_DK_CONTEXTMENU,
    GMB_DK_SCROLLVIEW,
    GMB_DK_WINDOW,
    GMB_DK_TBROWSE
};

/* Bit di `state`. */
enum {
    GMB_DS_FOCUSED   = 1 << 0,
    GMB_DS_ERROR     = 1 << 1,
    GMB_DS_UPPERCASE = 1 << 2,
    GMB_DS_CHECKED   = 1 << 3,   /* checkbox/toggle: selezionato */
    GMB_DS_OPEN      = 1 << 4,   /* combobox/menu: popup aperto */
    GMB_DS_READONLY  = 1 << 5,   /* textinput: sola lettura */
    GMB_DS_PASSWORD  = 1 << 6    /* textinput: maschera i caratteri */
};

/* Richiesta di disegno. Tutti i campi sono opzionali tranne kind/x/y/w/h. */
typedef struct gmb_draw_req {
    int kind;            /* GMB_DK_* */
    int x, y, w, h;      /* rect in CELLE del buffer */
    int state;           /* maschera GMB_DS_* */
    int sel;             /* indice selezionato (liste/tab/menu) */
    int min, max;        /* range (slider/progress/scrollbar/numeric) */
    double value;        /* valore corrente (progress/slider/numeric/star) */
    const char* text;    /* etichetta/testo; liste = voci separate da '|'; aree = righe '\n' */
    const char* extra;   /* dato secondario: titolo panel/menu, path explorer, tasti fkeybar */
    int has_fg, has_bg;  /* 1 = applica i colori sotto */
    unsigned fg_rgb, bg_rgb;  /* 0xRRGGBB */
} gmb_draw_req_t;

/* Disegna UN controllo nel buffer. Ritorna 1 se disegnato, 0 se kind ignoto/parametri invalidi. */
int gmb_tui_draw_control(gmb_screen_buffer_t* buf, const gmb_draw_req_t* req);

/* Come sopra, ma con buffer opaco (handle IntPtr lato C#). */
int gmb_tui_draw_control_h(void* buf, const gmb_draw_req_t* req);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_DRAWAPI_H */
