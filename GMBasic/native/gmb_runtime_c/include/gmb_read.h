#ifndef GMB_READ_H
#define GMB_READ_H
/* gmb_read.h — Contesto READ unificato (stadio 1): modale a campi, NON bloccante.
 * Un campo = un controllo CONTROLS text-input + proprietà Clipper (PICTURE/WHEN/VALID/
 * RANGE/MESSAGE/ERROR/COLOR/TYPE). Il loop usa l'UNICO punto di attesa park-based
 * (gmb_key_wait_park), come window/mouse/tastiera/controlli. Sostituisce ClipperRead.Get. */
#include "gmb_runtime.h"
#include "gmb_tui_widgets.h"
#include "gmb_keyboard_extra.h"

#define GMB_READ_MAX_FIELDS 64

typedef struct gmb_read_field {
    gmb_control_t* ctrl;        /* text-input CONTROLS (nel registry) */
    int   form_wid;             /* >=0: campo reso come WIDGET form (grafica) */
    int   kind;                 /* 0=text,1=num,2=spinner,3=date,4=check,5=list */
    double num;                 /* valore per spinner/check/list */
    char  items[512];           /* items per list/combobox ("a|b|c") */
    char  valbuf[64];           /* valore corrente come testo (spinner/check/list) */
    char  type[24];
    gmb_string_t val;           /* valore (widget form: strptr su questo) */
    int   handle;               /* handle del controllo */
    int   row, col, width;      /* posizione (celle) */
    int   numeric;              /* tipo/range attivo */
    int   skip;                 /* WHEN .F. deciso dal backend (IL: non ha un evaluator C) */
    int   visited;              /* 1: campo effettivamente mostrato in questo READ. Un campo
                                 * saltato da WHEN resta 0 e NON torna nel target (la
                                 * variabile conserva il suo valore — semantica Clipper). */
    int   when_addr;            /* >0: predicato WHEN (subroutine) */
    int (*when_fn)(void*);      /* WHEN come funzione C (emessa dall'emitter) */
    void* when_user;
    int (*valid_fn)(void*);     /* VALID come funzione C */
    void* valid_user;
    void (*apply_text)(void*, const char*);  /* write-through del testo nel target (Clpper/xBase) */
    void* apply_text_user;
    int   valid_addr;           /* >0: predicato VALID */
    int   err_active;           /* 1: ultima conferma rifiutata da VALID (mostra ERROR) */
    double range_lo, range_hi; int has_range;
    char  picture[32];
    char  color[16];
    char  message[128], error[128];
    void* apply_target;         /* ByRef/ref variabile: dove scrivere il valore */
    char  initial[256];         /* per re-prompt / annulla */
} gmb_read_field_t;

typedef enum {
    GMB_READ_RUNNING = 0,
    GMB_READ_CONFIRMED = 1,
    GMB_READ_CANCELLED = 2
} gmb_read_state_t;

/* Predicato WHEN/VALID (indirizzo subroutine) — fornito dal backend (VM-C/C99).
 * Per IL/PCode il backend usa il proprio evaluator (pred == NULL → predicato assente). */
typedef int (*gmb_read_pred_fn)(void* user, int addr);

typedef struct gmb_read_ctx {
    int id, n_fields, active, last_key, state;
    gmb_read_field_t fields[GMB_READ_MAX_FIELDS];
    void (*render_hook)(void* self);   /* disegno sul main thread */
    gmb_read_pred_fn pred;
    void* pred_user;
    /* Rendering MESSAGE/ERROR (TUI): label condiviso in fondo + ultimo testo mostrato. */
    gmb_control_t* msg_ctrl;
    char msg_shown[160];
    /* Apply backend-specifico: scrive il valore editato nella variabile del campo. */
    void (*apply)(void* user, gmb_read_field_t* f, const char* text);
    void* apply_user;
} gmb_read_ctx_t;

/* Contesto attivo (lazy): creato al primo @GET, svuotato dopo il READ. */
gmb_read_ctx_t*   gmb_read_active(void);
/* Accoda un campo (ritorna lo slot da riempire col text-input + proprietà). */
gmb_read_field_t* gmb_read_add_field(gmb_read_ctx_t* ctx);
/* @GET: crea il campo = text-input CONTROLS e lo accoda al contesto. */
gmb_read_field_t* gmb_read_add_textfield(gmb_read_ctx_t* ctx, int row, int col, int width, const char* initial);
int  gmb_read_field_count(gmb_read_ctx_t* ctx);
int  gmb_read_add_field_active(int row,int col,int width,const char* initial);
gmb_string_t gmb_read_items_join(const char* a, const char* b);
/* ITEMS da array di stringhe (emitter C99): concatena con '|' i .data degli elementi. */
gmb_string_t gmb_read_items_join_arr(const gmb_string_t* items, int n);
/* Predicati VALID/WHEN come funzioni C (ritorno != 0 = ok). Indice = campo. */
void gmb_read_set_when_fn(int idx, int (*fn)(void*), void* user);
void gmb_read_set_valid_fn(int idx, int (*fn)(void*), void* user);
void gmb_read_set_apply_text(int idx, void (*fn)(void*, const char*), void* user);
/* Entry point per i backend GESTITI (P/Invoke): niente puntatori al ctx. */
int         gmb_read_native_run(void);
/* Ri-prompt VALID: il prossimo run mostra solo il campo idx (skip sugli altri). */
void        gmb_read_native_focus_only(int idx);
const char* gmb_read_native_value(int idx);
void        gmb_read_native_reset(void);
void        gmb_read_native_clear(void);
int  gmb_read_add_field_typed(int row,int col,int width,const char* initial,
                              const char* type, const char* picture, const char* items,
                              double vmin, double vmax, double step);
void gmb_read_set_when(int idx, int addr);
void gmb_read_set_valid(int idx, int addr);
/* Loop READ (park-based): WHEN→skip, VALID→re-focus, ENTER/TAB/ESC; poi ApplyAll. */
int  gmb_read_run(gmb_read_ctx_t* ctx);
/* Processa un key event per il campo attivo (edit + navigazione + VALID). */
void gmb_read_key(gmb_read_ctx_t* ctx, gmb_read_field_t* f, int key);
int  gmb_read_when_ok(gmb_read_ctx_t* ctx, gmb_read_field_t* f);
int  gmb_read_valid_ok(gmb_read_ctx_t* ctx, gmb_read_field_t* f);
void gmb_read_apply_all(gmb_read_ctx_t* ctx);
void gmb_read_reset(gmb_read_ctx_t* ctx);
/* MESSAGE/ERROR Clipper: testo mostrato in fondo (ERROR prevale quando il campo e' stato
 * rifiutato da VALID). idx < 0 -> ultimo campo aggiunto. */
void gmb_read_set_message(int idx, const char* text);
void gmb_read_set_error(int idx, const char* text);
/* Riprende il loop dal campo active (ri-prompt VALID): non azzera i buffer. */
void gmb_read_resume(gmb_read_ctx_t* ctx, int active);
/* WHEN .F. deciso dal backend: il campo non riceve il fuoco (loop lo salta). */
void gmb_read_skip_field(int idx);
/* 1 se il campo idx è stato saltato (GET ... WHEN falso): il chiamante NON deve
 * riassegnare la variabile con il testo del campo (resta invariata). */
int  gmb_read_field_skipped(int idx);
/* Testo corrente del campo (dal text-input CONTROLS). */
const char* gmb_read_field_text(gmb_read_field_t* f);
const char* gmb_read_value_at(int idx);
/* Attesa tasto park-based (unico punto di blocco): mappa Enter/Tab/Esc/frecce. */
int  gmb_read_wait_key(int timeout_ms);
int  gmb_read_wait_key_text(int timeout_ms);

#endif
