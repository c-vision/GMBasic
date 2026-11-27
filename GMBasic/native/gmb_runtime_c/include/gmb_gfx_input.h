/* ============================================================================
 * gmb_gfx_input.h — stato input tastiera/mouse del motore grafico unico.
 *
 * La pump SDL (gmb_gfx_present_sdl.c, SOLO main thread) alimenta questo modulo
 * con gli eventi tradotti; il mondo gestito (bridge C#) li drena via P/Invoke:
 *   - tasti  -> coda FIFO di stringhe UTF-8 (INKEY/WAITKEY/GET/ON KEY)
 *   - mouse  -> coda FIFO di eventi (move/down/up/wheel) + stato corrente
 *   - Ctrl+C -> flag consumabile (la decisione "intercettato o abort" e' del
 *               runtime gestito, che conosce ON KEY)
 *
 * Headless-safe: senza SDL le code restano vuote; l'iniezione (test/CI) e'
 * sempre disponibile e non richiede video.
 * ============================================================================ */
#ifndef GMB_GFX_INPUT_H
#define GMB_GFX_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Tipi evento mouse (parita' con i DispatchMouse del runtime gestito). */
#define GMB_GFX_MOUSE_MOVE  1
#define GMB_GFX_MOUSE_DOWN  2
#define GMB_GFX_MOUSE_UP    3
#define GMB_GFX_MOUSE_WHEEL 4
/* Touch: sintetizzati dalla pump SDL dagli eventi SDL_FINGER* e accodati nella
 * STESSA coda mouse, cosi' i controlli (gmb_form, TUI, windowmanager) li
 * vedono senza modifiche. Semantica: TOUCHDOWN=penna giu' (come DOWN, bottone 1),
 * TOUCHUP=penna su (come UP). Il LONGPRESS e' emesso da gmb_gfx_input_pump_touch()
 * quando il dito resta fermo oltre la soglia: porta a termine il gesto (UP
 * sintetico, cosi' un controllo che aveva "premuto" non resta appeso). */
#define GMB_GFX_TOUCH_DOWN      5
#define GMB_GFX_TOUCH_UP        6
#define GMB_GFX_TOUCH_LONGPRESS 7
/* Movimento del dito: NON e' un mouse-move (i controlli non devono vederlo come
 * trascinamento prima che lo slop decida). Aggiorna solo la posizione nota del
 * gesto — quella che decide lo slop e le coordinate del long-press. Chi
 * normalizza lo vede come GMB_GFX_MOUSE_MOVE. */
#define GMB_GFX_TOUCH_MOVE      8

/* Soglia del long-press in millisecondi (default 500; <=0 la disabilita).
 * Spostamento massimo ammesso per restare "fermo": GMB_GFX_TOUCH_HOLD_SLOP px. */
#define GMB_GFX_TOUCH_HOLD_MS    500
#define GMB_GFX_TOUCH_HOLD_SLOP  12

/* Iniezione (test/CI/headless): accoda un tasto UTF-8 non vuoto. */
void gmb_gfx_input_push_key(const char* key);
/* Iniezione mouse: type = GMB_GFX_MOUSE_*; button = numero SDL (1=L,2=M,3=R);
 * wheel = delta (solo WHEEL). Le coordinate sono PIXEL SCHERMO (gia' mappate). */
void gmb_gfx_input_push_mouse(int type, int x, int y, int button, int wheel);

/* Iniezione touch (pump SDL o test): down/up accodano GMB_GFX_TOUCH_DOWN/UP
 * nella coda mouse e registrano il dito come "in attesa di long-press";
 * move aggiorna solo la posizione (annulla il long-press se si sposta oltre
 * lo slop). now_ms = istante dell'evento (SDL_GetTicks); <=0 = relativo. */
void gmb_gfx_input_push_touch(int down, int x, int y, unsigned int now_ms);

/* Movimento del dito durante un gesto: aggiorna la posizione nota del gesto
 * (slop del long-press e coordinate del punto premuto) e accoda un TOUCH_MOVE.
 * Ritorna 0 se nessun dito e' appoggiato. Con il touch gia' arrivato da
 * gmb_gfx_input_push_touch questa e' la via portante per il drag. */
int  gmb_gfx_input_touch_move(int x, int y);

/* Pompa il long-press: se il dito e' fermo da oltre la soglia accoda
 * GMB_GFX_TOUCH_LONGPRESS (e il TOUCH_UP che chiude il gesto). Da chiamare a
 * ogni frame con l'orologio corrente (main thread, stessa lane della pump SDL).
 * Ritorna 1 se ha emesso qualcosa. */
int gmb_gfx_input_pump_touch(unsigned int now_ms);

/* Soglia long-press in ms (default GMB_GFX_TOUCH_HOLD_MS; <=0 disabilita). */
void gmb_gfx_input_set_touch_hold_ms(int ms);
int  gmb_gfx_input_touch_hold_ms(void);

/* 1 = il dito e' appoggiato (tra TOUCH_DOWN e TOUCH_UP/LONGPRESS). */
int  gmb_gfx_input_touch_down(void);

/* Normalizza touch -> vocabolario mouse per i consumatori che ragionano solo
 * in termini di mouse (form pixel, window manager, campi INPUT):
 *   TOUCH_DOWN -> MOUSE_DOWN, TOUCH_UP -> MOUSE_UP, TOUCH_MOVE -> MOUSE_MOVE,
 *   LONGPRESS -> -1 (va
 *   ignorato: il controllo ha gia' ricevuto la pressione dal TOUCH_DOWN, e
 *   inoltrarlo come MOVE segnerebbe un falso trascinamento).
 * Un tipo gia' mouse (o sconosciuto) viene restituito invariato. */
int  gmb_gfx_input_touch_to_mouse(int type);

/* 1 = un dito ha completato un long-press dall'ultima chiamata (consuma).
 * Il long-press resta un gesto osservabile anche quando i consumatori mouse
 * lo ignorano: l'app (BASIC/host) puo' usarlo per un'azione secondaria. */
int  gmb_gfx_input_take_longpress(void);

/* Poll tasti (main thread): 1 = letto in out (NUL-terminato), 0 = coda vuota. */
int gmb_gfx_input_poll_key(char* out, int maxlen);
/* Poll eventi mouse (main thread): 1 = letto. wheel vale solo per WHEEL. */
int gmb_gfx_input_poll_mouse(int* type, int* x, int* y, int* button, int* wheel);

/* Ctrl+C premuto dall'ultimo poll (consuma il flag). 1 = premuto. */
int gmb_gfx_input_take_ctrl_c(void);
/* Segnala Ctrl+C (usato dalla pump SDL; esposto per i test). */
void gmb_gfx_input_set_ctrl_c(void);

/* Stato fisico tasto per scancode SDL (0..511). 1 = premuto. */
int  gmb_gfx_input_key_down(int scancode);
/* Aggiorna lo stato fisico (pump SDL / test). */
void gmb_gfx_input_set_key_down(int scancode, int down);

/* Ultima posizione mouse nota (pixel schermo) — per eventi WHEEL senza coord. */
void gmb_gfx_input_get_mouse(int* x, int* y);

/* 1 = un tasto del mouse è premuto (per drag/pan del chart). */
int gmb_gfx_input_mouse_down(void);

/* Delta rotella accumulato dall'ultimo poll (consuma): per zoom del chart. */
int gmb_gfx_input_take_wheel(void);

/* Azzera code/stato (present_stop o nuovo SCREEN). */
void gmb_gfx_input_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GFX_INPUT_H */
